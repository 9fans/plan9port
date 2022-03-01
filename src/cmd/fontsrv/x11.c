#include <u.h>

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include "a.h"

static FcConfig    *fc;
static FT_Library  lib;
static int         dpi = 96;

void
loadfonts(void)
{
	int i;
	FT_Error e;
	FcFontSet *sysfonts;

	if(!FcInit() || (fc=FcInitLoadConfigAndFonts()) == NULL) {
		fprint(2, "fontconfig initialization failed\n");
		exits("fontconfig failed");
	}

	e = FT_Init_FreeType(&lib);
	if(e) {
		fprint(2, "freetype initialization failed: %d\n", e);
		exits("freetype failed");
	}

	sysfonts = FcConfigGetFonts(fc, FcSetSystem);

	xfont = emalloc9p(sysfonts->nfont*sizeof xfont[0]);
	memset(xfont, 0, sysfonts->nfont*sizeof xfont[0]);
	for(i=0; i<sysfonts->nfont; i++) {
		FcChar8 *fullname, *fontfile;
		int index;
		FcPattern *pat = sysfonts->fonts[i];

		if(FcPatternGetString(pat, FC_POSTSCRIPT_NAME, 0, &fullname) != FcResultMatch ||
		   FcPatternGetString(pat, FC_FILE, 0, &fontfile) != FcResultMatch     ||
		   FcPatternGetInteger(pat, FC_INDEX, 0, &index) != FcResultMatch)
			continue;

		xfont[nxfont].name     = strdup((char*)fullname);
		xfont[nxfont].fontfile = strdup((char*)fontfile);
		xfont[nxfont].index    = index;
		nxfont++;
	}

	FcFontSetDestroy(sysfonts);
}

void
load(XFont *f)
{
	FT_Face face;
	FT_Error e;
	FT_ULong charcode;
	FT_UInt glyph_index;
	int i;

	if(f->loaded)
		return;

	e = FT_New_Face(lib, f->fontfile, f->index, &face);
	if(e){
		fprint(2, "load failed for %s (%s) index:%d\n", f->name, f->fontfile, f->index);
		return;
	}
	if(!FT_IS_SCALABLE(face)) {
		fprint(2, "%s is a non scalable font, skipping\n", f->name);
		FT_Done_Face(face);
		f->loaded = 1;
		return;
	}
	f->unit = face->units_per_EM;
	f->height = (int)((face->ascender - face->descender) * 1.35);
	f->originy = face->descender * 1.35; // bbox.yMin (or descender)  is negative, because the baseline is y-coord 0

	for(charcode=FT_Get_First_Char(face, &glyph_index); glyph_index != 0;
		charcode=FT_Get_Next_Char(face, charcode, &glyph_index)) {

		int idx = charcode/SubfontSize;

		if(charcode > Runemax)
			break;

		if(!f->range[idx])
			f->range[idx] = 1;
	}
	FT_Done_Face(face);

	// libdraw expects U+0000 to be present
	if(!f->range[0])
		f->range[0] = 1;

	// fix up file list
	for(i=0; i<nelem(f->range); i++)
		if(f->range[i])
			f->file[f->nfile++] = i;

	f->loaded = 1;
}

