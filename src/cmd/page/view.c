/*
 * the actual viewer that handles screen stuff
 */

#include <u.h>
#include <libc.h>
#include <9pclient.h>
#include <draw.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <thread.h>
#include <bio.h>
#include <plumb.h>
#include <ctype.h>
#include "page.h"

Document *doc;
Mousectl *mc;
Image *im;
Image *tofree;
int page;
int angle = 0;
int showbottom = 0;		/* on the next showpage, move the image so the bottom is visible. */

Rectangle ulrange;	/* the upper left corner of the image must be in this rectangle */
Point ul;			/* the upper left corner of the image is at this point on the screen */

Point pclip(Point, Rectangle);
Rectangle mkrange(Rectangle screenr, Rectangle imr);
void redraw(Image*);
void plumbproc(void*);

Cursor reading={
	{-1, -1},
	{0xff, 0x80, 0xff, 0x80, 0xff, 0x00, 0xfe, 0x00,
	 0xff, 0x00, 0xff, 0x80, 0xff, 0xc0, 0xef, 0xe0,
	 0xc7, 0xf0, 0x03, 0xf0, 0x01, 0xe0, 0x00, 0xc0,
	 0x03, 0xff, 0x03, 0xff, 0x03, 0xff, 0x03, 0xff, },
	{0x00, 0x00, 0x7f, 0x00, 0x7e, 0x00, 0x7c, 0x00,
	 0x7e, 0x00, 0x7f, 0x00, 0x6f, 0x80, 0x47, 0xc0,
	 0x03, 0xe0, 0x01, 0xf0, 0x00, 0xe0, 0x00, 0x40,
	 0x00, 0x00, 0x01, 0xb6, 0x01, 0xb6, 0x00, 0x00, }
};

Cursor query = {
	{-7,-7},
	{0x0f, 0xf0, 0x1f, 0xf8, 0x3f, 0xfc, 0x7f, 0xfe,
	 0x7c, 0x7e, 0x78, 0x7e, 0x00, 0xfc, 0x01, 0xf8,
	 0x03, 0xf0, 0x07, 0xe0, 0x07, 0xc0, 0x07, 0xc0,
	 0x07, 0xc0, 0x07, 0xc0, 0x07, 0xc0, 0x07, 0xc0, },
	{0x00, 0x00, 0x0f, 0xf0, 0x1f, 0xf8, 0x3c, 0x3c,
	 0x38, 0x1c, 0x00, 0x3c, 0x00, 0x78, 0x00, 0xf0,
	 0x01, 0xe0, 0x03, 0xc0, 0x03, 0x80, 0x03, 0x80,
	 0x00, 0x00, 0x03, 0x80, 0x03, 0x80, 0x00, 0x00, }
};

enum {
	Left = 1,
	Middle = 2,
	Right = 4,

	RMenu = 3,
};

static void
delayfreeimage(Image *m)
{
	if(m == tofree)
		return;
	if(tofree)
		freeimage(tofree);
	tofree = m;
}

void
unhide(void)
{
	USED(nil);
}

int
max(int a, int b)
{
	return a > b ? a : b;
}

int
min(int a, int b)
{
	return a < b ? a : b;
}


char*
menugen(int n)
{
	static char menustr[32];
	char *p;
	int len;

	if(n == doc->npage)
		return "exit";
	if(n > doc->npage)
		return nil;

	if(reverse)
		n = doc->npage-1-n;

	p = doc->pagename(doc, n);
	len = (sizeof menustr)-2;

	if(strlen(p) > len && strrchr(p, '/'))
		p = strrchr(p, '/')+1;
	if(strlen(p) > len)
		p = p+strlen(p)-len;

	strcpy(menustr+1, p);
	if(page == n)
		menustr[0] = '>';
	else
		menustr[0] = ' ';
	return menustr;
}

