/*
This code was taken from 9front repository (https://code.9front.org/hg/plan9front).
It is subject to license from 9front, below is a reproduction of the license.

Copyright (c) 20XX 9front

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>

/* additional libdraw function needed - defined here to avoid API change */
extern int             eenter(char*, char*, int, Mouse*);

char *filename;
int zoom = 1;
int brush = 1;
Point spos;		/* position on screen */
Point cpos;		/* position on canvas */
Image *canvas;
Image *ink;
Image *back;
Image *pal[16];		/* palette */
Rectangle palr;		/* palette rect on screen */
Rectangle penr;		/* pen size rect on screen */

enum {
	NBRUSH = 10+1,
};

int nundo = 0;
Image *undo[1024];

int c64[] = {		/* c64 color palette */
	0x000000,
	0xFFFFFF,
	0x68372B,
	0x70A4B2,
	0x6F3D86,
	0x588D43,
	0x352879,
	0xB8C76F,
	0x6F4F25,
	0x433900,
	0x9A6759,
	0x444444,
	0x6C6C6C,
	0x9AD284,
	0x6C5EB5,
	0x959595,
};

/*
 * get bounding rectnagle for stroke from r.min to r.max with
 * specified brush (size).
 */
static Rectangle
strokerect(Rectangle r, int brush)
{
	r = canonrect(r);
	return Rect(r.min.x-brush, r.min.y-brush, r.max.x+brush+1, r.max.y+brush+1);
}

/*
 * draw stroke from r.min to r.max to dst with color ink and
 * brush (size).
 */
static void
strokedraw(Image *dst, Rectangle r, Image *ink, int brush)
{
	if(!eqpt(r.min, r.max))
		line(dst, r.min, r.max, Enddisc, Enddisc, brush, ink, ZP);
	fillellipse(dst, r.max, brush, brush, ink, ZP);
}

/*
 * A draw operation that touches only the area contained in bot but not in top.
 * mp and sp get aligned with bot.min.
 */
static void
gendrawdiff(Image *dst, Rectangle bot, Rectangle top,
	Image *src, Point sp, Image *mask, Point mp, int op)
{
	Rectangle r;
	Point origin;
	Point delta;

	if(Dx(bot)*Dy(bot) == 0)
		return;

	/* no points in bot - top */
	if(rectinrect(bot, top))
		return;

	/* bot - top ≡ bot */
	if(Dx(top)*Dy(top)==0 || rectXrect(bot, top)==0){
		gendrawop(dst, bot, src, sp, mask, mp, op);
		return;
	}

	origin = bot.min;
	/* split bot into rectangles that don't intersect top */
	/* left side */
	if(bot.min.x < top.min.x){
		r = Rect(bot.min.x, bot.min.y, top.min.x, bot.max.y);
		delta = subpt(r.min, origin);
		gendrawop(dst, r, src, addpt(sp, delta), mask, addpt(mp, delta), op);
		bot.min.x = top.min.x;
	}

	/* right side */
	if(bot.max.x > top.max.x){
		r = Rect(top.max.x, bot.min.y, bot.max.x, bot.max.y);
		delta = subpt(r.min, origin);
		gendrawop(dst, r, src, addpt(sp, delta), mask, addpt(mp, delta), op);
		bot.max.x = top.max.x;
	}

	/* top */
	if(bot.min.y < top.min.y){
		r = Rect(bot.min.x, bot.min.y, bot.max.x, top.min.y);
		delta = subpt(r.min, origin);
		gendrawop(dst, r, src, addpt(sp, delta), mask, addpt(mp, delta), op);
		bot.min.y = top.min.y;
	}

	/* bottom */
	if(bot.max.y > top.max.y){
		r = Rect(bot.min.x, top.max.y, bot.max.x, bot.max.y);
		delta = subpt(r.min, origin);
		gendrawop(dst, r, src, addpt(sp, delta), mask, addpt(mp, delta), op);
		bot.max.y = top.max.y;
	}
}

