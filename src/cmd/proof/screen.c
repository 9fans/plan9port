#include <u.h>
#include <libc.h>
#include <draw.h>
#include <cursor.h>
#include <event.h>
#include <bio.h>
#include "proof.h"

static	int	checkmouse(void);
/* static	int	buttondown(void); */
static	char	*getmousestr(void);
static	char	*getkbdstr(int);

extern	Cursor	blot;
extern	char	*track;

Mouse	mouse;

void
mapscreen(void)
{
	if(initdraw(0, 0, "proof") < 0){
		fprint(2, "proof: initdraw failed: %r\n");
		exits("initdraw");
	}
	einit(Ekeyboard|Emouse);
}

void
clearscreen(void)
{
	draw(screen, screen->r, display->black, nil, ZP);
}

void
screenprint(char *fmt, ...)
{
	char buf[100];
	Point p;
	va_list args;

	va_start(args, fmt);
	vseprint(buf, &buf[sizeof buf], fmt, args);
	va_end(args);
	p = Pt(screen->clipr.min.x+40, screen->clipr.max.y-40);
	string(screen, p, display->black, ZP, font, buf);
}

#define	Viewkey	0xb2
#define etimer(x, y) 0

char *
getcmdstr(void)
{
	Event ev;
	int e;
	static ulong timekey = 0;
	ulong tracktm = 0;
	Dir *dir;

	if(track){
		if(timekey == 0)
			timekey = etimer(0, 5000);
		dir = dirstat(track);
		if(dir != nil){
			tracktm = dir->mtime;
			free(dir);
		}
	}
	for (;;) {
		e = event(&ev);
		if(resized){
			resized = 0;
			return "p";
		}
		if ((e & Emouse) && ev.mouse.buttons) {
			mouse = ev.mouse;
			return getmousestr();
		} else if (e & Ekeyboard)
			return getkbdstr(ev.kbdc);	/* sadly, no way to unget */
		else if (e & timekey) {
			if((dir = dirstat(track)) != nil){
				if(tracktm < dir->mtime){
					free(dir);
					return "q";
				}
				free(dir);
			}
		}
	}
}

static char *
getkbdstr(int c0)
{
	static char buf[100];
	char *p;
	int c;

	if (c0 == '\n')
		return "";
	buf[0] = c0;
	buf[1] = 0;
	screenprint("%s", buf);
	for (p = buf+1; (c = ekbd()) != '\n' && c != '\r' && c != -1 && c != Viewkey; ) {
		if (c == '\b' && p > buf) {
			*--p = ' ';
		} else {
			*p++ = c;
			*p = 0;
		}
		screenprint("%s", buf);
	}
	*p = 0;
	return buf;
}


#define button3(b)	((b) & 4)
#define button2(b)	((b) & 2)
#define button1(b)	((b) & 1)
#define button23(b)	((b) & 6)
#define button123(b)	((b) & 7)

#define	butcvt(b)	(1 << ((b) - 1))

#if 0
static int buttondown(void)	/* report state of buttons, if any */
{
	if (!ecanmouse())	/* no event pending */
		return 0;
	mouse = emouse();	/* something, but it could be motion */
	return mouse.buttons & 7;
}
#endif

int waitdown(void)	/* wait until some button is down */
{
	while (!(mouse.buttons & 7))
		mouse = emouse();
	return mouse.buttons & 7;
}

int waitup(void)
{
	while (mouse.buttons & 7)
		mouse = emouse();
	return mouse.buttons & 7;
}

char *m3[]	= { "next", "prev", "page n", "again", "bigger", "smaller", "pan", "quit?", 0 };
char *m2[]	= { 0 };

enum { Next = 0, Prev, Page, Again, Bigger, Smaller, Pan, Quit };

Menu	mbut3	= { m3, 0, 0 };
Menu	mbut2	= { m2, 0, 0 };

int	last_hit;
int	last_but;

