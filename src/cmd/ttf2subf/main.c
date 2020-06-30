#include <u.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/ftglyph.h>

#include <libc.h>
#include <draw.h>
#include <memdraw.h>

FT_Library lib;

#define FLOOR(x) ((x) & -64)
#define CEIL(x) (((x) + 63) & -64)
#define TRUNC(x) ((x) >> 6)
#define ROUND(x) (((x) + 32) & -64)

enum {
	Rmono = 1,
	Rantialias,
	Rsubpixel,
};

enum {
	Srgb = 1,
	Sbgr,
	Svrgb,
	Svbgr
};

typedef struct {
	int left;
	int right;
	int top;
	int bottom;
	int advance;
	int width;
} FTmetrics;

typedef struct _FTsubfont {
	int nchars;
	int startc;
	int ascent;
	int descent;
	int image_height;
	int image_width;
	FTmetrics* metrics;
	Fontchar* fchars;
	Subfont sf;
	Memimage* image;
	struct _FTsubfont* next;
	struct _FTsubfont* prev;
} FTsubfont;

typedef struct {
	char* name;
	int size;
	FT_Face face;
	FT_GlyphSlot slot;
	int rmode;
	int rgb;
	int image_channels;
	int ascent;
	int descent;
	int height;
	FTsubfont* sfont;
} FTfont;

int f[3] = { 1, 2, 3 };
int fsum = 9;

FT_Vector getScale(FTfont* font) {
	FT_Vector ret;

	ret.x = ret.y = 1;
	if (font->rmode == Rsubpixel) {
		switch (font->rgb) {
			case Srgb:
			case Sbgr:
				ret.x = 3;
				break;

			case Svrgb:
			case Svbgr:
				ret.y = 3;
				break;
		}
	}

	return ret;
}

FT_Glyph getGlyph(FTfont* font, int c) {
	int status;
	FT_Glyph glyph;
	FT_UInt gidx;
	FT_Int32 lflags;
	FT_Vector scale;
	FT_Vector pos;
	FT_Matrix tm;

	gidx = FT_Get_Char_Index(font->face, c);

	switch (font->rmode) {
		case Rmono:
		case Rsubpixel:
		default:
			lflags = FT_LOAD_TARGET_MONO;
			break;

		case Rantialias:
			lflags = FT_LOAD_DEFAULT;
			break;
	}

	status = FT_Load_Glyph(font->face, gidx, lflags);
	if (status) {
		print("FT_Load_Glyph: status=%d\n", status);
		exits("error: load glyph");
	}

	status = FT_Get_Glyph(font->face->glyph, &glyph);
	if (status) {
		print("FT_Get_Glyph: status=%d\n", status);
		exits("error: get glyph");
	}

	glyph->advance.x = font->slot->advance.x;
	glyph->advance.y = font->slot->advance.y;

	scale = getScale(font);
	tm.xx = 0x10000 * scale.x;
	tm.yy = 0x10000 * scale.y;
	tm.xy = tm.yx = 0;
	pos.x = pos.y = 0;

	status = FT_Glyph_Transform(glyph, &tm, &pos);
	if (status) {
		print("FT_Glyph_Transform: status=%d\n", status);
		exits("error: glyph transform");
	}

	return glyph;
}

FT_BitmapGlyph getBitmapGlyph(FTfont* font, FT_Glyph glyph) {
	int rmode = 0;
	int status;

	switch (font->rmode) {
		case Rmono:
			rmode = ft_render_mode_mono;
			break;

		case Rantialias:
		case Rsubpixel:
			rmode = ft_render_mode_normal;
			break;

		default:
			exits("error: rmode != antialias|mono|subpixel");
	}

	status = FT_Glyph_To_Bitmap(&glyph, rmode, 0, 1);
	if (status) {
		print("FT_Glyph_To_Bitmap: status=%d\n", status);
		exits("error: glyph to bitmap");
	}

	return (FT_BitmapGlyph) glyph;
}
	
FTmetrics getMetrics(FTfont* font, int c) {
	FT_Glyph glyph;
	FT_BitmapGlyph bglyph;
	FT_Bitmap bitmap;
	FT_Vector scale;
	FTmetrics ret;

	glyph = getGlyph(font, c);
	bglyph = getBitmapGlyph(font, glyph);
	scale = getScale(font);
	bitmap = bglyph->bitmap;

	ret.left =  bglyph->left / scale.x;
	ret.right = (bglyph->left + bitmap.width) / scale.x;
	ret.top = bglyph->top / scale.y;
	ret.bottom = (bglyph->top - bitmap.rows) / scale.y;

	ret.width = bitmap.width / scale.x;
	ret.advance = TRUNC(font->slot->advance.x); /* usage of slot ??? */

	if (font->rmode == Rsubpixel) {
		switch (font->rgb) {
			case Srgb:
			case Sbgr:
				ret.left -= 1;
				ret.right += 1;
				ret.width += 2;

				break;

			case Svrgb:
			case Svbgr:
				ret.top -= 1;
				ret.bottom -= 1;
				break;
		}
	}
			
/*	FT_Done_Glyph(glyph); */

	return ret;
}

