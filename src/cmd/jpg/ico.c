#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include <event.h>
#include <cursor.h>

typedef struct Icon Icon;
struct Icon
{
	Icon	*next;

	uchar	w;		/* icon width */
	uchar	h;		/* icon height */
	ushort	ncolor;		/* number of colors */
	ushort	nplane;		/* number of bit planes */
	ushort	bits;		/* bits per pixel */
	ulong	len;		/* length of data */
	ulong	offset;		/* file offset to data */

	Image	*img;
	Image	*mask;

	Rectangle r;		/* relative */
	Rectangle sr;		/* abs */
};

typedef struct Header Header;
struct Header
{
	uint	n;
	Icon	*first;
	Icon	*last;
};

int debug;
Mouse mouse;
Header h;
Image *background;

ushort
gets(uchar *p)
{
	return p[0] | (p[1]<<8);
}

ulong
getl(uchar *p)
{
	return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
}

int
Bgetheader(Biobuf *b, Header *h)
{
	Icon *icon;
	int i;
	uchar buf[40];

	memset(h, 0, sizeof(*h));
	if(Bread(b, buf, 6) != 6)
		goto eof;
	if(gets(&buf[0]) != 0)
		goto header;
	if(gets(&buf[2]) != 1)
		goto header;
	h->n = gets(&buf[4]);

	for(i = 0; i < h->n; i++){
		icon = mallocz(sizeof(*icon), 1);
		if(icon == nil)
			sysfatal("malloc: %r");
		if(Bread(b, buf, 16) != 16)
			goto eof;
		icon->w = buf[0];
		icon->h = buf[1];
		icon->ncolor = buf[2] == 0 ? 256 : buf[2];
		if(buf[3] != 0)
			goto header;
		icon->nplane = gets(&buf[4]);
		icon->bits = gets(&buf[6]);
		icon->len = getl(&buf[8]);
		icon->offset = getl(&buf[12]);

		if(i == 0)
			h->first = icon;
		else
			h->last->next = icon;
		h->last = icon;
	}
	return 0;

eof:
	werrstr("unexpected EOF");
	return -1;
header:
	werrstr("unknown header format");
	return -1;
}

uchar*
transcmap(Icon *icon, uchar *map)
{
	uchar *m, *p;
	int i;

	p = m = malloc(sizeof(int)*(1<<icon->bits));
	for(i = 0; i < icon->ncolor; i++){
		*p++ = rgb2cmap(map[2], map[1], map[0]);
		map += 4;
	}
	return m;
}

Image*
xor2img(Icon *icon, uchar *xor, uchar *map)
{
	uchar *data;
	Image *img;
	int inxlen;
	uchar *from, *to;
	int s, byte, mask;
	int x, y;

	inxlen = 4*((icon->bits*icon->w+31)/32);
	to = data = malloc(icon->w*icon->h);

	/* rotate around the y axis, go to 8 bits, and convert color */
	mask = (1<<icon->bits)-1;
	for(y = 0; y < icon->h; y++){
		s = -1;
		byte = 0;
		from = xor + (icon->h - 1 - y)*inxlen;
		for(x = 0; x < icon->w; x++){
			if(s < 0){
				byte = *from++;
				s = 8-icon->bits;
			}
			*to++ = map[(byte>>s) & mask];
			s -= icon->bits;
		}
	}

	/* stick in an image */
	img = allocimage(display, Rect(0,0,icon->w,icon->h), CMAP8, 0, DNofill);
	loadimage(img, Rect(0,0,icon->w,icon->h), data, icon->h*icon->w);

	free(data);
	return img;
}

Image*
and2img(Icon *icon, uchar *and)
{
	uchar *data;
	Image *img;
	int inxlen;
	int outxlen;
	uchar *from, *to;
	int x, y;

	inxlen = 4*((icon->w+31)/32);
	to = data = malloc(inxlen*icon->h);

	/* rotate around the y axis and invert bits */
	outxlen = (icon->w+7)/8;
	for(y = 0; y < icon->h; y++){
		from = and + (icon->h - 1 - y)*inxlen;
		for(x = 0; x < outxlen; x++){
			*to++ = ~(*from++);
		}
	}

	/* stick in an image */
	img = allocimage(display, Rect(0,0,icon->w,icon->h), GREY1, 0, DNofill);
	loadimage(img, Rect(0,0,icon->w,icon->h), data, icon->h*outxlen);

	free(data);
	return img;
}