int
alphachan(ulong chan)
{
	for(; chan; chan >>= 8)
		if(TYPE(chan) == CAlpha)
			return 1;
	return 0;
}

void
zoomdraw(Image *d, Rectangle r, Rectangle top, Image *b, Image *s, Point sp, int f)
{
	Rectangle dr;
	Image *t;
	Point a;
	int w;

	a = ZP;
	if(r.min.x < d->r.min.x){
		sp.x += (d->r.min.x - r.min.x)/f;
		a.x = (d->r.min.x - r.min.x)%f;
		r.min.x = d->r.min.x;
	}
	if(r.min.y < d->r.min.y){
		sp.y += (d->r.min.y - r.min.y)/f;
		a.y = (d->r.min.y - r.min.y)%f;
		r.min.y = d->r.min.y;
	}
	rectclip(&r, d->r);
	w = s->r.max.x - sp.x;
	if(w > Dx(r))
		w = Dx(r);
	dr = r;
	dr.max.x = dr.min.x+w;
	if(!alphachan(s->chan))
		b = nil;
	if(f <= 1){
		if(b) gendrawdiff(d, dr, top, b, sp, nil, ZP, SoverD);
		gendrawdiff(d, dr, top, s, sp, nil, ZP, SoverD);
		return;
	}
	if((t = allocimage(display, dr, s->chan, 0, 0)) == nil)
		return;
	for(; dr.min.y < r.max.y; dr.min.y++){
		dr.max.y = dr.min.y+1;
		draw(t, dr, s, nil, sp);
		if(++a.y == f){
			a.y = 0;
			sp.y++;
		}
	}
	dr = r;
	for(sp=dr.min; dr.min.x < r.max.x; sp.x++){
		dr.max.x = dr.min.x+1;
		if(b) gendrawdiff(d, dr, top, b, sp, nil, ZP, SoverD);
		gendrawdiff(d, dr, top, t, sp, nil, ZP, SoverD);
		for(dr.min.x++; ++a.x < f && dr.min.x < r.max.x; dr.min.x++){
			dr.max.x = dr.min.x+1;
			gendrawdiff(d, dr, top, d, Pt(dr.min.x-1, dr.min.y), nil, ZP, SoverD);
		}
		a.x = 0;
	}
	freeimage(t);
}

Point
s2c(Point p){
	p = subpt(p, spos);
	if(p.x < 0) p.x -= zoom-1;
	if(p.y < 0) p.y -= zoom-1;
	return addpt(divpt(p, zoom), cpos);
}

Point
c2s(Point p){
	return addpt(mulpt(subpt(p, cpos), zoom), spos);
}

Rectangle
c2sr(Rectangle r){
	return Rpt(c2s(r.min), c2s(r.max));
}

void
update(Rectangle *rp){
	if(canvas==nil)
		draw(screen, screen->r, back, nil, ZP);
	else {
		if(rp == nil)
			rp = &canvas->r;
		gendrawdiff(screen, screen->r, c2sr(canvas->r), back, ZP, nil, ZP, SoverD);
		zoomdraw(screen, c2sr(*rp), ZR, back, canvas, rp->min, zoom);
	}
	flushimage(display, 1);
}

void
expand(Rectangle r)
{
	Rectangle nr;
	Image *tmp;

	if(canvas==nil){
		if((canvas = allocimage(display, r, screen->chan, 0, DNofill)) == nil)
			sysfatal("allocimage: %r");
		draw(canvas, canvas->r, back, nil, ZP);
		return;
	}
	nr = canvas->r;
	combinerect(&nr, r);
	if(eqrect(nr, canvas->r))
		return;
	if((tmp = allocimage(display, nr, canvas->chan, 0, DNofill)) == nil)
		return;
	draw(tmp, canvas->r, canvas, nil, canvas->r.min);
	gendrawdiff(tmp, tmp->r, canvas->r, back, ZP, nil, ZP, SoverD);
	freeimage(canvas);
	canvas = tmp;
}