unsigned long getPixelMono(FTfont* f, FT_Bitmap* bitmap, int x, int y) {
	int g = ((bitmap->buffer[bitmap->pitch*y + x/8]) >> (7 - x%8) & 1) * 255;
	return (g << 24) | (g << 16) | (g << 8) | 0xFF;
}

unsigned long getPixelAntialias(FTfont* f, FT_Bitmap* bitmap, int x, int y) {
	int g = bitmap->buffer[bitmap->pitch*y + x];
	return (g << 24) | (g << 16) | (g << 8) | 0xFF;
}

unsigned long getPixelSubpixel(FTfont* font, FT_Bitmap* bitmap, int x, int y) {
	int xs = 0, ys = 0;
	int af = 0, bf = 0, cf = 0;
	int xx, yy;
	int h[7];
	int a, b, c;
	FT_Vector scale;
	int i;

	scale = getScale(font);

	switch (font->rgb) {
		case Srgb:
		case Sbgr:
			xs = 1;
			ys = 0;
			break;

		case Svrgb:
		case Svbgr:
			xs = 0;
			ys = 1;
			break;

	}

	switch (font->rgb) {
		case Srgb:
		case Svrgb:
			af = 24;
			bf = 16;
			cf = 8;
			break;

		case Sbgr:
		case Svbgr:
			af = 8;
			bf = 16;
			cf = 24;
			break;
	}

	xx = x * scale.x;
	yy = y * scale.y;

	for(i = -2; i < 5; i++) {
		int xc, yc;

		xc = xx + (i - scale.x) * xs;
		yc = yy + (i - scale.y) * ys;

		if (xc<0 || xc >= bitmap->width || yc<0 || yc>=bitmap->rows) {
			h[i + 2] = 0;
		} else {
			h[i + 2] = bitmap->buffer[yc * bitmap->pitch + xc] * 65536;
		}
	}

	a = (h[0]*f[0] + h[1]*f[1] + h[2]*f[2] + h[3]*f[1] + h[4]*f[0]) / fsum;
	b = (h[1]*f[0] + h[2]*f[1] + h[3]*f[2] + h[4]*f[1] + h[5]*f[0]) / fsum;
	c = (h[2]*f[0] + h[3]*f[1] + h[4]*f[2] + h[5]*f[1] + h[6]*f[0]) / fsum;

	return ((a / 65536) << af) | ((b / 65536) << bf) | ((c / 65536) << cf) | 0xFF;
}

typedef unsigned long (*getpixelfunc)(FTfont*, FT_Bitmap*, int, int);

getpixelfunc getPixelFunc(FTfont* font) {
	getpixelfunc ret = 0;

	switch (font->rmode) {
		case Rmono:
			ret = getPixelMono;
			break;

		case Rantialias:
			ret = getPixelAntialias;
			break;

		case Rsubpixel:
			ret = getPixelSubpixel;
			break;
	}

	return ret;
}

void drawChar(FTfont* font, Memimage* img, int x0, int y0, int c) {
	Rectangle r;
	Memimage* color;
	FT_Glyph glyph;
	FT_BitmapGlyph bglyph;
	FT_Bitmap bitmap;
	int x, y;
	getpixelfunc getPixel;
	FT_Vector scale;
	int height, width;

	glyph = getGlyph(font, c);
	scale = getScale(font);
	bglyph = getBitmapGlyph(font, glyph);
	bitmap = bglyph->bitmap;

	getPixel = getPixelFunc(font);

	r.min.x = 0;
	r.min.y = 0;
	r.max.x = 1;
	r.max.y = 1;
	color = allocmemimage(r, font->image_channels);

	r.min.x = r.min.y = 0;
	r.max.x = bitmap.width;
	r.max.y = bitmap.rows;

	width = bitmap.width / scale.x;
	height = bitmap.rows / scale.y;

	if (font->rmode == Rsubpixel) {
		switch (font->rgb) {
			case Srgb:
			case Sbgr:
				width += 2;
				break;

			case Svrgb:
			case Svbgr:
				height += 2;
				break;
		}
	}
	
	for(y = 0; y < height; y++) {
		for(x = 0; x < width; x++) {
			Point pt;
			unsigned long cl = getPixel(font, &bitmap, x, y);

			pt.x = x + x0;
			pt.y = y + y0;
			memfillcolor(color, cl);			
			memimagedraw(img, rectaddpt(Rect(0, 0, 1, 1), pt), 
				color, ZP, nil, ZP, SatopD);
		}
	}

	freememimage(color);
}