void
showpage(int page, Menu *m)
{
	if(doc->fwdonly)
		m->lasthit = 0;	/* this page */
	else
		m->lasthit = reverse ? doc->npage-1-page : page;

	setcursor(mc, &reading);
	delayfreeimage(nil);
	im = cachedpage(doc, angle, page);
	if(im == nil)
		wexits(0);
	if(resizing)
		resize(Dx(im->r), Dy(im->r));

	setcursor(mc, nil);
	if(showbottom){
		ul.y = screen->r.max.y - Dy(im->r);
		showbottom = 0;
	}

	if((doc->type == Tgfx) && fitwin)
		fit();
	else{
		redraw(screen);
	 	flushimage(display, 1);
	}
}

char*
writebitmap(void)
{
	char basename[64];
	char name[64+30];
	static char result[200];
	char *p, *q;
	int fd = -1;

	if(im == nil)
		return "no image";

	memset(basename, 0, sizeof basename);
	if(doc->docname)
		strncpy(basename, doc->docname, sizeof(basename)-1);
	else if((p = menugen(page)) && p[0] != '\0')
		strncpy(basename, p+1, sizeof(basename)-1);

	if(basename[0]) {
		if(q = strrchr(basename, '/'))
			q++;
		else
			q = basename;
		if(p = strchr(q, '.'))
			*p = 0;

		memset(name, 0, sizeof name);
		snprint(name, sizeof(name)-1, "%s.%d.bit", q, page+1);
		if(access(name, 0) >= 0) {
			strcat(name, "XXXX");
			fd = mkstemp(name);
		}
		if(fd < 0)
			return "couldn't think of a name for bitmap";
	} else {
		strcpy(name, "bitXXXX");
		mkstemp(name);
		if(fd < 0)
			return "couldn't think of a name for bitmap";
	}

	if(fd < 0) {
		snprint(result, sizeof result, "cannot create %s: %r", name);
		return result;
	}

	if(writeimage(fd, im, 0) < 0) {
		snprint(result, sizeof result, "cannot writeimage: %r");
		close(fd);
		return result;
	}
	close(fd);

	snprint(result, sizeof result, "wrote %s", name);
	return result;
}

static void translate(Point);

static int
showdata(Plumbmsg *msg)
{
	char *s;

	s = plumblookup(msg->attr, "action");
	return s && strcmp(s, "showdata")==0;
}

/* correspond to entries in miditems[] below,
 * changing one means you need to change
 */
enum{
	Restore = 0,
	Zin,
	Fit,
	Rot,
	Upside,
	Empty1,
	Next,
	Prev,
	Zerox,
	Empty2,
	Reverse,
	Del,
	Write,
	Empty3,
	Exit,
};