void
save(Rectangle r, int mark)
{
	Image *tmp;
	int x;

	if(mark){
		x = nundo++ % nelem(undo);
		if(undo[x])
			freeimage(undo[x]);
		undo[x] = nil;
	}
	if(canvas==nil || nundo<0)
		return;
	if(!rectclip(&r, canvas->r))
		return;
	if((tmp = allocimage(display, r, canvas->chan, 0, DNofill)) == nil)
		return;
	draw(tmp, r, canvas, nil, r.min);
	x = nundo++ % nelem(undo);
	if(undo[x])
		freeimage(undo[x]);
	undo[x] = tmp;
}

void
restore(int n)
{
	Image *tmp;
	int x;

	while(nundo > 0){
		if(n-- == 0)
			return;
		x = --nundo % nelem(undo);
		if((tmp = undo[x]) == nil)
			return;
		undo[x] = nil;
		if(canvas == nil || canvas->chan != tmp->chan){
			freeimage(canvas);
			canvas = tmp;
			update(nil);
		} else {
			expand(tmp->r);
			draw(canvas, tmp->r, tmp, nil, tmp->r.min);
			update(&tmp->r);
			freeimage(tmp);
		}
	}
}

typedef struct {
	Rectangle	r;
	Rectangle	r0;
	Image*		dst;

	int		yscan;	/* current scanline */
	int		wscan;	/* bscan width in bytes */
	Image*		iscan;	/* scanline image */
	uchar*		bscan;	/* scanline buffer */

	int		nmask;	/* size of bmask in bytes */
	int		wmask;	/* width of bmask in bytes */
	Image*		imask;	/* mask image */
	uchar*		bmask;	/* mask buffer */

	int		ncmp;
	uchar		bcmp[4];
} Filldata;

void
fillscan(Filldata *f, Point p0)
{
	int x, y;
	uchar *b;

	x = p0.x;
	y = p0.y;
	b = f->bmask + y*f->wmask;
	if(b[x/8] & 0x80>>(x%8))
		return;

	if(f->yscan != y){
		draw(f->iscan, f->iscan->r, f->dst, nil, Pt(f->r.min.x, f->r.min.y+y));
		if(unloadimage(f->iscan, f->iscan->r, f->bscan, f->wscan) < 0)
			return;
		f->yscan = y;
	}

	for(x = p0.x; x >= 0; x--){
		if(memcmp(f->bscan + x*f->ncmp, f->bcmp, f->ncmp))
			break;
		b[x/8] |= 0x80>>(x%8);
	}
	for(x = p0.x+1; x < f->r0.max.x; x++){
		if(memcmp(f->bscan + x*f->ncmp, f->bcmp, f->ncmp))
			break;
		b[x/8] |= 0x80>>(x%8);
	}

	y = p0.y-1;
	if(y >= 0){
		for(x = p0.x; x >= 0; x--){
			if((b[x/8] & 0x80>>(x%8)) == 0)
				break;
			fillscan(f, Pt(x, y));
		}
		for(x = p0.x+1; x < f->r0.max.x; x++){
			if((b[x/8] & 0x80>>(x%8)) == 0)
				break;
			fillscan(f, Pt(x, y));
		}
	}

	y = p0.y+1;
	if(y < f->r0.max.y){
		for(x = p0.x; x >= 0; x--){
			if((b[x/8] & 0x80>>(x%8)) == 0)
				break;
			fillscan(f, Pt(x, y));
		}
		for(x = p0.x+1; x < f->r0.max.x; x++){
			if((b[x/8] & 0x80>>(x%8)) == 0)
				break;
			fillscan(f, Pt(x, y));
		}
	}
}