void initSubfont(FTfont* font, FTsubfont* sf, int startc) {
	int i;
	int width;
	int ascent, descent;
	FTmetrics rc[257];

	// get first defined character
	while (startc < 65536) {
		if (startc == 0 || FT_Get_Char_Index(font->face, startc) != 0) {
			break;
		}

		startc++;
	}

	ascent = 0; descent = 100000;
	width = 0;
	for(i = 0; i < sizeof(rc)/sizeof(rc[0]); i++) {
		if (startc+i >= 65536) {
			break;
		}


		if (width > 2000) {
			break;
		}

		if (i > 0 && FT_Get_Char_Index(font->face, startc + i) == 0) {
			rc[i] = getMetrics(font, 0);
			rc[i].width = 0;
			rc[i].advance = 0;
		} else
			rc[i] = getMetrics(font, startc + i);

			
		if (ascent < rc[i].top) {
			ascent = rc[i].top;
		}

		if (descent > rc[i].bottom) {
			descent = rc[i].bottom;
		}

		width += rc[i].width; // +1?

	}

	sf->startc = startc;
	sf->nchars = i;
	sf->ascent = ascent;
	sf->descent = descent;
	sf->image_width = width;
	sf->metrics = malloc(i * sizeof(FTmetrics));
	sf->fchars = malloc((i + 1) * sizeof(Fontchar));

	for(i = 0; i < sf->nchars; i++) {
		sf->metrics[i] = rc[i];
	}	
}

int createSubfont(FTfont* font, FTsubfont* sf, char* fname) {
	int i;
	Rectangle r;
	int x;
	int fd;

	sf->image_height = sf->ascent - sf->descent;
/*	print("creating %d: (%d, %d)\n", sf->startc, sf->image_height, sf->image_width); */


	r = Rect(0, 0, sf->image_width, font->height /*sf->image_height*/);
	if (sf->image_width <= 0 || sf->image_height <= 0) {
		return 1;
	}

	sf->image = allocmemimage(r, font->image_channels);
	memfillcolor(sf->image, 0x000000FF);

	x = 0;
	for(i = 0; i < sf->nchars; i++) {
		sf->fchars[i].x = x;
		sf->fchars[i].width = sf->metrics[i].advance;
		sf->fchars[i].left = sf->metrics[i].left;
		sf->fchars[i].bottom = /*sf->ascent*/ font->ascent - sf->metrics[i].bottom;
		sf->fchars[i].top = (font->ascent - sf->metrics[i].top) > 0 ? font->ascent - sf->metrics[i].top : 0;
		x += sf->metrics[i].width; // +1?

		drawChar(font, sf->image, sf->fchars[i].x, sf->fchars[i].top, i + sf->startc);
	}

	sf->fchars[i].x = x;

	sf->sf.name = font->name;
	sf->sf.n = sf->nchars;
	sf->sf.height = font->height; /*sf->image_height; */
	sf->sf.ascent = font->ascent /*sf->ascent */;
	sf->sf.info = sf->fchars;
	sf->sf.ref = 1;

	fd = create(fname, OWRITE, 0666);
	if(fd < 0)
		sysfatal("can not create font file: %r");

	writememimage(fd, sf->image);
	writesubfont(fd, &sf->sf);
	close(fd);
	freememimage(sf->image);
	sf->image = nil;

	return 0;
}

void
usage(char *s)
{
	fprint(2, "\nusage: %s <options>\n", s);
	fprint(2, "\nwhere:\n", s);
	fprint(2, 
	"	-s size						- point size\n"
	"	-h 							- this message\n"
	"	-f fname					- ttf file name\n"
	"	-n name						- font name\n"
	"	-m mono|antialias|subpixel	- rendering mode\n"
	"	-r rgb|bgr|vrgb|vbgr		- LCD display type\n\n");
	exits("usage");
}