Memsubfont*
mksubfont(XFont *xf, char *name, int lo, int hi, int size, int antialias)
{
	FT_Face face;
	FT_Error e;
	Memimage *m, *mc, *m1;
	double pixel_size;
	int w, x, y, y0;
	int i;
	Fontchar *fc, *fc0;
	Memsubfont *sf;
	//Point rect_points[4];

	e = FT_New_Face(lib, xf->fontfile, xf->index, &face);
	if(e){
		fprint(2, "load failed for %s (%s) index:%d\n", xf->name, xf->fontfile, xf->index);
		return nil;
	}

	e = FT_Set_Char_Size(face, 0, size<<6, dpi, dpi);
	if(e){
		fprint(2, "FT_Set_Char_Size failed\n");
		FT_Done_Face(face);
		return nil;
	}

	pixel_size = (dpi*size)/72.0;
	w = x = (int)((face->max_advance_width) * pixel_size/xf->unit + 0.99999999);
	y = (int)((face->ascender - face->descender) * pixel_size/xf->unit + 0.99999999);
	y0 = (int)(-face->descender * pixel_size/xf->unit + 0.99999999);

	m = allocmemimage(Rect(0, 0, x*(hi+1-lo)+1, y+1), antialias ? GREY8 : GREY1);
	if(m == nil) {
		FT_Done_Face(face);
		return nil;
	}
	mc = allocmemimage(Rect(0, 0, x+1, y+1), antialias ? GREY8 : GREY1);
	if(mc == nil) {
		freememimage(m);
		FT_Done_Face(face);
		return nil;
	}
	memfillcolor(m, DBlack);
	memfillcolor(mc, DBlack);
	fc = malloc((hi+2 - lo) * sizeof fc[0]);
	sf = malloc(sizeof *sf);
	if(fc == nil || sf == nil) {
		freememimage(m);
		freememimage(mc);
		free(fc);
		free(sf);
		FT_Done_Face(face);
		return nil;
	}
	fc0 = fc;

	//rect_points[0] = mc->r.min;
	//rect_points[1] = Pt(mc->r.max.x, mc->r.min.y);
	//rect_points[2] = mc->r.max;
	//rect_points[3] = Pt(mc->r.min.x, mc->r.max.y);

	x = 0;
	for(i=lo; i<=hi; i++, fc++) {
		int k, r;
		int advance;

		memfillcolor(mc, DBlack);

		fc->x = x;
		fc->top = 0;
		fc->bottom = Dy(m->r);
		e = 1;
		k = FT_Get_Char_Index(face, i);
		if(k != 0) {
			e = FT_Load_Glyph(face, k, FT_LOAD_RENDER|FT_LOAD_NO_AUTOHINT|(antialias ? 0:FT_LOAD_TARGET_MONO));
		}
		if(e || face->glyph->advance.x <= 0) {
			fc->width = 0;
			fc->left = 0;
			if(i == 0) {
				drawpjw(m, fc, x, w, y, y - y0);
				x += fc->width;
			}
			continue;
		}

		FT_Bitmap *bitmap = &face->glyph->bitmap;
		uchar *base = byteaddr(mc, mc->r.min);
		advance = (face->glyph->advance.x+32) >> 6;

		for(r=0; r < bitmap->rows; r++)
			memmove(base + r*mc->width*sizeof(u32int), bitmap->buffer + r*bitmap->pitch, bitmap->pitch);

		memimagedraw(m, Rect(x, 0, x + advance, y), mc,
			Pt(-face->glyph->bitmap_left, -(y - y0 - face->glyph->bitmap_top)),
			memopaque, ZP, S);

		fc->width = advance;
		fc->left = 0;
		x += advance;

#ifdef DEBUG_FT_BITMAP
		for(r=0; r < bitmap->rows; r++) {
			int c;
			uchar *span = bitmap->buffer+(r*bitmap->pitch);
			for(c = 0; c < bitmap->width; c++) {
				fprint(1, "%02x", span[c]);
			}
			fprint(1,"\n");
		}
#endif

#ifdef DEBUG_9_BITMAP
		for(r=0; r < mc->r.max.y; r++) {
			int c;
			uchar *span = base+(r*mc->width*sizeof(u32int));
			for(c = 0; c < Dx(mc->r); c++) {
				fprint(1, "%02x", span[c]);
			}
			fprint(1,"\n");
		}
#endif
	}
	fc->x = x;

	// round up to 32-bit boundary
	// so that in-memory data is same
	// layout as in-file data.
	if(x == 0)
		x = 1;
	if(y == 0)
		y = 1;
	if(antialias)
		x += -x & 3;
	else
		x += -x & 31;
	m1 = allocmemimage(Rect(0, 0, x, y), antialias ? GREY8 : GREY1);
	memimagedraw(m1, m1->r, m, m->r.min, memopaque, ZP, S);
	freememimage(m);
	freememimage(mc);

	sf->name = nil;
	sf->n = hi+1 - lo;
	sf->height = Dy(m1->r);
	sf->ascent = Dy(m1->r) - y0;
	sf->info = fc0;
	sf->bits = m1;

	FT_Done_Face(face);
	return sf;
}