void
floodfill(Image *dst, Rectangle r, Point p, Image *src)
{
	Filldata f;

	if(!rectclip(&r, dst->r))
		return;
	if(!ptinrect(p, r))
		return;
	memset(&f, 0, sizeof(f));
	f.dst = dst;
	f.r = r;
	f.r0 = rectsubpt(r, r.min);
	f.wmask = bytesperline(f.r0, 1);
	f.nmask = f.wmask*f.r0.max.y;
	if((f.bmask = mallocz(f.nmask, 1)) == nil)
		goto out;
	if((f.imask = allocimage(display, f.r0, GREY1, 0, DNofill)) == nil)
		goto out;

	r = f.r0;
	r.max.y = 1;
	if((f.iscan = allocimage(display, r, RGB24, 0, DNofill)) == nil)
		goto out;
	f.yscan = -1;
	f.wscan = bytesperline(f.iscan->r, f.iscan->depth);
	if((f.bscan = mallocz(f.wscan, 0)) == nil)
		goto out;

	r = Rect(0,0,1,1);
	f.ncmp = (f.iscan->depth+7) / 8;
	draw(f.iscan, r, dst, nil, p);
	if(unloadimage(f.iscan, r, f.bcmp, sizeof(f.bcmp)) < 0)
		goto out;

	fillscan(&f, subpt(p, f.r.min));

	loadimage(f.imask, f.imask->r, f.bmask, f.nmask);
	draw(f.dst, f.r, src, f.imask, f.imask->r.min);
out:
	free(f.bmask);
	free(f.bscan);
	if(f.iscan)
		freeimage(f.iscan);
	if(f.imask)
		freeimage(f.imask);
}

void
translate(Point d)
{
	Rectangle r, nr;

	if(canvas==nil || d.x==0 && d.y==0)
		return;
	r = c2sr(canvas->r);
	nr = rectaddpt(r, d);
	rectclip(&r, screen->clipr);
	draw(screen, rectaddpt(r, d), screen, nil, r.min);
	zoomdraw(screen, nr, rectaddpt(r, d), back, canvas, canvas->r.min, zoom);
	gendrawdiff(screen, screen->r, nr, back, ZP, nil, ZP, SoverD);
	spos = addpt(spos, d);
	flushimage(display, 1);
}

void
setzoom(Point o, int z)
{
	if(z < 1)
		return;
	cpos = s2c(o);
	spos = o;
	zoom = z;
	update(nil);
}

void
center(void)
{
	cpos = ZP;
	if(canvas)
		cpos = addpt(canvas->r.min,
			divpt(subpt(canvas->r.max, canvas->r.min), 2));
	spos = addpt(screen->r.min,
		divpt(subpt(screen->r.max, screen->r.min), 2));
	update(nil);
}

void
drawpal(void)
{
	Rectangle r, rr;
	int i;

	r = screen->r;
	r.min.y = r.max.y - 20;
	replclipr(screen, 0, r);

	penr = r;
	penr.min.x = r.max.x - NBRUSH*Dy(r);

	palr = r;
	palr.max.x = penr.min.x;

	r = penr;
	draw(screen, r, back, nil, ZP);
	for(i=0; i<NBRUSH; i++){
		r.max.x = penr.min.x + (i+1)*Dx(penr) / NBRUSH;
		rr = r;
		if(i == brush)
			rr.min.y += Dy(r)/3;
		if(i == NBRUSH-1){
			/* last is special brush for fill draw */
			draw(screen, rr, ink, nil, ZP);
		} else {
			rr.min = addpt(rr.min, divpt(subpt(rr.max, rr.min), 2));
			rr.max = rr.min;
			strokedraw(screen, rr, ink, i);
		}
		r.min.x = r.max.x;
	}

	r = palr;
	for(i=1; i<=nelem(pal); i++){
		r.max.x = palr.min.x + i*Dx(palr) / nelem(pal);
		rr = r;
		if(ink == pal[i-1])
			rr.min.y += Dy(r)/3;
		draw(screen, rr, pal[i-1], nil, ZP);
		gendrawdiff(screen, r, rr, back, ZP, nil, ZP, SoverD);
		r.min.x = r.max.x;
	}

	r = screen->r;
	r.max.y -= Dy(palr);
	replclipr(screen, 0, r);
}

