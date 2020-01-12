#include <u.h>
#include <libc.h>
#include <draw.h>
#include <cursor.h>
#include <event.h>
#include <bio.h>

typedef struct	Thing	Thing;

struct Thing
{
	Image	*b;
	Subfont 	*s;
	char		*name;	/* file name */
	int		face;		/* is 48x48 face file or cursor file*/
	Rectangle r;		/* drawing region */
	Rectangle tr;		/* text region */
	Rectangle er;		/* entire region */
	long		c;		/* character number in subfont */
	int		mod;	/* modified */
	int		mag;		/* magnification */
	Rune		off;		/* offset for subfont indices */
	Thing	*parent;	/* thing of which i'm an edit */
	Thing	*next;
};

enum
{
	Border	= 1,
	Up		= 1,
	Down	= 0,
	Mag		= 4,
	Maxmag	= 20
};

enum
{
	NORMAL	=0,
	FACE	=1,
	CURSOR	=2
};

enum
{
	Mopen,
	Mread,
	Mwrite,
	Mcopy,
	Mchar,
	Mpixels,
	Mclose,
	Mexit
};

enum
{
	Blue	= 54
};

char	*menu3str[] = {
	"open",
	"read",
	"write",
	"copy",
	"char",
	"pixels",
	"close",
	"exit",
	0
};

Menu	menu3 = {
	menu3str
};

Cursor sweep0 = {
	{-7, -7},
	{0x03, 0xC0, 0x03, 0xC0, 0x03, 0xC0, 0x03, 0xC0,
	 0x03, 0xC0, 0x03, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF,
	 0xFF, 0xFF, 0xFF, 0xFF, 0x03, 0xC0, 0x03, 0xC0,
	 0x03, 0xC0, 0x03, 0xC0, 0x03, 0xC0, 0x03, 0xC0},
	{0x00, 0x00, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80,
	 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x7F, 0xFE,
	 0x7F, 0xFE, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80,
	 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x00, 0x00}
};

Cursor box = {
	{-7, -7},
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	 0xFF, 0xFF, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F,
	 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xFF, 0xFF,
	 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{0x00, 0x00, 0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE,
	 0x70, 0x0E, 0x70, 0x0E, 0x70, 0x0E, 0x70, 0x0E,
	 0x70, 0x0E, 0x70, 0x0E, 0x70, 0x0E, 0x70, 0x0E,
	 0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x00, 0x00}
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

Cursor pixel = {
	{-7, -7},
	{0x1f, 0xf8, 0x3f, 0xfc,  0x7f, 0xfe,  0xf8, 0x1f,
	0xf0, 0x0f,  0xe0, 0x07, 0xe0, 0x07, 0xfe, 0x7f,
	0xfe, 0x7f, 0xe0, 0x07, 0xe0, 0x07, 0xf0, 0x0f,
	0x78, 0x1f, 0x7f, 0xfe, 0x3f, 0xfc, 0x1f, 0xf8, },
	{0x00, 0x00, 0x0f, 0xf0, 0x31, 0x8c, 0x21, 0x84,
	0x41, 0x82, 0x41, 0x82, 0x41, 0x82, 0x40, 0x02,
	0x40, 0x02, 0x41, 0x82, 0x41, 0x82, 0x41, 0x82,
	0x21, 0x84, 0x31, 0x8c, 0x0f, 0xf0, 0x00, 0x00, }
};

Cursor busy = {
	{-7, -7},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x0c, 0x00, 0x8e, 0x1d, 0xc7,
	 0xff, 0xe3, 0xff, 0xf3, 0xff, 0xff, 0x7f, 0xfe,
	 0x3f, 0xf8, 0x17, 0xf0, 0x03, 0xe0, 0x00, 0x00,},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x08, 0x00, 0x04, 0x00, 0x82,
	 0x04, 0x41, 0xff, 0xe1, 0x5f, 0xf1, 0x3f, 0xfe,
	 0x17, 0xf0, 0x03, 0xe0, 0x00, 0x00, 0x00, 0x00,}
};

Cursor skull = {
	{-7,-7},
	{0x00, 0x00, 0x00, 0x00, 0xc0, 0x03, 0xe7, 0xe7,
	 0xff, 0xff, 0xff, 0xff, 0x3f, 0xfc, 0x1f, 0xf8,
	 0x0f, 0xf0, 0x3f, 0xfc, 0xff, 0xff, 0xff, 0xff,
	 0xef, 0xf7, 0xc7, 0xe3, 0x00, 0x00, 0x00, 0x00,},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x03,
	 0xE7, 0xE7, 0x3F, 0xFC, 0x0F, 0xF0, 0x0D, 0xB0,
	 0x07, 0xE0, 0x06, 0x60, 0x37, 0xEC, 0xE4, 0x27,
	 0xC3, 0xC3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,}
};

Rectangle	cntlr;		/* control region */
Rectangle	editr;		/* editing region */
Rectangle	textr;		/* text region */
Thing		*thing;
Mouse		mouse;
char		hex[] = "0123456789abcdefABCDEF";
jmp_buf		err;
char		*file;
int		mag;
int		but1val = 0;
int		but2val = 255;
int		invert = 0;
Image		*values[256];
Image		*greyvalues[256];
uchar		data[8192];

Thing*	tget(char*, int);
void	mesg(char*, ...);
void	drawthing(Thing*, int);
void	xselect(void);
void	menu(void);
void	error(Display*, char*);
void	buttons(int);
void	drawall(void);
void	tclose1(Thing*);

void
usage(void)
{
	fprint(2, "usage: tweak [-W winsize] file...\n");
	exits("usage");
}

void
main(volatile int argc, char **volatile argv)
{
	volatile int i;
	Event e;
	Thing *t;
	Thing *nt;

	ARGBEGIN{
	case 'W':
		winsize = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND
	mag = Mag;
	if(initdraw(error, 0, "tweak") < 0){
		fprint(2, "tweak: initdraw failed: %r\n");
		exits("initdraw");
	}
	for(i=0; i<256; i++){
		values[i] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, cmap2rgba(i));
		greyvalues[i] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, (i<<24)|(i<<16)|(i<<8)|0xFF);
		if(values[i] == 0 || greyvalues[i] == 0)
			drawerror(display, "can't allocate image");
	}
	einit(Emouse|Ekeyboard);
	eresized(0);
	i = 0;
	setjmp(err);
	for(; i<argc; i++){
		file = argv[i];
		t = tget(argv[i], 1);
		if(t) {
			nt = t->next;
			t->next = 0;
			drawthing(t, 1);
			if(nt)
				drawthing(nt, 1);
		}
		flushimage(display, 1);
	}
	file = 0;
	setjmp(err);
	for(;;)
		switch(event(&e)){
		case Ekeyboard:
			break;
		case Emouse:
			mouse = e.mouse;
			if(mouse.buttons & 3){
				xselect();
				break;
			}
			if(mouse.buttons & 4)
				menu();
		}
}

int
xlog2(int n)
{
	int i;

	for(i=0; (1<<i) <= n; i++)
		if((1<<i) == n)
			return i;
	fprint(2, "log2 %d = 0\n", n);
	return 0;
}