int
Bgeticon(Biobuf *b, Icon *icon)
{
	ulong l;
	ushort s;
	uchar *xor;
	uchar *and;
	uchar *cm;
	uchar *buf;
	uchar *map2map;
	Image *img;

	Bseek(b, icon->offset, 0);
	buf = malloc(icon->len);
	if(buf == nil)
		return -1;
	if(Bread(b, buf, icon->len) != icon->len){
		werrstr("unexpected EOF");
		return -1;
	}

	/* this header's info takes precedence over previous one */
	if(getl(buf) != 40){
		werrstr("bad icon header");
		return -1;
	}
	l = getl(buf+4);
	if(l != icon->w)
		icon->w = l;
	l = getl(buf+8);
	if(l>>1 != icon->h)
		icon->h = l>>1;
	s = gets(buf+12);
	if(s != icon->nplane)
		icon->nplane = s;
	s = gets(buf+14);
	if(s != icon->bits)
		icon->bits = s;

	/* limit what we handle */
	switch(icon->bits){
	case 1:
	case 2:
	case 4:
	case 8:
		break;
	default:
		werrstr("don't support %d bit pixels", icon->bits);
		return -1;
	}
	if(icon->nplane != 1){
		werrstr("don't support %d planes", icon->nplane);
		return -1;
	}

	cm = buf + 40;
	xor = cm + 4*icon->ncolor;
	and = xor + icon->h*4*((icon->bits*icon->w+31)/32);

	/* translate the color map to a plan 9 one */
	map2map = transcmap(icon, cm);

	/* convert the images */
	icon->img = xor2img(icon, xor, map2map);
	icon->mask = and2img(icon, and);

	/* so that we save an image with a white background */
	img = allocimage(display, icon->img->r, CMAP8, 0, DWhite);
	draw(img, icon->img->r, icon->img, icon->mask, ZP);
	icon->img = img;

	free(buf);
	free(map2map);
	return 0;
}

void
usage(void)
{
	fprint(2, "usage: %s -W winsize [file]\n", argv0);
	exits("usage");
}

enum
{
	Mimage,
	Mmask,
	Mexit,

	Up= 1,
	Down= 0
};

char	*menu3str[] = {
	"write image",
	"write mask",
	"exit",
	0
};

Menu	menu3 = {
	menu3str
};

Cursor sight = {
	{-7, -7},
	{0x1F, 0xF8, 0x3F, 0xFC, 0x7F, 0xFE, 0xFB, 0xDF,
	 0xF3, 0xCF, 0xE3, 0xC7, 0xFF, 0xFF, 0xFF, 0xFF,
	 0xFF, 0xFF, 0xFF, 0xFF, 0xE3, 0xC7, 0xF3, 0xCF,
	 0x7B, 0xDF, 0x7F, 0xFE, 0x3F, 0xFC, 0x1F, 0xF8,},
	{0x00, 0x00, 0x0F, 0xF0, 0x31, 0x8C, 0x21, 0x84,
	 0x41, 0x82, 0x41, 0x82, 0x41, 0x82, 0x7F, 0xFE,
	 0x7F, 0xFE, 0x41, 0x82, 0x41, 0x82, 0x41, 0x82,
	 0x21, 0x84, 0x31, 0x8C, 0x0F, 0xF0, 0x00, 0x00,}
};

void
buttons(int ud)
{
	while((mouse.buttons==0) != ud)
		mouse = emouse();
}

void
mesg(char *fmt, ...)
{
	va_list arg;
	char buf[1024];
	static char obuf[1024];

	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	string(screen, screen->r.min, background, ZP, font, obuf);
	string(screen, screen->r.min, display->white, ZP, font, buf);
	strcpy(obuf, buf);
}

