/*
 * the actual viewer that handles screen stuff
 */

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <cursor.h>
#include <cursor.h>
#include <event.h>
#include <bio.h>
#include <plumb.h>
#include <ctype.h>
#include <keyboard.h>
#include "page.h"

Document *doc;
Image *im;
int page;
int angle = 0;
int showbottom = 0;		/* on the next showpage, move the image so the bottom is visible. */

Rectangle ulrange;	/* the upper left corner of the image must be in this rectangle */
Point ul;			/* the upper left corner of the image is at this point on the screen */

Point pclip(Point, Rectangle);
Rectangle mkrange(Rectangle screenr, Rectangle imr);
void redraw(Image*);

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

	RMenu = 3
};

void
unhide(void)
{
	static int wctl = -1;

	if(wctl < 0)
		wctl = open("/dev/wctl", OWRITE);
	if(wctl < 0)
		return;

	write(wctl, "unhide", 6);
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
	Image *tmp;

	if(doc->fwdonly)
		m->lasthit = 0;	/* this page */
	else
		m->lasthit = reverse ? doc->npage-1-page : page;
	
	esetcursor(&reading);
	freeimage(im);
	if((page < 0 || page >= doc->npage) && !doc->fwdonly){
		im = nil;
		return;
	}
	im = doc->drawpage(doc, page);
	if(im == nil) {
		if(doc->fwdonly)	/* this is how we know we're out of pages */
			wexits(0);

		im = xallocimage(display, Rect(0,0,50,50), GREY1, 1, DBlack);
		if(im == nil) {
			fprint(2, "out of memory: %r\n");
			wexits("memory");
		}
		string(im, ZP, display->white, ZP, display->defaultfont, "?");
	}else if(resizing){
		resize(Dx(im->r), Dy(im->r));
	}
	if(im->r.min.x > 0 || im->r.min.y > 0) {
		tmp = xallocimage(display, Rect(0, 0, Dx(im->r), Dy(im->r)), im->chan, 0, DNofill);
		if(tmp == nil) {
			fprint(2, "out of memory during showpage: %r\n");
			wexits("memory");
		}
		drawop(tmp, tmp->r, im, nil, im->r.min, S);
		freeimage(im);
		im = tmp;
	}

	switch(angle){
	case 90:
		im = rot90(im);
		break;
	case 180:
		rot180(im);
		break;
	case 270:
		im = rot270(im);
		break;
	}

	esetcursor(nil);
	if(showbottom){
		ul.y = screen->r.max.y - Dy(im->r);
		showbottom = 0;
	}

	redraw(screen);
	flushimage(display, 1);
}

char*
writebitmap(void)
{
	char basename[64];
	char name[64+30];
	static char result[200];
	char *p, *q;
	int fd;

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
			mktemp(name);
		}
		if(access(name, 0) >= 0)
			return "couldn't think of a name for bitmap";
	} else {
		strcpy(name, "bitXXXX");
		mktemp(name);
		if(access(name, 0) >= 0) 
			return "couldn't think of a name for bitmap";
	}

	if((fd = create(name, OWRITE, 0666)) < 0) {
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
	Exit
};
 