void
viewer(Document *dd)
{
	int i, fd, n, oldpage;
	int nxt, a;
	Channel *cp;
	Menu menu, midmenu;
	Mouse m;
	Keyboardctl *kc;
	Point dxy, oxy, xy0;
	Rune run;
	Rectangle r;
	int size[2];
	Image *tmp;
	PDFInfo *pdf;
	PSInfo *ps;
	static char *fwditems[] = { "this page", "next page", "exit", 0 };
 	static char *miditems[] = {
 		"orig size",
 		"zoom in",
 		"fit window",
 		"rotate 90",
 		"upside down",
 		"",
 		"next",
 		"prev",
		"zerox",
 		"",
 		"reverse",
 		"discard",
 		"write",
 		"",
 		"quit",
 		0
 	};
	char *s;
	enum {
	    CMouse,
	    CResize,
	    CKeyboard,
	    CPlumb,
	    CN
	};
	Alt alts[CN+1];
	Plumbmsg *pm;

	cp = chancreate(sizeof pm, 0);
	assert(cp);

	doc = dd;    /* save global for menuhit */
	ul = screen->r.min;
	mc = initmouse(nil, screen);
	kc = initkeyboard(nil);
	alts[CMouse].c = mc->c;
	alts[CMouse].v = &m;
	alts[CMouse].op = CHANRCV;
	alts[CResize].c = mc->resizec;
	alts[CResize].v = &size;
	alts[CResize].op = CHANRCV;
	alts[CKeyboard].c = kc->c;
	alts[CKeyboard].v = &run;
	alts[CKeyboard].op = CHANRCV;
	alts[CPlumb].c = cp;
	alts[CPlumb].v = &pm;
	alts[CPlumb].op = CHANNOP;
	alts[CN].op = CHANEND;

	/* XXX: Event */
	if(doc->addpage != nil) {
		alts[CPlumb].op = CHANRCV;
		proccreate(plumbproc, cp, 16384);
	}

	setcursor(mc, &reading);
	r.min = ZP;

	/*
	 * im is a global pointer to the current image.
	 * eventually, i think we will have a layer between
	 * the display routines and the ps/pdf/whatever routines
	 * to perhaps cache and handle images of different
	 * sizes, etc.
	 */
	im = 0;
	page = reverse ? doc->npage-1 : 0;

	if(doc->fwdonly) {
		menu.item = fwditems;
		menu.gen = 0;
		menu.lasthit = 0;
	} else {
		menu.item = 0;
		menu.gen = menugen;
		menu.lasthit = 0;
	}

	midmenu.item = miditems;
	midmenu.gen = 0;
	midmenu.lasthit = Next;

	showpage(page, &menu);
	setcursor(mc, nil);

	nxt = 0;
	for(;;) {
		/*
		 * throughout, if doc->fwdonly is set, we restrict the functionality
		 * a fair amount.  we don't care about doc->npage anymore, and
		 * all that can be done is select the next page.
		 */
		unlockdisplay(display);
		a = alt(alts);
		lockdisplay(display);
		switch(a) {
		case CKeyboard:
			if(run <= 0xFF && isdigit(run)) {
				nxt = nxt*10+run-'0';
				break;
			} else if(run != '\n')
				nxt = 0;
			switch(run) {
			case 'r':	/* reverse page order */
				if(doc->fwdonly)
					break;
				reverse = !reverse;
				menu.lasthit = doc->npage-1-menu.lasthit;

				/*
				 * the theory is that if we are reversing the
				 * document order and are on the first or last
				 * page then we're just starting and really want
		 	 	 * to view the other end.  maybe the if
				 * should be dropped and this should happen always.
				 */
				if(page == 0 || page == doc->npage-1) {
					page = doc->npage-1-page;
					showpage(page, &menu);
				}
				break;
			case 'w':	/* write bitmap of current screen */
				setcursor(mc, &reading);
				s = writebitmap();
				if(s)
					string(screen, addpt(screen->r.min, Pt(5,5)), display->black, ZP,
						display->defaultfont, s);
				setcursor(mc, nil);
				flushimage(display, 1);
				break;
			case 'd':	/* remove image from working set */
				if(doc->rmpage && page < doc->npage) {
					if(doc->rmpage(doc, page) >= 0) {
						if(doc->npage < 0)
							wexits(0);
						if(page >= doc->npage)
							page = doc->npage-1;
						showpage(page, &menu);
					}
				}
				break;
			case 'q':
			case 0x04: /* ctrl-d */
				wexits(0);
			case 'u':
				if(im==nil)
					break;
				setcursor(mc, &reading);
				rot180(im);
				setcursor(mc, nil);
				angle = (angle+180) % 360;
				redraw(screen);
				flushimage(display, 1);
				break;
			case '-':
			case '\b':
			case Kleft:
				if(page > 0 && !doc->fwdonly) {
					--page;
					showpage(page, &menu);
				}
				break;
			case '\n':
				if(nxt) {
					nxt--;
					if(nxt >= 0 && nxt < doc->npage && !doc->fwdonly)
						showpage(page=nxt, &menu);
					nxt = 0;
					break;
				}
				goto Gotonext;
			case Kright:
			case ' ':
			Gotonext:
				if(doc->npage && ++page >= doc->npage && !doc->fwdonly)
					wexits(0);
				showpage(page, &menu);
				break;

			/*
			 * The upper y coordinate of the image is at ul.y in screen->r.
			 * Panning up means moving the upper left corner down.  If the
			 * upper left corner is currently visible, we need to go back a page.
			 */
			case Kup:
				if(screen->r.min.y <= ul.y && ul.y < screen->r.max.y){
					if(page > 0 && !doc->fwdonly){
						--page;
						showbottom = 1;
						showpage(page, &menu);
					}
				} else {
					i = Dy(screen->r)/2;
					if(i > 10)
						i -= 10;
					if(i+ul.y > screen->r.min.y)
						i = screen->r.min.y - ul.y;
					translate(Pt(0, i));
				}
				break;

			/*
			 * If the lower y coordinate is on the screen, we go to the next page.
			 * The lower y coordinate is at ul.y + Dy(im->r).
			 */
			case Kdown:
				i = ul.y + Dy(im->r);
				if(screen->r.min.y <= i && i <= screen->r.max.y){
					ul.y = screen->r.min.y;
					goto Gotonext;
				} else {
					i = -Dy(screen->r)/2;
					if(i < -10)
						i += 10;
					if(i+ul.y+Dy(im->r) <= screen->r.max.y)
						i = screen->r.max.y - Dy(im->r) - ul.y - 1;
					translate(Pt(0, i));
				}
				break;
			default:
				setcursor(mc, &query);
				sleep(1000);
				setcursor(mc, nil);
				break;
			}
			break;

		case CMouse:
			switch(m.buttons){
			case Left:
				oxy = m.xy;
				xy0 = oxy;
				do {
					dxy = subpt(m.xy, oxy);
					oxy = m.xy;
					translate(dxy);
					recv(mc->c, &m);
				} while(m.buttons == Left);
				if(m.buttons) {
					dxy = subpt(xy0, oxy);
					translate(dxy);
				}
				break;

			case Middle:
				if(doc->npage == 0)
					break;

				n = menuhit(Middle, mc, &midmenu, nil);
				if(n == -1)
					break;
				switch(n){
				case Next: 	/* next */
					if(reverse)
						page--;
					else
						page++;
					if(page < 0) {
						if(reverse) return;
						else page = 0;
					}

					if((page >= doc->npage) && !doc->fwdonly)
						return;

					showpage(page, &menu);
					nxt = 0;
					break;
				case Prev:	/* prev */
					if(reverse)
						page++;
					else
						page--;
					if(page < 0) {
						if(reverse) return;
						else page = 0;
					}

					if((page >= doc->npage) && !doc->fwdonly && !reverse)
						return;

					showpage(page, &menu);
					nxt = 0;
					break;
				case Zerox:	/* prev */
					zerox();
					break;
				case Zin:	/* zoom in */
					if (dd->type == Tpdf){		/* pdf */
						pdf = (PDFInfo *) dd->extra;
						if (pdf != nil){
							ppi+= 50;
							setdim(&pdf->gs, Rect(0,0,0,0), ppi, 0);
							showpage(page, &menu);
						}
						break;
					}
					if (dd->type == Tps){		/* ps */
						ps = (PSInfo *) dd->extra;
						if (ps != nil){
							ppi+= 50;
							setdim(&ps->gs, Rect(0,0,0,0), ppi, 0);
							showpage(page, &menu);
						}
						break;
					}
					else{ 	/* image */
						double delta;
						Rectangle r;

						r = getrect(Middle, mc);
						if((rectclip(&r, rectaddpt(im->r, ul)) == 0) ||
							Dx(r) == 0 || Dy(r) == 0)
							break;
						/* use the smaller side to expand */
						if(Dx(r) < Dy(r))
							delta = (double)Dx(im->r)/(double)Dx(r);
						else
							delta = (double)Dy(im->r)/(double)Dy(r);

						setcursor(mc, &reading);
						tmp = xallocimage(display,
								Rect(0, 0, (int)((double)Dx(im->r)*delta), (int)((double)Dy(im->r)*delta)),
								im->chan, 0, DBlack);
						if(tmp == nil) {
							fprint(2, "out of memory during zoom: %r\n");
							wexits("memory");
						}
						resample(im, tmp);
						im = tmp;
						delayfreeimage(tmp);
						setcursor(mc, nil);
						ul = screen->r.min;
						redraw(screen);
						flushimage(display, 1);
						break;
					}
				case Fit:	/* fit */
					/* no op if pdf or ps*/
					if (dd->type == Tgfx){
						fitwin = 1;
						fit();
					}
					break;
				case Rot:	/* rotate 90 */
					angle = (angle+90) % 360;
					showpage(page, &menu);
					break;
				case Upside: 	/* upside-down */
					angle = (angle+180) % 360;
					showpage(page, &menu);
					break;
				case Restore:	/* restore */
					if (dd->type == Tpdf){		/* pdf */
						pdf = (PDFInfo *) dd->extra;
						if (pdf != nil){
							ppi = 100;
							setdim(&pdf->gs, Rect(0,0,0,0), ppi, 0);
						}
						showpage(page, &menu);
						break;
					}
					if (dd->type == Tps){		/* ps */
						ps = (PSInfo *) dd->extra;
						if (ps != nil){
							ppi = 100;
							setdim(&ps->gs, Rect(0,0,0,0), ppi, 0);
						}
						showpage(page, &menu);
						break;
					}
					fitwin = 0;
					showpage(page, &menu);
					break;
				case Reverse:	/* reverse */
					if(doc->fwdonly)
						break;
					reverse = !reverse;
					menu.lasthit = doc->npage-1-menu.lasthit;

					if(page == 0 || page == doc->npage-1) {
						page = doc->npage-1-page;
						showpage(page, &menu);
					}
					break;
				case Write: /* write */
					setcursor(mc, &reading);
					s = writebitmap();
					if(s)
						string(screen, addpt(screen->r.min, Pt(5,5)), display->black, ZP,
							display->defaultfont, s);
					setcursor(mc, nil);
					flushimage(display, 1);
					break;
				case Del: /* delete */
					if(doc->rmpage && page < doc->npage) {
						if(doc->rmpage(doc, page) >= 0) {
							if(doc->npage < 0)
								wexits(0);
							if(page >= doc->npage)
								page = doc->npage-1;
							showpage(page, &menu);
						}
					}
					break;
				case Exit:	/* exit */
					return;
				case Empty1:
				case Empty2:
				case Empty3:
					break;

				};



			case Right:
				if(doc->npage == 0)
					break;

				oldpage = page;
				n = menuhit(RMenu, mc, &menu, nil);
				if(n == -1)
					break;

				if(doc->fwdonly) {
					switch(n){
					case 0:	/* this page */
						break;
					case 1:	/* next page */
						showpage(++page, &menu);
						break;
					case 2:	/* exit */
						return;
					}
					break;
				}

				if(n == doc->npage)
					return;
				else
					page = reverse ? doc->npage-1-n : n;

				if(oldpage != page)
					showpage(page, &menu);
				nxt = 0;
				break;
			}
			break;
		case CResize:
			r = screen->r;
			if(getwindow(display, Refnone) < 0)
				fprint(2,"can't reattach to window");
			ul = addpt(ul, subpt(screen->r.min, r.min));
			redraw(screen);
			flushimage(display, 1);
			break;
		case CPlumb:
			if(pm->ndata <= 0){
				plumbfree(pm);
				break;
			}
			if(showdata(pm)) {
				s = estrdup("/tmp/pageplumbXXXXXXX");
				fd = opentemp(s, ORDWR|ORCLOSE);
				write(fd, pm->data, pm->ndata);
				/* lose fd reference on purpose; the file is open ORCLOSE */
			} else if(pm->data[0] == '/') {
				s = estrdup(pm->data);
			} else {
				s = emalloc(strlen(pm->wdir)+1+pm->ndata+1);
				sprint(s, "%s/%s", pm->wdir, pm->data);
				cleanname(s);
			}
			if((i = doc->addpage(doc, s)) >= 0) {
				page = i;
				unhide();
				showpage(page, &menu);
			}
			free(s);
			plumbfree(pm);
			break;
		}
	}
}