void
error(Display *d, char *s)
{
	USED(d);

	if(file)
		mesg("can't read %s: %s: %r", file, s);
	else
		mesg("/dev/bitblt error: %s", s);
	if(err[0])
		longjmp(err, 1);
	exits(s);
}

void
redraw(Thing *t)
{
	Thing *nt;
	Point p;

	if(thing==0 || thing==t)
		draw(screen, editr, display->white, nil, ZP);
	if(thing == 0)
		return;
	if(thing != t){
		for(nt=thing; nt->next!=t; nt=nt->next)
			;
		draw(screen, Rect(screen->r.min.x, nt->er.max.y, editr.max.x, editr.max.y),
			display->white, nil, ZP);
	}
	for(nt=t; nt; nt=nt->next){
		drawthing(nt, 0);
		if(nt->next == 0){
			p = Pt(editr.min.x, nt->er.max.y);
			draw(screen, Rpt(p, editr.max), display->white, nil, ZP);
		}
	}
	mesg("");
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		error(display, "can't reattach to window");
	cntlr = insetrect(screen->clipr, 1);
	editr = cntlr;
	textr = editr;
	textr.min.y = textr.max.y - font->height;
	cntlr.max.y = cntlr.min.y + font->height;
	editr.min.y = cntlr.max.y+1;
	editr.max.y = textr.min.y-1;
	draw(screen, screen->clipr, display->white, nil, ZP);
	draw(screen, Rect(editr.min.x, editr.max.y, editr.max.x+1, editr.max.y+1), display->black, nil, ZP);
	replclipr(screen, 0, editr);
	drawall();
}

void
mesgstr(Point p, int line, char *s)
{
	Rectangle c, r;

	r.min = p;
	r.min.y += line*font->height;
	r.max.y = r.min.y+font->height;
	r.max.x = editr.max.x;
	c = screen->clipr;
	replclipr(screen, 0, r);
	draw(screen, r, values[0xDD], nil, ZP);
	r.min.x++;
	string(screen, r.min, display->black, ZP, font, s);
	replclipr(screen, 0, c);
	flushimage(display, 1);
}

void
mesg(char *fmt, ...)
{
	char buf[1024];
	va_list arg;

	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	mesgstr(textr.min, 0, buf);
}

void
tmesg(Thing *t, int line, char *fmt, ...)
{
	char buf[1024];
	va_list arg;

	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	mesgstr(t->tr.min, line, buf);
}


void
scntl(char *l)
{
	sprint(l, "mag: %d  but1: %d  but2: %d  invert-on-copy: %c", mag, but1val, but2val, "ny"[invert]);
}

void
cntl(void)
{
	char buf[256];

	scntl(buf);
	mesgstr(cntlr.min, 0, buf);
}

void
stext(Thing *t, char *l0, char *l1)
{
	Fontchar *fc;
	char buf[256];

	l1[0] = 0;
	sprint(buf, "depth:%d r:%d %d  %d %d ",
		t->b->depth, t->b->r.min.x, t->b->r.min.y,
		t->b->r.max.x, t->b->r.max.y);
	if(t->parent)
		sprint(buf+strlen(buf), "mag: %d ", t->mag);
	sprint(l0, "%s file: %s", buf, t->name);
	if(t->c >= 0){
		fc = &t->parent->s->info[t->c];
		sprint(l1, "c(hex): %x c(char): %C x: %d "
			   "top: %d bottom: %d left: %d width: %d iwidth: %d",
			(int)(t->c+t->parent->off), (int)(t->c+t->parent->off),
			fc->x, fc->top, fc->bottom, fc->left,
			fc->width, Dx(t->b->r));
	}else if(t->s)
		sprint(l1, "offset(hex): %ux n:%d  height:%d  ascent:%d",
			t->off, t->s->n, t->s->height, t->s->ascent);
	else if(t->face == CURSOR)
		sprint(l0+strlen(l0), " cursor:%d", Dx(t->b->r));
}

void
text(Thing *t)
{
	char l0[256], l1[256];

	stext(t, l0, l1);
	tmesg(t, 0, l0);
	if(l1[0])
		tmesg(t, 1, l1);
}

void
drawall(void)
{
	Thing *t;

	cntl();
	for(t=thing; t; t=t->next)
		drawthing(t, 0);
}

int
value(Image *b, int x)
{
	int v, l, w;
	uchar mask;

	w = b->depth;
	if(w > 8){
		mesg("ldepth too large");
		return 0;
	}
	l = xlog2(w);
	mask = (1<<w)-1;		/* ones at right end of word */
	x -= b->r.min.x&~(7>>l);	/* adjust x relative to first pixel */
	v = data[x>>(3-l)];
	v >>= ((7>>l)<<l) - ((x&(7>>l))<<l);	/* pixel at right end of word */
	v &= mask;			/* pixel at right end of word */
	return v;
}

int
bvalue(int v, int d)
{
	v &= (1<<d)-1;
	if(d > screen->depth)
		v >>= d - screen->depth;
	else
		while(d < screen->depth && d < 8){
			v |= v << d;
			d <<= 1;
		}
	if(v<0 || v>255){
		mesg("internal error: bad color");
		return Blue;
	}
	return v;
}

void
drawthing(Thing *nt, int link)
{
	int n, nl, nf, i, x, y, sx, sy, fdx, dx, dy, v;
	Thing *t;
	Subfont *s;
	Image *b, *col;
	Point p, p1, p2;

	if(link){
		nt->next = 0;
		if(thing == 0){
			thing = nt;
			y = editr.min.y;
		}else{
			for(t=thing; t->next; t=t->next)
				;
			t->next = nt;
			y = t->er.max.y;
		}
	}else{
		if(thing == nt)
			y = editr.min.y;
		else{
			for(t=thing; t->next!=nt; t=t->next)
				;
			y = t->er.max.y;
		}
	}
	s = nt->s;
	b = nt->b;
	nl = font->height;
	if(s || nt->c>=0)
		nl += font->height;
	fdx = Dx(editr) - 2*Border;
	dx = Dx(b->r);
	dy = Dy(b->r);
	if(nt->mag > 1){
		dx *= nt->mag;
		dy *= nt->mag;
		fdx -= fdx%nt->mag;
	}
	nf = 1 + dx/fdx;
	nt->er.min.y = y;
	nt->er.min.x = editr.min.x;
	nt->er.max.x = nt->er.min.x + Border + dx + Border;
	if(nt->er.max.x > editr.max.x)
		nt->er.max.x = editr.max.x;
	nt->er.max.y = nt->er.min.y + Border + nf*(dy+Border);
	nt->r = insetrect(nt->er, Border);
	nt->er.max.x = editr.max.x;
	draw(screen, nt->er, display->white, nil, ZP);
	for(i=0; i<nf; i++){
		p1 = Pt(nt->r.min.x-1, nt->r.min.y+i*(Border+dy));
		/* draw portion of bitmap */
		p = Pt(p1.x+1, p1.y);
		if(nt->mag == 1)
			draw(screen, Rect(p.x, p.y, p.x+fdx+Dx(b->r), p.y+Dy(b->r)),
				b, nil, Pt(b->r.min.x+i*fdx, b->r.min.y));
		else{
			for(y=b->r.min.y; y<b->r.max.y; y++){
				sy = p.y+(y-b->r.min.y)*nt->mag;
				if((n=unloadimage(b, Rect(b->r.min.x, y, b->r.max.x, y+1), data, sizeof data)) < 0)
					fprint(2, "unloadimage: %r\n");
				for(x=b->r.min.x+i*(fdx/nt->mag); x<b->r.max.x; x++){
					sx = p.x+(x-i*(fdx/nt->mag)-b->r.min.x)*nt->mag;
					if(sx >= nt->r.max.x)
						break;
					v = bvalue(value(b, x), b->depth);
					if(v == 255)
						continue;
					if(b->chan == GREY8)
						draw(screen, Rect(sx, sy, sx+nt->mag, sy+nt->mag),
							greyvalues[v], nil, ZP);
					else
						draw(screen, Rect(sx, sy, sx+nt->mag, sy+nt->mag),
							values[v], nil, ZP);
				}

			}
		}
		/* line down left */
		if(i == 0)
			col = display->black;
		else
			col = display->white;
		draw(screen, Rect(p1.x, p1.y, p1.x+1, p1.y+dy+Border), col, nil, ZP);
		/* line across top */
		draw(screen, Rect(p1.x, p1.y-1, nt->r.max.x+Border, p1.y), display->black, nil, ZP);
		p2 = p1;
		if(i == nf-1){
			p2.x += 1 + dx%fdx;
			col = display->black;
		}else{
			p2.x = nt->r.max.x;
			col = display->white;
		}
		/* line down right */
		draw(screen, Rect(p2.x, p2.y, p2.x+1, p2.y+dy+Border), col, nil, ZP);
		/* line across bottom */
		if(i == nf-1){
			p1.y += Border+dy;
			draw(screen, Rect(p1.x, p1.y-1, p2.x,p1.y), display->black, nil, ZP);
		}
	}
	nt->tr.min.x = editr.min.x;
	nt->tr.max.x = editr.max.x;
	nt->tr.min.y = nt->er.max.y + Border;
	nt->tr.max.y = nt->tr.min.y + nl;
	nt->er.max.y = nt->tr.max.y + Border;
	text(nt);
}