void
doimage(Icon *icon)
{
	int rv;
	char file[256];
	int fd;

	rv = -1;
	snprint(file, sizeof(file), "%dx%d.img", icon->w, icon->h);
	fd = create(file, OWRITE, 0664);
	if(fd >= 0){
		rv = writeimage(fd, icon->img, 0);
		close(fd);
	}
	if(rv < 0)
		mesg("error writing %s: %r", file);
	else
		mesg("created %s", file);
}

void
domask(Icon *icon)
{
	int rv;
	char file[64];
	int fd;

	rv = -1;
	snprint(file, sizeof(file), "%dx%d.mask", icon->w, icon->h);
	fd = create(file, OWRITE, 0664);
	if(fd >= 0){
		rv = writeimage(fd, icon->mask, 0);
		close(fd);
	}
	if(rv < 0)
		mesg("error writing %s: %r", file);
	else
		mesg("created %s", file);
}

void
apply(void (*f)(Icon*))
{
	Icon *icon;

	esetcursor(&sight);
	buttons(Down);
	if(mouse.buttons == 4)
		for(icon = h.first; icon; icon = icon->next)
			if(ptinrect(mouse.xy, icon->sr)){
				buttons(Up);
				f(icon);
				break;
			}
	buttons(Up);
	esetcursor(0);
}

void
menu(void)
{
	int sel;

	sel = emenuhit(3, &mouse, &menu3);
	switch(sel){
	case Mimage:
		apply(doimage);
		break;
	case Mmask:
		apply(domask);
		break;
	case Mexit:
		exits(0);
		break;
	}
}

void
mousemoved(void)
{
	Icon *icon;

	for(icon = h.first; icon; icon = icon->next)
		if(ptinrect(mouse.xy, icon->sr)){
			mesg("%dx%d", icon->w, icon->h);
			return;
		}
	mesg("");
}

enum
{
	BORDER= 1
};

void
eresized(int new)
{
	Icon *icon;
	Rectangle r;

	if(new && getwindow(display, Refnone) < 0)
		sysfatal("can't reattach to window");
	draw(screen, screen->clipr, background, nil, ZP);
	r.max.x = screen->r.min.x;
	r.min.y = screen->r.min.y + font->height + 2*BORDER;
	for(icon = h.first; icon != nil; icon = icon->next){
		r.min.x = r.max.x + BORDER;
		r.max.x = r.min.x + Dx(icon->img->r);
		r.max.y = r.min.y + Dy(icon->img->r);
		draw(screen, r, icon->img, nil, ZP);
		border(screen, r, -BORDER, display->black, ZP);
		icon->sr = r;
	}
	flushimage(display, 1);
}

void
main(int argc, char **argv)
{
	Biobuf in;
	Icon *icon;
	int fd;
	Rectangle r;
	Event e;

	ARGBEGIN{
	case 'W':
		winsize = EARGF(usage());
		break;
	case 'd':
		debug = 1;
		break;
	}ARGEND;

	fd = -1;
	switch(argc){
	case 0:
		fd = 0;
		break;
	case 1:
		fd = open(argv[0], OREAD);
		if(fd < 0)
			sysfatal("opening: %r");
		break;
	default:
		usage();
		break;
	}

	Binit(&in, fd, OREAD);

	if(Bgetheader(&in, &h) < 0)
		sysfatal("reading header: %r");

	initdraw(0, nil, "ico");
	background = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x808080FF);

	einit(Emouse|Ekeyboard);

	r.min = Pt(4, 4);
	for(icon = h.first; icon != nil; icon = icon->next){
		if(Bgeticon(&in, icon) < 0){
			fprint(2, "bad rectangle: %r\n");
			continue;
		}
		if(debug)
			fprint(2, "w %ud h %ud ncolor %ud bits %ud len %lud offset %lud\n",
			   icon->w, icon->h, icon->ncolor, icon->bits, icon->len, icon->offset);
		r.max = addpt(r.min, Pt(icon->w, icon->h));
		icon->r = r;
		r.min.x += r.max.x;
	}
	eresized(0);

	for(;;)
		switch(event(&e)){
		case Ekeyboard:
			break;
		case Emouse:
			mouse = e.mouse;
			if(mouse.buttons & 4)
				menu();
			else
				mousemoved();
			break;
		}
}