Image *gray;

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

	USED(op);

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

static void
drawdiff(Image *dst, Rectangle bot, Rectangle top, Image *src, Image *mask, Point p, int op)
{
	gendrawdiff(dst, bot, top, src, p, mask, p, op);
}

/*
 * Translate the image in the window by delta.
 */
static void
translate(Point delta)
{
	Point u;
	Rectangle r, or;

	if(im == nil)
		return;

	u = pclip(addpt(ul, delta), ulrange);
	delta = subpt(u, ul);
	if(delta.x == 0 && delta.y == 0)
		return;

	/*
	 * The upper left corner of the image is currently at ul.
	 * We want to move it to u.
	 */
	or = rectaddpt(Rpt(ZP, Pt(Dx(im->r), Dy(im->r))), ul);
	r = rectaddpt(or, delta);

	drawop(screen, r, screen, nil, ul, S);
	ul = u;

	/* fill in gray where image used to be but isn't. */
	drawdiff(screen, insetrect(or, -2), insetrect(r, -2), gray, nil, ZP, S);

	/* fill in black border */
	drawdiff(screen, insetrect(r, -2), r, display->black, nil, ZP, S);

	/* fill in image where it used to be off the screen. */
	if(rectclip(&or, screen->r))
		drawdiff(screen, r, rectaddpt(or, delta), im, nil, im->r.min, S);
	else
		drawop(screen, r, im, nil, im->r.min, S);
	flushimage(display, 1);
}