int
hitpal(Mouse m)
{
	if(ptinrect(m.xy, penr)){
		if(m.buttons & 7){
			brush = ((m.xy.x - penr.min.x) * NBRUSH) / Dx(penr);
			drawpal();
		}
		return 1;
	}
	if(ptinrect(m.xy, palr)){
		Image *col;

		col = pal[(m.xy.x - palr.min.x) * nelem(pal) / Dx(palr)];
		switch(m.buttons & 7){
		case 1:
			ink = col;
			drawpal();
			break;
		case 2:
			back = col;
			drawpal();
			update(nil);
			break;
		}
		return 1;
	}
	return 0;
}

void
catch(void * _, char *msg)
{
	USED(_);
	if(strstr(msg, "closed pipe"))
		noted(NCONT);
	noted(NDFLT);
}

int
pipeline(char *fmt, ...)
{
	char buf[1024];
	va_list a;
	int p[2];

	va_start(a, fmt);
	vsnprint(buf, sizeof(buf), fmt, a);
	va_end(a);
	if(pipe(p) < 0)
		return -1;
	switch(rfork(RFPROC|RFMEM|RFFDG|RFNOTEG)){ // RFEND not available in libc port
	case -1:
		close(p[0]);
		close(p[1]);
		return -1;
	case 0:
		close(p[1]);
		dup(p[0], 0);
		dup(p[0], 1);
		close(p[0]);
		execl("/bin/rc", "rc", "-c", buf, nil);
		exits("exec");
	}
	close(p[0]);
	return p[1];
}

void
usage(void)
{
	fprint(2, "usage: %s [ file ]\n", argv0);
	exits("usage");
}