int
tohex(int c)
{
	if('0'<=c && c<='9')
		return c - '0';
	if('a'<=c && c<='f')
		return 10 + (c - 'a');
	if('A'<=c && c<='F')
		return 10 + (c - 'A');
	return 0;
}

Thing*
tget(char *file, int extra)
{
	int i, j, fd, face, x, y, c, chan;
	Image *b;
	Subfont *s;
	Thing *t;
	Dir *volatile d;
	jmp_buf oerr;
	uchar buf[300];
	char *data;
	Rectangle r;

	buf[0] = '\0';
	errstr((char*)buf, sizeof buf);	/* flush pending error message */
	memmove(oerr, err, sizeof err);
	d = nil;
	if(setjmp(err)){
   Err:
		free(d);
		memmove(err, oerr, sizeof err);
		return 0;
	}
	fd = open(file, OREAD);
	if(fd < 0){
		mesg("can't open %s: %r", file);
		goto Err;
	}
	d = dirfstat(fd);
	if(d == nil){
		mesg("can't stat bitmap file %s: %r", file);
		close(fd);
		goto Err;
	}
	if(read(fd, buf, 11) != 11){
		mesg("can't read %s: %r", file);
		close(fd);
		goto Err;
	}
	seek(fd, 0, 0);
	data = (char*)buf;
	if(*data == '{')
		data++;
	if(memcmp(data, "0x", 2)==0 && data[4]==','){
		/*
		 * cursor file
		 */
		face = CURSOR;
		s = 0;
		data = malloc(d->length+1);
		if(data == 0){
			mesg("can't malloc buffer: %r");
			close(fd);
			goto Err;
		}
		data[d->length] = 0;
		if(read(fd, data, d->length) != d->length){
			mesg("can't read cursor file %s: %r", file);
			close(fd);
			goto Err;
		}
		i = 0;
		for(x=0;; ){
			if((c=data[i]) == '\0' || x > 256) {
				if(x == 64 || x == 256)
					goto HaveCursor;
				mesg("ill-formed cursor file %s", file);
				close(fd);
				goto Err;
			}
			if(c=='0' && data[i+1] == 'x'){
				i += 2;
				continue;
			}
			if(strchr(hex, c)){
				buf[x++] = (tohex(c)<<4) | tohex(data[i+1]);
				i += 2;
				continue;
			}
			i++;
		}
	HaveCursor:
		if(x == 64)
			r = Rect(0, 0, 16, 32);
		else
			r = Rect(0, 0, 32, 64);
		b = allocimage(display, r, GREY1, 0, DNofill);
		if(b == 0){
			mesg("image alloc failed file %s: %r", file);
			free(data);
			close(fd);
			goto Err;
		}
		loadimage(b, r, buf, sizeof buf);
		free(data);
	}else if(memcmp(buf, "0x", 2)==0){
		/*
		 * face file
		 */
		face = FACE;
		s = 0;
		data = malloc(d->length+1);
		if(data == 0){
			mesg("can't malloc buffer: %r");
			close(fd);
			goto Err;
		}
		data[d->length] = 0;
		if(read(fd, data, d->length) != d->length){
			mesg("can't read bitmap file %s: %r", file);
			close(fd);
			goto Err;
		}
		for(y=0,i=0; i<d->length; i++)
			if(data[i] == '\n')
				y++;
		if(y == 0){
	ill:
			mesg("ill-formed face file %s", file);
			close(fd);
			free(data);
			goto Err;
		}
		for(x=0,i=0; (c=data[i])!='\n'; ){
			if(c==',' || c==' ' || c=='\t'){
				i++;
				continue;
			}
			if(c=='0' && data[i+1] == 'x'){
				i += 2;
				continue;
			}
			if(strchr(hex, c)){
				x += 4;
				i++;
				continue;
			}
			goto ill;
		}
		if(x % y)
			goto ill;
		switch(x / y){
		default:
			goto ill;
		case 1:
			chan = GREY1;
			break;
		case 2:
			chan = GREY2;
			break;
		case 4:
			chan = GREY4;
			break;
		case 8:
			chan = CMAP8;
			break;
		}
		b = allocimage(display, Rect(0, 0, y, y), chan, 0, -1);
		if(b == 0){
			mesg("image alloc failed file %s: %r", file);
			free(data);
			close(fd);
			goto Err;
		}
		i = 0;
		for(j=0; j<y; j++){
			for(x=0; (c=data[i])!='\n'; ){
				if(c=='0' && data[i+1] == 'x'){
					i += 2;
					continue;
				}
				if(strchr(hex, c)){
					buf[x++] = ~((tohex(c)<<4) | tohex(data[i+1]));
					i += 2;
					continue;
				}
				i++;
			}
			i++;
			loadimage(b, Rect(0, j, y, j+1), buf, sizeof buf);
		}
		free(data);
	}else{
		face = NORMAL;
		s = 0;
		b = readimage(display, fd, 0);
		if(b == 0){
			mesg("can't read bitmap file %s: %r", file);
			close(fd);
			goto Err;
		}
		if(seek(fd, 0, 1) < d->length)
			s = readsubfonti(display, file, fd, b, 0);
	}
	close(fd);
	t = mallocz(sizeof(Thing), 1);
	if(t == 0){
   nomem:
		mesg("malloc failed: %r");
		if(s)
			freesubfont(s);
		else
			freeimage(b);
		goto Err;
	}
	t->name = strdup(file);
	if(t->name == 0){
		free(t);
		goto nomem;
	}
	t->b = b;
	t->s = s;
	t->face = face;
	t->mod = 0;
	t->parent = 0;
	t->c = -1;
	t->mag = 1;
	t->off = 0;
	if(face == CURSOR && extra && Dx(t->b->r) == 16) {
		// Make 32x32 cursor as second image.
		Thing *nt;
		Cursor c;
		Cursor2 c2;

		nt = mallocz(sizeof(Thing), 1);
		if(nt == 0)
			goto nomem;
		nt->name = strdup("");
		if(nt->name == 0) {
			free(nt);
			goto nomem;
		}
		b = allocimage(display, Rect(0, 0, 32, 64), GREY1, 0, DNofill);
		if(b == nil) {
			free(nt->name);
			free(nt);
			goto nomem;
		}
		memmove(c.clr, buf, 64);
		scalecursor(&c2, &c);
		memmove(buf, c2.clr, 256);
		loadimage(b, b->r, buf, sizeof buf);
		t->next = nt;
		nt->b = b;
		nt->s = 0;
		nt->face = CURSOR;
		nt->mod = 0;
		nt->parent = 0;
		nt->c = -1;
		nt->mag = 1;
		nt->off = 0;
	}
	memmove(err, oerr, sizeof err);
	return t;
}