void
viewer(Document *dd)
{
	int i, fd, n, oldpage;
	int nxt;
	Menu menu, midmenu;
	Mouse m;
	Event e;
	Point dxy, oxy, xy0;
	Rectangle r;
	Image *tmp;
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
	enum { Eplumb = 4 };
	Plumbmsg *pm;

	doc = dd;    /* save global for menuhit */
	ul = screen->r.min;
	einit(Emouse|Ekeyboard);
	if(doc->addpage != nil)
		eplumb(Eplumb, "image");

	esetcursor(&reading);
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
	esetcursor(nil);

	nxt = 0;
	for(;;) {
		/*
		 * throughout, if doc->fwdonly is set, we restrict the functionality
		 * a fair amount.  we don't care about doc->npage anymore, and
		 * all that can be done is select the next page.
		 */
		switch(eread(Emouse|Ekeyboard|Eplumb, &e)){
		case Ekeyboard:
			if(e.kbdc <= 0xFF && isdigit(e.kbdc)) {
				nxt = nxt*10+e.kbdc-'0';
				break;
			} else if(e.kbdc != '\n')
				nxt = 0;
			switch(e.kbdc) {
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
				esetcursor(&reading);
				s = writebitmap();
				if(s)
					string(screen, addpt(screen->r.min, Pt(5,5)), display->black, ZP,
						display->defaultfont, s);
				esetcursor(nil);
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
				esetcursor(&reading);
				rot180(im);
				esetcursor(nil);
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
				esetcursor(&query);
				sleep(1000);
				esetcursor(nil);
				break;	
			}
			break;

		case Emouse:
			m = e.mouse;
			switch(m.buttons){
			case Left:
				oxy = m.xy;
				xy0 = oxy;
				do {
					dxy = subpt(m.xy, oxy);
					oxy = m.xy;	
					translate(dxy);
					m = emouse();
				} while(m.buttons == Left);
				if(m.buttons) {
					dxy = subpt(xy0, oxy);
					translate(dxy);
				}
				break;
	
			case Middle:
				if(doc->npage == 0)
					break;

				n = emenuhit(Middle, &m, &midmenu);
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
					{
						double delta;
						Rectangle r;

						r = egetrect(Middle, &m);
						if((rectclip(&r, rectaddpt(im->r, ul)) == 0) ||
							Dx(r) == 0 || Dy(r) == 0)
							break;
						/* use the smaller side to expand */
						if(Dx(r) < Dy(r))
							delta = (double)Dx(im->r)/(double)Dx(r);
						else
							delta = (double)Dy(im->r)/(double)Dy(r);

						esetcursor(&reading);
						tmp = xallocimage(display, 
								Rect(0, 0, (int)((double)Dx(im->r)*delta), (int)((double)Dy(im->r)*delta)), 
								im->chan, 0, DBlack);
						if(tmp == nil) {
							fprint(2, "out of memory during zoom: %r\n");
							wexits("memory");
						}
						resample(im, tmp);
						freeimage(im);
						im = tmp;
						esetcursor(nil);
						ul = screen->r.min;
						redraw(screen);
						flushimage(display, 1);
						break;
					}
				case Fit:	/* fit */
					{
						double delta;
						Rectangle r;
						
						delta = (double)Dx(screen->r)/(double)Dx(im->r);
						if((double)Dy(im->r)*delta > Dy(screen->r))
							delta = (double)Dy(screen->r)/(double)Dy(im->r);

						r = Rect(0, 0, (int)((double)Dx(im->r)*delta), (int)((double)Dy(im->r)*delta));
						esetcursor(&reading);
						tmp = xallocimage(display, r, im->chan, 0, DBlack);
						if(tmp == nil) {
							fprint(2, "out of memory during fit: %r\n");
							wexits("memory");
						}
						resample(im, tmp);
						freeimage(im);
						im = tmp;
						esetcursor(nil);
						ul = screen->r.min;
						redraw(screen);
						flushimage(display, 1);
						break;
					}
				case Rot:	/* rotate 90 */
					esetcursor(&reading);
					im = rot90(im);
					esetcursor(nil);
					angle = (angle+90) % 360;
					redraw(screen);
					flushimage(display, 1);
					break;
				case Upside: 	/* upside-down */
					if(im==nil)
						break;
					esetcursor(&reading);
					rot180(im);
					esetcursor(nil);
					angle = (angle+180) % 360;
					redraw(screen);
					flushimage(display, 1);
					break;
				case Restore:	/* restore */
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
					esetcursor(&reading);
					s = writebitmap();
					if(s)
						string(screen, addpt(screen->r.min, Pt(5,5)), display->black, ZP,
							display->defaultfont, s);
					esetcursor(nil);
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
				n = emenuhit(RMenu, &m, &menu);
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

		case Eplumb:
			pm = e.v;
			if(pm->ndata <= 0){
				plumbfree(pm);
				break;
			}
			if(showdata(pm)) {
				s = estrdup("/tmp/pageplumbXXXXXXX");
				fd = opentemp(s);
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
/*	flushimage(display, 0);	 */
}

void
eresized(int new)
{
	Rectangle r;
	r = screen->r;
	if(new && getwindow(display, Refnone) < 0)
		fprint(2,"can't reattach to window");
	ul = addpt(ul, subpt(screen->r.min, r.min));
	redraw(screen);
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

	dx += 2*Borderwidth;
	dy += 2*Borderwidth;
	if(wctlfd < 0){
		wctlfd = open("/dev/wctl", OWRITE);
		if(wctlfd < 0)
			return;
	}

	r = insetrect(screen->r, -Borderwidth);
	if(Dx(r) >= dx && Dy(r) >= dy)
		return;

	if(Dx(sr)*Dy(sr) == 0)
		sr = screenrect();

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

	fprint(wctlfd, "resize -minx %d -miny %d -maxx %d -maxy %d\n",
		r.min.x, r.min.y, r.max.x, r.max.y);
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

/* all code below this line should be in the library, but is stolen from colors instead */
static char*
rdenv(char *name)
{
	char *v;
	int fd, size;

	fd = open(name, OREAD);
	if(fd < 0)
		return 0;
	size = seek(fd, 0, 2);
	v = malloc(size+1);
	if(v == 0){
		fprint(2, "page: can't malloc: %r\n");
		wexits("no mem");
	}
	seek(fd, 0, 0);
	read(fd, v, size);
	v[size] = 0;
	close(fd);
	return v;
}

void
newwin(void)
{
	char *srv, *mntsrv;
	char spec[100];
	int srvfd, cons, pid;

	switch(rfork(RFFDG|RFPROC|RFNAMEG|RFENVG|RFNOTEG|RFNOWAIT)){
	case -1:
		fprint(2, "page: can't fork: %r\n");
		wexits("no fork");
	case 0:
		break;
	default:
		wexits(0);
	}

	srv = rdenv("/env/wsys");
	if(srv == 0){
		mntsrv = rdenv("/mnt/term/env/wsys");
		if(mntsrv == 0){
			fprint(2, "page: can't find $wsys\n");
			wexits("srv");
		}
		srv = malloc(strlen(mntsrv)+10);
		sprint(srv, "/mnt/term%s", mntsrv);
		free(mntsrv);
		pid  = 0;			/* can't send notes to remote processes! */
	}else
		pid = getpid();
	srvfd = open(srv, ORDWR);
	free(srv);
	if(srvfd == -1){
		fprint(2, "page: can't open %s: %r\n", srv);
		wexits("no srv");
	}
	sprint(spec, "new -pid %d", pid);
	if(mount(srvfd, -1, "/mnt/wsys", 0, spec) == -1){
		fprint(2, "page: can't mount /mnt/wsys: %r (spec=%s)\n", spec);
		wexits("no mount");
	}
	close(srvfd);
	unmount("/mnt/acme", "/dev");
	bind("/mnt/wsys", "/dev", MBEFORE);
	cons = open("/dev/cons", OREAD);
	if(cons==-1){
	NoCons:
		fprint(2, "page: can't open /dev/cons: %r");
		wexits("no cons");
	}
	dup(cons, 0);
	close(cons);
	cons = open("/dev/cons", OWRITE);
	if(cons==-1)
		goto NoCons;
	dup(cons, 1);
	dup(cons, 2);
	close(cons);
/*	wctlfd = open("/dev/wctl", OWRITE); */
}

Rectangle
screenrect(void)
{
	int fd;
	char buf[12*5];

	fd = open("/dev/screen", OREAD);
	if(fd == -1)
		fd=open("/mnt/term/dev/screen", OREAD);
	if(fd == -1){
		fprint(2, "page: can't open /dev/screen: %r\n");
		wexits("window read");
	}
	if(read(fd, buf, sizeof buf) != sizeof buf){
		fprint(2, "page: can't read /dev/screen: %r\n");
		wexits("screen read");
	}
	close(fd);
	return Rect(atoi(buf+12), atoi(buf+24), atoi(buf+36), atoi(buf+48));
}

void
zerox(void)
{
	int pfd[2];

	pipe(pfd);
	switch(rfork(RFFDG|RFPROC)) {
		case -1:
			wexits("cannot fork in zerox: %r");
		case 0: 
			dup(pfd[1], 0);
			close(pfd[0]);
			execl("/bin/page", "page", "-w", nil);
			wexits("cannot exec in zerox: %r\n");
		default:
			close(pfd[1]);
			writeimage(pfd[0], im, 0);
			close(pfd[0]);
			break;
	}
}