void
main(int argc, char *argv[])
{
	char *s, buf[1024];
	Rectangle r;
	Image *img;
	int i, fd;
	Event e;
	Mouse m;
	Point p, d;

	ARGBEGIN {
	default:
		usage();
	} ARGEND;

	if(argc == 1)
		filename = strdup(argv[0]);
	else if(argc != 0)
		usage();

	if(initdraw(0, 0, "paint") < 0)
		sysfatal("initdraw: %r");

	if(filename){
		if((fd = open(filename, OREAD)) < 0)
			sysfatal("open: %r");
		if((canvas = readimage(display, fd, 0)) == nil)
			sysfatal("readimage: %r");
		close(fd);
	}

	/* palette initialization */
	for(i=0; i<nelem(pal); i++){
		pal[i] = allocimage(display, Rect(0, 0, 1, 1), RGB24, 1,
			c64[i % nelem(c64)]<<8 | 0xFF);
		if(pal[i] == nil)
			sysfatal("allocimage: %r");
	}
	ink = pal[0];
	back = pal[1];
	drawpal();
	center();

	einit(Emouse | Ekeyboard);

	notify(catch);
	for(;;) {
		switch(event(&e)){
		case Emouse:
			if(hitpal(e.mouse))
				continue;

			img = ink;
			switch(e.mouse.buttons & 7){
			case 2:
				img = back;
				/* no break */
			case 1:
				p = s2c(e.mouse.xy);
				if(brush == NBRUSH-1){
					/* flood fill brush */
					if(canvas == nil || !ptinrect(p, canvas->r)){
						back = img;
						drawpal();
						update(nil);
						break;
					}
					r = canvas->r;
					save(r, 1);
					floodfill(canvas, r, p, img);
					update(&r);

					/* wait for mouse release */
					while(event(&e) == Emouse && (e.mouse.buttons & 7) != 0)
						;
					break;
				}
				r = strokerect(Rpt(p, p), brush);
				expand(r);
				save(r, 1);
				strokedraw(canvas, Rpt(p, p), img, brush);
				update(&r);
				for(;;){
					m = e.mouse;
					if(event(&e) != Emouse)
						break;
					if((e.mouse.buttons ^ m.buttons) & 7)
						break;
					d = s2c(e.mouse.xy);
					if(eqpt(d, p))
						continue;
					r = strokerect(Rpt(p, d), brush);
					expand(r);
					save(r, 0);
					strokedraw(canvas, Rpt(p, d), img, brush);
					update(&r);
					p = d;
				}
				break;
			case 4:
				for(;;){
					m = e.mouse;
					if(event(&e) != Emouse)
						break;
					if((e.mouse.buttons & 7) != 4)
						break;
					translate(subpt(e.mouse.xy, m.xy));
				}
				break;
			}
			break;
		case Ekeyboard:
			switch(e.kbdc){
			case Kesc:
				zoom = 1;
				center();
				break;
			case '+':
				if(zoom < 0x1000)
					setzoom(e.mouse.xy, zoom*2);
				break;
			case '-':
				if(zoom > 1)
					setzoom(e.mouse.xy, zoom/2);
				break;
			case 'c':
				if(canvas == nil)
					break;
				save(canvas->r, 1);
				freeimage(canvas);
				canvas = nil;
				update(nil);
				break;
			case 'u':
				restore(16);
				break;
			case 'f':
				brush = NBRUSH-1;
				drawpal();
				break;
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				brush = e.kbdc - '0';
				drawpal();
				break;
			default:
				if(e.kbdc == Kdel)
					e.kbdc = 'q';
				buf[0] = 0;
				if(filename && (e.kbdc == 'r' || e.kbdc == 'w'))
					snprint(buf, sizeof(buf), "%C %s", e.kbdc, filename);
				else if(e.kbdc > 0x20 && e.kbdc < 0x7f)
					snprint(buf, sizeof(buf), "%C", e.kbdc);
				if(eenter("Cmd", buf, sizeof(buf), &e.mouse) <= 0)
					break;
				if(strcmp(buf, "q") == 0)
					exits(nil);
				s = buf+1;
				while(*s == ' ' || *s == '\t')
					s++;
				if(*s == 0)
					break;
				switch(buf[0]){
				case 'r':
					if((fd = open(s, OREAD)) < 0){
					Error:
						snprint(buf, sizeof(buf), "%r");
						eenter(buf, nil, 0, &e.mouse);
						break;
					}
					free(filename);
					filename = strdup(s);
				Readimage:
					unlockdisplay(display);
					img = readimage(display, fd, 1);
					close(fd);
					lockdisplay(display);
					if(img == nil){
						werrstr("readimage: %r");
						goto Error;
					}
					if(canvas){
						save(canvas->r, 1);
						freeimage(canvas);
					}
					canvas = img;
					center();
					break;
				case 'w':
					if((fd = create(s, OWRITE, 0660)) < 0)
						goto Error;
					free(filename);
					filename = strdup(s);
				Writeimage:
					if(canvas)
					if(writeimage(fd, canvas, 0) < 0){
						close(fd);
						werrstr("writeimage: %r");
						goto Error;
					}
					close(fd);
					break;
				case '<':
					if((fd = pipeline("%s", s)) < 0)
						goto Error;
					goto Readimage;
				case '>':
					if((fd = pipeline("%s", s)) < 0)
						goto Error;
					goto Writeimage;
				case '|':
					if(canvas == nil)
						break;
					if((fd = pipeline("%s", s)) < 0)
						goto Error;
					switch(rfork(RFMEM|RFPROC|RFFDG)){
					case -1:
						close(fd);
						werrstr("rfork: %r");
						goto Error;
					case 0:
						writeimage(fd, canvas, 1);
						exits(nil);
					}
					goto Readimage;
				}
				break;
			}
			break;
		}
	}
}

void
eresized(int _)
{
	USED(_);
	if(getwindow(display, Refnone) < 0)
		sysfatal("resize failed");
	drawpal();
	update(nil);
}