int
atline(int x, Point p, char *line, char *buf)
{
	char *s, *c, *word, *hit;
	int w, wasblank;
	Rune r;

	wasblank = 1;
	hit = 0;
	word = 0;
	for(s=line; *s; s+=w){
		w = chartorune(&r, s);
		x += runestringnwidth(font, &r, 1);
		if(wasblank && r!=' ')
			word = s;
		wasblank = 0;
		if(r == ' '){
			if(x >= p.x)
				break;
			wasblank = 1;
		}
		if(r == ':')
			hit = word;
	}
	if(x < p.x)
		return 0;
	c = utfrune(hit, ':');
	strncpy(buf, hit, c-hit);
	buf[c-hit] = 0;
	return 1;
}

int
attext(Thing *t, Point p, char *buf)
{
	char l0[256], l1[256];

	if(!ptinrect(p, t->tr))
		return 0;
	stext(t, l0, l1);
	if(p.y < t->tr.min.y+font->height)
		return atline(t->r.min.x, p, l0, buf);
	else
		return atline(t->r.min.x, p, l1, buf);
}

int
type(char *buf, char *tag)
{
	Rune r;
	char *p;

	esetcursor(&busy);
	p = buf;
	for(;;){
		*p = 0;
		mesg("%s: %s", tag, buf);
		r = ekbd();
		switch(r){
		case '\n':
			mesg("");
			esetcursor(0);
			return p-buf;
		case 0x15:	/* control-U */
			p = buf;
			break;
		case '\b':
			if(p > buf)
				--p;
			break;
		default:
			p += runetochar(p, &r);
		}
	}
	/* return 0;	shut up compiler */
}