void main(int argc, char* argv[]) {
	FTfont font;

	int status;
	int i;
	FTsubfont* sf;
	int fd;
	char buf[256];

	char* fname = nil;
	char* mode = nil;
	char* srgb = nil;

	char *progname = argv[0];

	font.name = nil;
	font.size = 0;
	
	if(argc == 1)
		usage(progname);

	ARGBEGIN {
		case 's':
			font.size = atoi(ARGF());
			break;
		case 'h':
			usage(progname);
			break;	/* never reached */
		case 'f':
			fname = ARGF();
			break;
		case 'n':
			font.name = ARGF();
			break;

		case 'm':
			mode = ARGF();
			break;

		case 'r':
			srgb = ARGF();
			break;

		default:
			print("bad flag: %c", ARGC());
	} ARGEND

	if (fname == nil) {
		print("no font name specified\n");
		exits("fname");
	}

	if (font.size == 0) {
		print("no font size specified\n");
		exits("size");
	}

	if (mode != nil) {
		if (strcmp(mode, "mono") == 0) {
			font.rmode = Rmono;
		} else if (strcmp(mode, "antialias") == 0) {
			font.rmode = Rantialias;
		} else if (strcmp(mode, "subpixel") == 0) {
			font.rmode = Rsubpixel;
		}
	} else {
		font.rmode = Rmono;
	}

	switch (font.rmode) {
		case Rmono:
			font.image_channels = GREY1;
			break;

		case Rantialias:
			font.image_channels = GREY8;
			break;

		case Rsubpixel:
			font.image_channels = RGB24;
			if (strcmp(srgb, "rgb") == 0) {
				font.rgb = Srgb;
			} else if (strcmp(srgb, "bgr") == 0) {
				font.rgb = Sbgr;
			} else if (strcmp(srgb, "vrgb") == 0) {
				font.rgb = Svrgb;
			} else if (strcmp(srgb, "vbgr") == 0) {
				font.rgb = Svbgr;
			} else {
				print("unknown subpixel mode: %s\n", srgb);
				exits("rgb");
			}
			break;
	}

	if (font.name == nil) {
		font.name = fname;
	}

	print("font file: %s, font name: %s, size: %d\n", fname, font.name, font.size);
	memimageinit();
	status = FT_Init_FreeType(&lib);
	if (status) {
		print("FT_Init_FreeType: status=%d\n", status);
		return;
	}

	FT_New_Face(lib, fname, 0, &font.face);
	if (status) {
		print("FT_New_Face: status=%d\n", status);
		exits("FT_New_Face");
	}

	FT_Set_Char_Size(font.face, 0, font.size * 64, 72, 72);
	if (status) {
		print("FT_Set_Char_Size: status=%d\n", status);
		exits("FR_Set_Char_Size");
	}

	FT_Select_Charmap(font.face, ft_encoding_unicode);

	font.slot = font.face->glyph;
	font.sfont = nil;
	font.ascent = 0;
	font.descent = 10000;

	i = 0;
	while (i < 65536) {
		struct _FTsubfont* sf = malloc(sizeof(FTsubfont));

		initSubfont(&font, sf, i);

		if (sf->nchars == 0) {
			break;
		}

		print("last: %d, start: %d, nchars: %d, width: %d\n", i, sf->startc, sf->nchars, sf->image_width);

		i = sf->startc + sf->nchars;
		sf->next = font.sfont;
		font.sfont = sf;

		if (font.ascent < sf->ascent) {
			font.ascent = sf->ascent;
		}

		if (font.descent > sf->descent) {
			font.descent = sf->descent;
		}
	}

	/*	font.height = font.ascent - font.descent; */
	font.height = font.size;
	font.ascent = font.height-3;

	/* do we have a directory created for this font? */
	snprint(buf, sizeof(buf), "%s", font.name);
	if(access(buf, AEXIST) == -1) {
		fd = create(buf, OREAD, DMDIR|0777);
		if(fd < 0) 
			sysfatal("cannot create font directory: %r");
	}

	snprint(buf, sizeof(buf), "%s/%s.%d.font", font.name, font.name, font.size);
	fd = create(buf, OWRITE, 0666);
	if(fd < 0)
		sysfatal("cannot create .font file: %r");
		
	fprint(fd, "%d\t%d\n", font.height, font.ascent);

	for(sf = font.sfont; sf != nil; sf = sf->next) {

		snprint(buf, sizeof(buf), "%s/%s.%d.%04x", font.name, font.name, font.size, sf->startc);
//		printf("generating 0x%04x - 0x%04x\n", sf->startc, sf->startc + sf->nchars);

		if (0 ==createSubfont(&font, sf, buf)) {
			snprint(buf, sizeof(buf), "%s.%d.%04x", font.name, font.size, sf->startc);
			fprint(fd, "0x%04x\t0x%04x\t%s\n", sf->startc, sf->startc+sf->nchars - 1, buf);
		}

	}
	close(fd);
}