void
redraw(Image *screen)
{
	Rectangle r;

	if(im == nil)
		return;

	ulrange.max = screen->r.max;
	ulrange.min = subpt(screen->r.min, Pt(Dx(im->r), Dy(im->r)));

	ul = pclip(ul, ulrange);
	drawop(screen, screen->r, im, nil, subpt(im->r.min, subpt(ul, screen->r.min)), S);

	if(im->repl)
		return;

	/* fill in any outer edges */
	/* black border */
	r = rectaddpt(im->r, subpt(ul, im->r.min));
	border(screen, r, -2, display->black, ZP);
	r.min = subpt(r.min, Pt(2,2));
	r.max = addpt(r.max, Pt(2,2));

	/* gray for the rest */
	if(gray == nil) {
		gray = xallocimage(display, Rect(0,0,1,1), RGB24, 1, 0x888888FF);
		if(gray == nil) {
			fprint(2, "g out of memory: %r\n");
			wexits("mem");
		}
	}
	border(screen, r, -4000, gray, ZP);
//	flushimage(display, 0);
}

/* clip p to be in r */
Point
pclip(Point p, Rectangle r)
{
	if(p.x < r.min.x)
		p.x = r.min.x;
	else if(p.x >= r.max.x)
		p.x = r.max.x-1;

	if(p.y < r.min.y)
		p.y = r.min.y;
	else if(p.y >= r.max.y)
		p.y = r.max.y-1;

	return p;
}