void
textedit(Thing *t, char *tag)
{
	char buf[256];
	char *s;
	Image *b;
	Subfont *f;
	Fontchar *fc, *nfc;
	Rectangle r;
	ulong chan;
	int i, ld, d, w, c, doredraw, fdx, x;
	Thing *nt;

	buttons(Up);
	if(type(buf, tag) == 0)
		return;
	if(strcmp(tag, "file") == 0){
		for(s=buf; *s; s++)
			if(*s <= ' '){
				mesg("illegal file name");
				return;
			}
		if(strcmp(t->name, buf) != 0){
			if(t->parent)
				t->parent->mod = 1;
			else
				t->mod = 1;
		}
		for(nt=thing; nt; nt=nt->next)
			if(t==nt || t->parent==nt || nt->parent==t){
				free(nt->name);
				nt->name = strdup(buf);
				if(nt->name == 0){
					mesg("malloc failed: %r");
					return;
				}
				text(nt);
			}
		return;
	}
	if(strcmp(tag, "depth") == 0){
		if(buf[0]<'0' || '9'<buf[0] || (d=atoi(buf))<0 || d>8 || xlog2(d)<0){
			mesg("illegal ldepth");
			return;
		}
		if(d == t->b->depth)
			return;
		if(t->parent)
			t->parent->mod = 1;
		else
			t->mod = 1;
		if(d == 8)
			chan = CMAP8;
		else
			chan = CHAN1(CGrey, d);
		for(nt=thing; nt; nt=nt->next){
			if(nt!=t && nt!=t->parent && nt->parent!=t)
				continue;
			b = allocimage(display, nt->b->r, chan, 0, 0);
			if(b == 0){
	nobmem:
				mesg("image alloc failed: %r");
				return;
			}
			draw(b, b->r, nt->b, nil, nt->b->r.min);
			freeimage(nt->b);
			nt->b = b;
			if(nt->s){
				b = allocimage(display, nt->b->r, chan, 0, -1);
				if(b == 0)
					goto nobmem;
				draw(b, b->r, nt->b, nil, nt->b->r.min);
				f = allocsubfont(t->name, nt->s->n, nt->s->height, nt->s->ascent, nt->s->info, b);
				if(f == 0){
	nofmem:
					freeimage(b);
					mesg("can't make subfont: %r");
					return;
				}
				nt->s->info = 0;	/* prevent it being freed */
				nt->s->bits = 0;
				freesubfont(nt->s);
				nt->s = f;
			}
			drawthing(nt, 0);
		}
		return;
	}
	if(strcmp(tag, "mag") == 0){
		if(buf[0]<'0' || '9'<buf[0] || (ld=atoi(buf))<=0 || ld>Maxmag){
			mesg("illegal magnification");
			return;
		}
		if(t->mag == ld)
			return;
		t->mag = ld;
		redraw(t);
		return;
	}
	if(strcmp(tag, "r") == 0){
		if(t->s){
			mesg("can't change rectangle of subfont\n");
			return;
		}
		s = buf;
		r.min.x = strtoul(s, &s, 0);
		r.min.y = strtoul(s, &s, 0);
		r.max.x = strtoul(s, &s, 0);
		r.max.y = strtoul(s, &s, 0);
		if(Dx(r)<=0 || Dy(r)<=0){
			mesg("illegal rectangle");
			return;
		}
		if(t->parent)
			t = t->parent;
		for(nt=thing; nt; nt=nt->next){
			if(nt->parent==t && !rectinrect(nt->b->r, r))
				tclose1(nt);
		}
		b = allocimage(display, r, t->b->chan, 0, 0);
		if(b == 0)
			goto nobmem;
		draw(b, r, t->b, nil, r.min);
		freeimage(t->b);
		t->b = b;
		b = allocimage(display, r, t->b->chan, 0, 0);
		if(b == 0)
			goto nobmem;
		redraw(t);
		t->mod = 1;
		return;
	}
	if(strcmp(tag, "ascent") == 0){
		if(buf[0]<'0' || '9'<buf[0] || (ld=atoi(buf))<0 || ld>t->s->height){
			mesg("illegal ascent");
			return;
		}
		if(t->s->ascent == ld)
			return;
		t->s->ascent = ld;
		text(t);
		t->mod = 1;
		return;
	}
	if(strcmp(tag, "height") == 0){
		if(buf[0]<'0' || '9'<buf[0] || (ld=atoi(buf))<0){
			mesg("illegal height");
			return;
		}
		if(t->s->height == ld)
			return;
		t->s->height = ld;
		text(t);
		t->mod = 1;
		return;
	}
	if(strcmp(tag, "left")==0 || strcmp(tag, "width") == 0){
		if(buf[0]<'0' || '9'<buf[0] || (ld=atoi(buf))<0){
			mesg("illegal value");
			return;
		}
		fc = &t->parent->s->info[t->c];
		if(strcmp(tag, "left")==0){
			if(fc->left == ld)
				return;
			fc->left = ld;
		}else{
			if(fc->width == ld)
				return;
			fc->width = ld;
		}
		text(t);
		t->parent->mod = 1;
		return;
	}
	if(strcmp(tag, "offset(hex)") == 0){
		if(!strchr(hex, buf[0])){
	illoff:
			mesg("illegal offset");
			return;
		}
		s = 0;
		ld = strtoul(buf, &s, 16);
		if(*s)
			goto illoff;
		t->off = ld;
		text(t);
		for(nt=thing; nt; nt=nt->next)
			if(nt->parent == t)
				text(nt);
		return;
	}
	if(strcmp(tag, "n") == 0){
		if(buf[0]<'0' || '9'<buf[0] || (w=atoi(buf))<=0){
			mesg("illegal n");
			return;
		}
		f = t->s;
		if(w == f->n)
			return;
		doredraw = 0;
	again:
		for(nt=thing; nt; nt=nt->next)
			if(nt->parent == t){
				doredraw = 1;
				tclose1(nt);
				goto again;
			}
		r = t->b->r;
		if(w < f->n)
			r.max.x = f->info[w].x;
		b = allocimage(display, r, t->b->chan, 0, 0);
		if(b == 0)
			goto nobmem;
		draw(b, b->r, t->b, nil, r.min);
		fdx = Dx(editr) - 2*Border;
		if(Dx(t->b->r)/fdx != Dx(b->r)/fdx)
			doredraw = 1;
		freeimage(t->b);
		t->b = b;
		b = allocimage(display, r, t->b->chan, 0, 0);
		if(b == 0)
			goto nobmem;
		draw(b, b->r, t->b, nil, r.min);
		nfc = malloc((w+1)*sizeof(Fontchar));
		if(nfc == 0){
			mesg("malloc failed");
			freeimage(b);
			return;
		}
		fc = f->info;
		for(i=0; i<=w && i<=f->n; i++)
			nfc[i] = fc[i];
		if(i < w+1)
			memset(nfc+i, 0, ((w+1)-i)*sizeof(Fontchar));
		x = fc[f->n].x;
		for(; i<=w; i++)
			nfc[i].x = x;
		f = allocsubfont(t->name, w, f->height, f->ascent, nfc, b);
		if(f == 0)
			goto nofmem;
		t->s->bits = nil;	/* don't free it */
		freesubfont(t->s);
		f->info = nfc;
		t->s = f;
		if(doredraw)
			redraw(thing);
		else
			drawthing(t, 0);
		t->mod = 1;
		return;
	}
	if(strcmp(tag, "iwidth") == 0){
		if(buf[0]<'0' || '9'<buf[0] || (w=atoi(buf))<0){
			mesg("illegal iwidth");
			return;
		}
		w -= Dx(t->b->r);
		if(w == 0)
			return;
		r = t->parent->b->r;
		r.max.x += w;
		c = t->c;
		t = t->parent;
		f = t->s;
		b = allocimage(display, r, t->b->chan, 0, 0);
		if(b == 0)
			goto nobmem;
		fc = &f->info[c];
		draw(b, Rect(b->r.min.x, b->r.min.y,
				b->r.min.x+(fc[1].x-t->b->r.min.x), b->r.min.y+Dy(t->b->r)),
				t->b, nil, t->b->r.min);
		draw(b, Rect(fc[1].x+w, b->r.min.y, w+t->b->r.max.x, b->r.min.y+Dy(t->b->r)),
			t->b, nil, Pt(fc[1].x, t->b->r.min.y));
		fdx = Dx(editr) - 2*Border;
		doredraw = 0;
		if(Dx(t->b->r)/fdx != Dx(b->r)/fdx)
			doredraw = 1;
		freeimage(t->b);
		t->b = b;
		b = allocimage(display, r, t->b->chan, 0, 0);
		if(b == 0)
			goto nobmem;
		draw(b, b->r, t->b, nil, t->b->r.min);
		fc = &f->info[c+1];
		for(i=c+1; i<=f->n; i++, fc++)
			fc->x += w;
		f = allocsubfont(t->name, f->n, f->height, f->ascent,
			f->info, b);
		if(f == 0)
			goto nofmem;
		/* t->s and f share info; free carefully */
		fc = f->info;
		t->s->bits = nil;
		t->s->info = 0;
		freesubfont(t->s);
		f->info = fc;
		t->s = f;
		if(doredraw)
			redraw(t);
		else
			drawthing(t, 0);
		/* redraw all affected chars */
		for(nt=thing; nt; nt=nt->next){
			if(nt->parent!=t || nt->c<c)
				continue;
			fc = &f->info[nt->c];
			r.min.x = fc[0].x;
			r.min.y = nt->b->r.min.y;
			r.max.x = fc[1].x;
			r.max.y = nt->b->r.max.y;
			b = allocimage(display, r, nt->b->chan, 0, 0);
			if(b == 0)
				goto nobmem;
			draw(b, r, t->b, nil, r.min);
			doredraw = 0;
			if(Dx(nt->b->r)/fdx != Dx(b->r)/fdx)
				doredraw = 1;
			freeimage(nt->b);
			nt->b = b;
			if(c != nt->c)
				text(nt);
			else{
				if(doredraw)
					redraw(nt);
				else
					drawthing(nt, 0);
			}
		}
		t->mod = 1;
		return;
	}
	mesg("cannot edit %s in file %s", tag, t->name);
}

void
cntledit(char *tag)
{
	char buf[256];
	ulong l;

	buttons(Up);
	if(type(buf, tag) == 0)
		return;
	if(strcmp(tag, "mag") == 0){
		if(buf[0]<'0' || '9'<buf[0] || (l=atoi(buf))<=0 || l>Maxmag){
			mesg("illegal magnification");
			return;
		}
		mag = l;
		cntl();
		return;
	}
	if(strcmp(tag, "but1")==0
	|| strcmp(tag, "but2")==0){
		if(buf[0]<'0' || '9'<buf[0] || (long)(l=atoi(buf))<0 || l>255){
			mesg("illegal value");
			return;
		}
		if(strcmp(tag, "but1") == 0)
			but1val = l;
		else if(strcmp(tag, "but2") == 0)
			but2val = l;
		cntl();
		return;
	}
	if(strcmp(tag, "invert-on-copy")==0){
		if(buf[0]=='y' || buf[0]=='1')
			invert = 1;
		else if(buf[0]=='n' || buf[0]=='0')
			invert = 0;
		else{
			mesg("illegal value");
			return;
		}
		cntl();
		return;
	}
	mesg("cannot edit %s", tag);
}