char *pan(void)
{
	Point dd, xy, lastxy, min, max;

	esetcursor(&blot);
	waitdown();
	xy = mouse.xy;
	do{
		lastxy = mouse.xy;
		mouse = emouse();
		dd = subpt(mouse.xy, lastxy);
		min = addpt(screen->clipr.min, dd);
		max = addpt(screen->clipr.max, dd);
		draw(screen, rectaddpt(screen->r, subpt(mouse.xy, lastxy)),
			screen, nil, screen->r.min);
		if(mouse.xy.x < lastxy.x)	/* moved left, clear right */
			draw(screen, Rect(max.x, screen->r.min.y, screen->r.max.x, screen->r.max.y),
				display->white, nil, ZP);
		else	/* moved right, clear left*/
			draw(screen, Rect(screen->r.min.x, screen->r.min.y, min.x, screen->r.max.y),
				display->white, nil, ZP);
		if(mouse.xy.y < lastxy.y)	/* moved up, clear down */
			draw(screen, Rect(screen->r.min.x, max.y, screen->r.max.x, screen->r.max.y),
				display->white, nil, ZP);
		else		/* moved down, clear up */
			draw(screen, Rect(screen->r.min.x, screen->r.min.y, screen->r.max.x, min.y),
				display->white, nil, ZP);
		flushimage(display, 1);
	}while(mouse.buttons);

	xyoffset = addpt(xyoffset, subpt(mouse.xy, xy));

	esetcursor(0);
	return "p";
}

static char *getmousestr(void)
{
	static char buf[20];

	checkmouse();
	if (last_but == 1)
		return "p";	/* repaint after panning */
	if (last_but == 2) {
		return "c";
	} else if (last_but == 3) {
		switch (last_hit) {
		case Next:
			return "";
		case Prev:
			return "-1";
		case Page:
			screenprint("page? ");
			return "c";
		case Again:
			return "p";
		case Bigger:
			sprint(buf, "m%g", mag * 1.1);
			return buf;
		case Smaller:
			sprint(buf, "m%g", mag / 1.1);
			return buf;
		case Pan:
			return pan();
		case Quit:
			return "q";
		default:
			return "c";
		}
	} else {		/* button 1 or bail out */
		return "c";
	}
}

static int
checkmouse(void)	/* return button touched if any */
{
	int c, b;
	char *p;
	extern int confirm(int);

	b = waitdown();
	last_but = 0;
	last_hit = -1;
	c = 0;
	if (button3(b)) {
		last_hit = emenuhit(3, &mouse, &mbut3);
		last_but = 3;
	} else if (button2(b)) {
		last_hit = emenuhit(2, &mouse, &mbut2);
		last_but = 2;
	} else {		/* button1() */
		pan();
		last_but = 1;
	}
	waitup();
	if (last_but == 3 && last_hit >= 0) {
		p = m3[last_hit];
		c = p[strlen(p) - 1];
	}
	if (c == '?' && !confirm(last_but))
		last_hit = -1;
	return last_but;
}

Cursor deadmouse = {
	{ 0, 0},	/* offset */
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x0C, 0x00, 0x82, 0x04, 0x41,
	  0xFF, 0xE1, 0x5F, 0xF1, 0x3F, 0xFE, 0x17, 0xF0,
	  0x03, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x0C, 0x00, 0x82, 0x04, 0x41,
	  0xFF, 0xE1, 0x5F, 0xF1, 0x3F, 0xFE, 0x17, 0xF0,
	  0x03, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, }
};

Cursor blot ={
	{ 0, 0 },
	{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, },
	{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, }
};

Cursor skull ={
	{ 0, 0 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x03,
	  0xE7, 0xE7, 0x3F, 0xFC, 0x0F, 0xF0, 0x0D, 0xB0,
	  0x07, 0xE0, 0x06, 0x60, 0x37, 0xEC, 0xE4, 0x27,
	  0xC3, 0xC3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x03,
	  0xE7, 0xE7, 0x3F, 0xFC, 0x0F, 0xF0, 0x0D, 0xB0,
	  0x07, 0xE0, 0x06, 0x60, 0x37, 0xEC, 0xE4, 0x27,
	  0xC3, 0xC3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, }
};

int
confirm(int but)	/* ask for confirmation if menu item ends with '?' */
{
	int c;
	static int but_cvt[8] = { 0, 1, 2, 0, 3, 0, 0, 0 };

	esetcursor(&skull);
	c = waitdown();
	waitup();
	esetcursor(0);
	return but == but_cvt[c];
}