/*
 * resize is perhaps a misnomer.
 * this really just grows the window to be at least dx across
 * and dy high.  if the window hits the bottom or right edge,
 * it is backed up until it hits the top or left edge.
 */
void
resize(int dx, int dy)
{
	static Rectangle sr;
	Rectangle r, or;

	r = screen->r;
	if(Dx(sr)*Dy(sr) == 0) {
		sr = screenrect();
		/* Start with the size of the first image */
		r.max.x = r.min.x;
		r.max.y = r.min.y;
	}

	if(Dx(r) >= dx && Dy(r) >= dy)
		return;

	or = r;

	r.max.x = max(r.min.x+dx, r.max.x);
	r.max.y = max(r.min.y+dy, r.max.y);
	if(r.max.x > sr.max.x){
		if(Dx(r) > Dx(sr)){
			r.min.x = 0;
			r.max.x = sr.max.x;
		}else
			r = rectaddpt(r, Pt(sr.max.x-r.max.x, 0));
	}
	if(r.max.y > sr.max.y){
		if(Dy(r) > Dy(sr)){
			r.min.y = 0;
			r.max.y = sr.max.y;
		}else
			r = rectaddpt(r, Pt(0, sr.max.y-r.max.y));
	}

	/*
	 * Sometimes we can't actually grow the window big enough,
	 * and resizing it to the same shape makes it flash.
	 */
	if(Dx(r) == Dx(or) && Dy(r) == Dy(or))
		return;

	drawresizewindow(r);
}