void
buttons(int ud)
{
	while((mouse.buttons==0) != ud)
		mouse = emouse();
}

Point
screenpt(Thing *t, Point realp)
{
	int fdx, n;
	Point p;

	fdx = Dx(editr)-2*Border;
	if(t->mag > 1)
		fdx -= fdx%t->mag;
	p = mulpt(subpt(realp, t->b->r.min), t->mag);
	if(fdx < Dx(t->b->r)*t->mag){
		n = p.x/fdx;
		p.y += n * (Dy(t->b->r)*t->mag+Border);
		p.x -= n * fdx;
	}
	p = addpt(p, t->r.min);
	return p;
}

Point
realpt(Thing *t, Point screenp)
{
	int fdx, n, dy;
	Point p;

	fdx = (Dx(editr)-2*Border);
	if(t->mag > 1)
		fdx -= fdx%t->mag;
	p.y = screenp.y-t->r.min.y;
	p.x = 0;
	if(fdx < Dx(t->b->r)*t->mag){
		dy = Dy(t->b->r)*t->mag+Border;
		n = (p.y/dy);
		p.x = n * fdx;
		p.y -= n * dy;
	}
	p.x += screenp.x-t->r.min.x;
	p = addpt(divpt(p, t->mag), t->b->r.min);
	return p;
}

int
sweep(int but, Rectangle *r)
{
	Thing *t;
	Point p, q, lastq;

	esetcursor(&sweep0);
	buttons(Down);
	if(mouse.buttons != (1<<(but-1))){
		buttons(Up);
		esetcursor(0);
		return 0;
	}
	p = mouse.xy;
	for(t=thing; t; t=t->next)
		if(ptinrect(p, t->r))
			break;
	if(t)
		p = screenpt(t, realpt(t, p));
	r->min = p;
	r->max = p;
	esetcursor(&box);
	lastq = ZP;
	while(mouse.buttons == (1<<(but-1))){
		edrawgetrect(insetrect(*r, -Borderwidth), 1);
		mouse = emouse();
		edrawgetrect(insetrect(*r, -Borderwidth), 0);
		q = mouse.xy;
		if(t)
			q = screenpt(t, realpt(t, q));
		if(eqpt(q, lastq))
			continue;
		*r = canonrect(Rpt(p, q));
		lastq = q;
	}
	esetcursor(0);
	if(mouse.buttons){
		buttons(Up);
		return 0;
	}
	return 1;
}

void
openedit(Thing *t, Point pt, int c)
{
	int x, y;
	Point p;
	Rectangle r;
	Rectangle br;
	Fontchar *fc;
	Thing *nt;

	if(t->b->depth > 8){
		mesg("image has depth %d; can't handle >8", t->b->depth);
		return;
	}
	br = t->b->r;
	if(t->s == 0){
		c = -1;
		/* if big enough to bother, sweep box */
		if(Dx(br)<=16 && Dy(br)<=16)
			r = br;
		else{
			if(!sweep(1, &r))
				return;
			r = rectaddpt(r, subpt(br.min, t->r.min));
			if(!rectclip(&r, br))
				return;
			if(Dx(br) <= 8){
				r.min.x = br.min.x;
				r.max.x = br.max.x;
			}else if(Dx(r) < 4){
	    toosmall:
				mesg("rectangle too small");
				return;
			}
			if(Dy(br) <= 8){
				r.min.y = br.min.y;
				r.max.y = br.max.y;
			}else if(Dy(r) < 4)
				goto toosmall;
		}
	}else if(c >= 0){
		fc = &t->s->info[c];
		r.min.x = fc[0].x;
		r.min.y = br.min.y;
		r.max.x = fc[1].x;
		r.max.y = br.min.y + Dy(br);
	}else{
		/* just point at character */
		fc = t->s->info;
		p = addpt(pt, subpt(br.min, t->r.min));
		x = br.min.x;
		y = br.min.y;
		for(c=0; c<t->s->n; c++,fc++){
	    again:
			r.min.x = x;
			r.min.y = y;
			r.max.x = x + fc[1].x - fc[0].x;
			r.max.y = y + Dy(br);
			if(ptinrect(p, r))
				goto found;
			if(r.max.x >= br.min.x+Dx(t->r)){
				x -= Dx(t->r);
				y += t->s->height;
				if(fc[1].x > fc[0].x)
					goto again;
			}
			x += fc[1].x - fc[0].x;
		}
		return;
	   found:
		r = br;
		r.min.x = fc[0].x;
		r.max.x = fc[1].x;
	}
	nt = malloc(sizeof(Thing));
	if(nt == 0){
   nomem:
		mesg("can't allocate: %r");
		return;
	}
	memset(nt, 0, sizeof(Thing));
	nt->c = c;
	nt->b = allocimage(display, r, t->b->chan, 0, DNofill);
	if(nt->b == 0){
		free(nt);
		goto nomem;
	}
	draw(nt->b, r, t->b, nil, r.min);
	nt->name = strdup(t->name);
	if(nt->name == 0){
		freeimage(nt->b);
		free(nt);
		goto nomem;
	}
	nt->parent = t;
	nt->mag = mag;
	drawthing(nt, 1);
}

void
ckinfo(Thing *t, Rectangle mod)
{
	int i, j, k, top, bot, n, zero;
	Fontchar *fc;
	Rectangle r;
	Image *b;
	Thing *nt;

	if(t->parent)
		t = t->parent;
	if(t->s==0 || Dy(t->b->r)==0)
		return;
	b = 0;
	/* check bounding boxes */
	fc = &t->s->info[0];
	r.min.y = t->b->r.min.y;
	r.max.y = t->b->r.max.y;
	for(i=0; i<t->s->n; i++, fc++){
		r.min.x = fc[0].x;
		r.max.x = fc[1].x;
		if(!rectXrect(mod, r))
			continue;
		if(b==0 || Dx(b->r)<Dx(r)){
			if(b)
				freeimage(b);
			b = allocimage(display, rectsubpt(r, r.min), t->b->chan, 0, 0);
			if(b == 0){
				mesg("can't alloc image");
				break;
			}
		}
		draw(b, b->r, display->white, nil, ZP);
		draw(b, b->r, t->b, nil, r.min);
		top = 100000;
		bot = 0;
		n = 2+((Dx(r)/8)*t->b->depth);
		for(j=0; j<b->r.max.y; j++){
			memset(data, 0, n);
			unloadimage(b, Rect(b->r.min.x, j, b->r.max.x, j+1), data, sizeof data);
			zero = 1;
			for(k=0; k<n; k++)
				if(data[k]){
					zero = 0;
					break;
				}
			if(!zero){
				if(top > j)
					top = j;
				bot = j+1;
			}
		}
		if(top > j)
			top = 0;
		if(top!=fc->top || bot!=fc->bottom){
			fc->top = top;
			fc->bottom = bot;
			for(nt=thing; nt; nt=nt->next)
				if(nt->parent==t && nt->c==i)
					text(nt);
		}
	}
	if(b)
		freeimage(b);
}

void
twidpix(Thing *t, Point p, int set)
{
	Image *b, *v;
	int c;

	b = t->b;
	if(!ptinrect(p, b->r))
		return;
	if(set)
		c = but1val;
	else
		c = but2val;
	if(b->chan == GREY8)
		v = greyvalues[c];
	else
		v = values[c];
	draw(b, Rect(p.x, p.y, p.x+1, p.y+1), v, nil, ZP);
	p = screenpt(t, p);
	draw(screen, Rect(p.x, p.y, p.x+t->mag, p.y+t->mag), v, nil, ZP);
}

void
twiddle(Thing *t)
{
	int set;
	Point p, lastp;
	Image *b;
	Thing *nt;
	Rectangle mod;

	if(mouse.buttons!=1 && mouse.buttons!=2){
		buttons(Up);
		return;
	}
	set = mouse.buttons==1;
	b = t->b;
	lastp = addpt(b->r.min, Pt(-1, -1));
	mod = Rpt(addpt(b->r.max, Pt(1, 1)), lastp);
	while(mouse.buttons){
		p = realpt(t, mouse.xy);
		if(!eqpt(p, lastp)){
			lastp = p;
			if(ptinrect(p, b->r)){
				for(nt=thing; nt; nt=nt->next)
					if(nt->parent==t->parent || nt==t->parent)
						twidpix(nt, p, set);
				if(t->parent)
					t->parent->mod = 1;
				else
					t->mod = 1;
				if(p.x < mod.min.x)
					mod.min.x = p.x;
				if(p.y < mod.min.y)
					mod.min.y = p.y;
				if(p.x >= mod.max.x)
					mod.max.x = p.x+1;
				if(p.y >= mod.max.y)
					mod.max.y = p.y+1;
			}
		}
		mouse = emouse();
	}
	ckinfo(t, mod);
}

void
xselect(void)
{
	Thing *t;
	char line[128], buf[128];
	Point p;

	if(ptinrect(mouse.xy, cntlr)){
		scntl(line);
		if(atline(cntlr.min.x, mouse.xy, line, buf)){
			if(mouse.buttons == 1)
				cntledit(buf);
			else
				buttons(Up);
			return;
		}
		return;
	}
	for(t=thing; t; t=t->next){
		if(attext(t, mouse.xy, buf)){
			if(mouse.buttons == 1)
				textedit(t, buf);
			else
				buttons(Up);
			return;
		}
		if(ptinrect(mouse.xy, t->r)){
			if(t->parent == 0){
				if(mouse.buttons == 1){
					p = mouse.xy;
					buttons(Up);
					openedit(t, p, -1);
				}else
					buttons(Up);
				return;
			}
			twiddle(t);
			return;
		}
	}
}

void
twrite(Thing *t)
{
	int i, j, x, y, fd, ws, ld;
	Biobuf buf;
	Rectangle r;

	if(t->parent)
		t = t->parent;
	esetcursor(&busy);
	fd = create(t->name, OWRITE, 0666);
	if(fd < 0){
		mesg("can't write %s: %r", t->name);
		return;
	}
	if(t->face && t->b->depth <= 4){
		r = t->b->r;
		ld = xlog2(t->b->depth);
		/* This heuristic reflects peculiarly different formats */
		ws = 4;
		if(t->face == 2)	/* cursor file */
			ws = 1;
		else if(Dx(r)<32 || ld==0)
			ws = 2;
		Binit(&buf, fd, OWRITE);
		if(t->face == CURSOR)
			Bprint(&buf, "{");
		for(y=r.min.y; y<r.max.y; y++){
			unloadimage(t->b, Rect(r.min.x, y, r.max.x, y+1), data, sizeof data);
			j = 0;
			for(x=r.min.x; x<r.max.x; j+=ws,x+=ws*8>>ld){
				Bprint(&buf, "0x");
				for(i=0; i<ws; i++)
					Bprint(&buf, "%.2x", data[i+j]);
				Bprint(&buf, ", ");
			}
			if(t->face == CURSOR) {
				if(y == Dy(r)/2-1)
					Bprint(&buf, "},\n{");
				else if(y == Dy(r)-1)
					Bprint(&buf, "}\n");
				else
					Bprint(&buf, "\n\t");
			}else
				Bprint(&buf, "\n");
		}
		Bterm(&buf);
	}else
		if(writeimage(fd, t->b, 0)<0 || (t->s && writesubfont(fd, t->s)<0)){
			close(fd);
			mesg("can't write %s: %r", t->name);
		}
	t->mod = 0;
	close(fd);
	mesg("wrote %s", t->name);
}

void
tpixels(void)
{
	Thing *t;
	Point p, lastp;

	esetcursor(&pixel);
	for(;;){
		buttons(Down);
		if(mouse.buttons != 4)
			break;
		for(t=thing; t; t=t->next){
			lastp = Pt(-1, -1);
			if(ptinrect(mouse.xy, t->r)){
				while(ptinrect(mouse.xy, t->r) && mouse.buttons==4){
					p = realpt(t, mouse.xy);
					if(!eqpt(p, lastp)){
						if(p.y != lastp.y)
							unloadimage(t->b, Rect(t->b->r.min.x, p.y, t->b->r.max.x, p.y+1), data, sizeof data);
						mesg("[%d,%d] = %d=0x%ux", p.x, p.y, value(t->b, p.x), value(t->b, p.x));
						lastp = p;
					}
					mouse = emouse();
				}
				goto Continue;
			}
		}
		mouse = emouse();
    Continue:;
	}
	buttons(Up);
	esetcursor(0);
}

void
tclose1(Thing *t)
{
	Thing *nt;

	if(t == thing)
		thing = t->next;
	else{
		for(nt=thing; nt->next!=t; nt=nt->next)
			;
		nt->next = t->next;
	}
	do
		for(nt=thing; nt; nt=nt->next)
			if(nt->parent == t){
				tclose1(nt);
				break;
			}
	while(nt);
	if(t->s)
		freesubfont(t->s);
	else
		freeimage(t->b);
	free(t->name);
	free(t);
}

void
tclose(Thing *t)
{
	Thing *ct;

	if(t->mod){
		mesg("%s modified", t->name);
		t->mod = 0;
		return;
	}
	/* fiddle to save redrawing unmoved things */
	if(t == thing)
		ct = 0;
	else
		for(ct=thing; ct; ct=ct->next)
			if(ct->next==t || ct->next->parent==t)
				break;
	tclose1(t);
	if(ct)
		ct = ct->next;
	else
		ct = thing;
	redraw(ct);
}

void
tread(Thing *t)
{
	Thing *nt, *new;
	Fontchar *i;
	Rectangle r;
	int nclosed;

	if(t->parent)
		t = t->parent;
	new = tget(t->name, 0);
	if(new == 0)
		return;
	nclosed = 0;
    again:
	for(nt=thing; nt; nt=nt->next)
		if(nt->parent == t){
			if(!rectinrect(nt->b->r, new->b->r)
			|| new->b->depth!=nt->b->depth){
    closeit:
				nclosed++;
				nt->parent = 0;
				tclose1(nt);
				goto again;
			}
			if((t->s==0) != (new->s==0))
				goto closeit;
			if((t->face==0) != (new->face==0))
				goto closeit;
			if(t->s){	/* check same char */
				if(nt->c >= new->s->n)
					goto closeit;
				i = &new->s->info[nt->c];
				r.min.x = i[0].x;
				r.max.x = i[1].x;
				r.min.y = new->b->r.min.y;
				r.max.y = new->b->r.max.y;
				if(!eqrect(r, nt->b->r))
					goto closeit;
			}
			nt->parent = new;
			draw(nt->b, nt->b->r, new->b, nil, nt->b->r.min);
		}
	new->next = t->next;
	if(t == thing)
		thing = new;
	else{
		for(nt=thing; nt->next!=t; nt=nt->next)
			;
		nt->next = new;
	}
	if(t->s)
		freesubfont(t->s);
	else
		freeimage(t->b);
	free(t->name);
	free(t);
	for(nt=thing; nt; nt=nt->next)
		if(nt==new || nt->parent==new)
			if(nclosed == 0)
				drawthing(nt, 0);	/* can draw in place */
			else{
				redraw(nt);	/* must redraw all below */
				break;
			}
}