/*
 * If we allocimage after a resize but before flushing the draw buffer,
 * we won't have seen the reshape event, and we won't have called
 * getwindow, and allocimage will fail.  So we flushimage before every alloc.
 */
Image*
xallocimage(Display *d, Rectangle r, ulong chan, int repl, ulong val)
{
	flushimage(display, 0);
	return allocimage(d, r, chan, repl, val);
}

void
plumbproc(void *c)
{
	Channel *cp;
	CFid *fd;

	cp = c;
	fd = plumbopenfid("image", OREAD|OCEXEC);
	if(fd == nil) {
		fprint(2, "Cannot connect to the plumber");
		threadexits("plumber");
	}
	for(;;) {
		send(cp, plumbrecvfid(fd));
	}
}

/* XXX: This function is ugly and hacky. There may be a better way... or not */
Rectangle
screenrect(void)
{
	int fd[3], pfd[2];
	int n, w, h;
	char buf[64];
	char *p, *pr;

	if(pipe(pfd) < 0)
		wexits("pipe failed");

	fd[0] = open("/dev/null", OREAD);
	fd[1] = pfd[1];
	fd[2] = dup(2, -1);
	if(threadspawnl(fd, "rc", "rc", "-c", "xdpyinfo | grep 'dimensions:'", nil) == -1)
		wexits("threadspawnl failed");

	if((n = read(pfd[0], buf, 63)) <= 0)
		wexits("read xdpyinfo failed");
	close(fd[0]);

	buf[n] = '\0';
	for(p = buf; *p; p++)
		if(*p >= '0' && *p <= '9') break;
	if(*p == '\0')
		wexits("xdpyinfo parse failed");

	w = strtoul(p, &pr, 10);
	if(p == pr || *pr == '\0' || *(++pr) == '\0')
		wexits("xdpyinfo parse failed");
	h = strtoul(pr, &p, 10);
	if(p == pr)
		wexits("xdpyinfo parse failed");

	return Rect(0, 0, w, h);
}

void
zerox(void)
{
	int pfd[2];
	int fd[3];

	pipe(pfd);
	fd[0] = pfd[0];
	fd[1] = dup(1, -1);
	fd[2] = dup(2, -1);
	threadspawnl(fd, "page", "page", "-R", nil);

	writeimage(pfd[1], im, 0);
	close(pfd[1]);
}

void
fit()
{
	double delta;
	Rectangle r;
	Image* tmp;

	delta = (double)Dx(screen->r)/(double)Dx(im->r);
	if((double)Dy(im->r)*delta > Dy(screen->r))
		delta = (double)Dy(screen->r)/(double)Dy(im->r);
	r = Rect(0, 0, (int)((double)Dx(im->r)*delta), (int)((double)Dy(im->r)*delta));
	setcursor(mc, &reading);
	tmp = xallocimage(display, r, im->chan, 0, DBlack);
	if(tmp == nil) {
		fprint(2, "out of memory during fit: %r\n");
		wexits("memory");
	}
	resample(im, tmp);
	im = tmp;
	delayfreeimage(tmp);
	setcursor(mc, nil);
	ul = screen->r.min;
	redraw(screen);
	flushimage(display, 1);
}