void
tchar(Thing *t)
{
	char buf[256], *p;
	Rune r;
	ulong c, d;

	if(t->s == 0){
		t = t->parent;
		if(t==0 || t->s==0){
			mesg("not a subfont");
			return;
		}
	}
	if(type(buf, "char (hex or character or hex-hex)") == 0)
		return;
	if(utflen(buf) == 1){
		chartorune(&r, buf);
		c = r;
		d = r;
	}else{
		if(!strchr(hex, buf[0])){
			mesg("illegal hex character");
			return;
		}
		c = strtoul(buf, 0, 16);
		d = c;
		p = utfrune(buf, '-');
		if(p){
			d = strtoul(p+1, 0, 16);
			if(d < c){
				mesg("invalid range");
				return;
			}
		}
	}
	c -= t->off;
	d -= t->off;
	while(c <= d){
		if((long)c<0 || c>=t->s->n){
			mesg("0x%lux not in font %s", c+t->off, t->name);
			return;
		}
		openedit(t, Pt(0, 0), c);
		c++;
	}
}

void
apply(void (*f)(Thing*))
{
	Thing *t;

	esetcursor(&sight);
	buttons(Down);
	if(mouse.buttons == 4)
		for(t=thing; t; t=t->next)
			if(ptinrect(mouse.xy, t->er)){
				buttons(Up);
				f(t);
				break;
			}
	buttons(Up);
	esetcursor(0);
}

int
complement(Image *t)
{
	int i, n;
	uchar *buf;

	n = Dy(t->r)*bytesperline(t->r, t->depth);
	buf = malloc(n);
	if(buf == 0)
		return 0;
	unloadimage(t, t->r, buf, n);
	for(i=0; i<n; i++)
		buf[i] = ~buf[i];
	loadimage(t, t->r, buf, n);
	free(buf);
	return 1;
}

void
copy(void)
{
	Thing *st, *dt, *nt;
	Rectangle sr, dr, fr;
	Image *tmp;
	Point p1, p2;
	int but, up;

	if(!sweep(3, &sr))
		return;
	for(st=thing; st; st=st->next)
		if(rectXrect(sr, st->r))
			break;
	if(st == 0)
		return;
	/* click gives full rectangle */
	if(Dx(sr)<4 && Dy(sr)<4)
		sr = st->r;
	rectclip(&sr, st->r);
	p1 = realpt(st, sr.min);
	p2 = realpt(st, Pt(sr.min.x, sr.max.y));
	up = 0;
	if(p1.x != p2.x){	/* swept across a fold */
   onafold:
		mesg("sweep spans a fold");
		goto Return;
	}
	p2 = realpt(st, sr.max);
	sr.min = p1;
	sr.max = p2;
	fr.min = screenpt(st, sr.min);
	fr.max = screenpt(st, sr.max);
	p1 = subpt(p2, p1);	/* diagonal */
	if(p1.x==0 || p1.y==0)
		return;
	border(screen, fr, -1, values[Blue], ZP);
	esetcursor(&box);
	for(; mouse.buttons==0; mouse=emouse()){
		for(dt=thing; dt; dt=dt->next)
			if(ptinrect(mouse.xy, dt->er))
				break;
		if(up)
			edrawgetrect(insetrect(dr, -Borderwidth), 0);
		up = 0;
		if(dt == 0)
			continue;
		dr.max = screenpt(dt, realpt(dt, mouse.xy));
		dr.min = subpt(dr.max, mulpt(p1, dt->mag));
		if(!rectXrect(dr, dt->r))
			continue;
		edrawgetrect(insetrect(dr, -Borderwidth), 1);
		up = 1;
	}
	/* if up==1, we had a hit */
	esetcursor(0);
	if(up)
		edrawgetrect(insetrect(dr, -Borderwidth), 0);
	but = mouse.buttons;
	buttons(Up);
	if(!up || but!=4)
		goto Return;
	dt = 0;
	for(nt=thing; nt; nt=nt->next)
		if(rectXrect(dr, nt->r)){
			if(dt){
				mesg("ambiguous sweep");
				return;
			}
			dt = nt;
		}
	if(dt == 0)
		goto Return;
	p1 = realpt(dt, dr.min);
	p2 = realpt(dt, Pt(dr.min.x, dr.max.y));
	if(p1.x != p2.x)
		goto onafold;
	p2 = realpt(dt, dr.max);
	dr.min = p1;
	dr.max = p2;

	if(invert){
		tmp = allocimage(display, dr, dt->b->chan, 0, 255);
		if(tmp == 0){
    nomem:
			mesg("can't allocate temporary");
			goto Return;
		}
		draw(tmp, dr, st->b, nil, sr.min);
		if(!complement(tmp))
			goto nomem;
		draw(dt->b, dr, tmp, nil, dr.min);
		freeimage(tmp);
	}else
		draw(dt->b, dr, st->b, nil, sr.min);
	if(dt->parent){
		draw(dt->parent->b, dr, dt->b, nil, dr.min);
		dt = dt->parent;
	}
	drawthing(dt, 0);
	for(nt=thing; nt; nt=nt->next)
		if(nt->parent==dt && rectXrect(dr, nt->b->r)){
			draw(nt->b, dr, dt->b, nil, dr.min);
			drawthing(nt, 0);
		}
	ckinfo(dt, dr);
	dt->mod = 1;

Return:
	/* clear blue box */
	drawthing(st, 0);
}

void
menu(void)
{
	Thing *t;
	char *mod;
	int sel;
	char buf[256];

	sel = emenuhit(3, &mouse, &menu3);
	switch(sel){
	case Mopen:
		if(type(buf, "file")){
			t = tget(buf, 0);
			if(t)
				drawthing(t, 1);
		}
		break;
	case Mwrite:
		apply(twrite);
		break;
	case Mread:
		apply(tread);
		break;
	case Mchar:
		apply(tchar);
		break;
	case Mcopy:
		copy();
		break;
	case Mpixels:
		tpixels();
		break;
	case Mclose:
		apply(tclose);
		break;
	case Mexit:
		mod = 0;
		for(t=thing; t; t=t->next)
			if(t->mod){
				mod = t->name;
				t->mod = 0;
			}
		if(mod){
			mesg("%s modified", mod);
			break;
		}
		esetcursor(&skull);
		buttons(Down);
		if(mouse.buttons == 4){
			buttons(Up);
			exits(0);
		}
		buttons(Up);
		esetcursor(0);
		break;
	}
}
