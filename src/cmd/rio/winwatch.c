/*
 * slightly modified from
 * https://github.com/fhs/misc/blob/master/cmd/winwatch/winwatch.c
 * so as to deal with memory leaks and certain X errors
 */

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <regexp.h>
#include <fmt.h>
#include "../devdraw/x11-inc.h"

AUTOLIB(X11);

typedef struct Win Win;
struct Win {
	XWindow n;
	int dirty;
	char *label;
	Rectangle r;
};

XDisplay *dpy;
XWindow root;
Atom net_active_window;
Reprog *exclude = nil;
Win *win;
int nwin;
int mwin;
int onwin;
int rows, cols;
int sortlabels;
int showwmnames;
Font *font;
Image *lightblue;

XErrorHandler oldxerrorhandler;

enum {
	PAD = 3,
	MARGIN = 5
};

static jmp_buf savebuf;

int
winwatchxerrorhandler(XDisplay *disp, XErrorEvent *xe)
{
	char buf[100];

	XGetErrorText(disp, xe->error_code, buf, 100);
	fprint(2, "winwatch: X error %s, request code %d\n",
	    buf, xe->request_code);
	XFlush(disp);
	XSync(disp, False);
	XSetErrorHandler(oldxerrorhandler);
	longjmp(savebuf, 1);
	return(0);  /* Not reached */
}

void*
erealloc(void *v, ulong n)
{
	v = realloc(v, n);
	if(v==nil)
		sysfatal("out of memory reallocating");
	return v;
}

char*
estrdup(char *s)
{
	s = strdup(s);
	if(s==nil)
		sysfatal("out of memory allocating");
	return(s);
}

char*
getproperty(XWindow w, Atom a)
{
	uchar *p;
	int fmt;
	Atom type;
	ulong n, dummy;
	int s;

	n = 100;
	p = nil;
	oldxerrorhandler = XSetErrorHandler(winwatchxerrorhandler);
	s = XGetWindowProperty(dpy, w, a, 0, 100L, 0,
	    AnyPropertyType, &type, &fmt, &n, &dummy, &p);
	XFlush(dpy);
	XSync(dpy, False);
	XSetErrorHandler(oldxerrorhandler);
	if(s!=0){
		XFree(p);
		return(nil);
	}

	return((char*)p);
}

XWindow
findname(XWindow w)
{
	int i;
	uint nxwin;
	XWindow dw1, dw2, *xwin;
	char *p;
	int s;
	Atom net_wm_name;

	p = getproperty(w, XA_WM_NAME);
	if(p){
		free(p);
		return(w);
	}

	net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", FALSE);
	p = getproperty(w, net_wm_name);
	if(p){
		free(p);
		return(w);
	}

	oldxerrorhandler = XSetErrorHandler(winwatchxerrorhandler);
	s = XQueryTree(dpy, w, &dw1, &dw2, &xwin, &nxwin);
	XFlush(dpy);
	XSync(dpy, False);
	XSetErrorHandler(oldxerrorhandler);
	if(s == 0) {
		if (xwin != NULL)
			XFree(xwin);
		return 0;
	}

	for (i = 0; i < nxwin; i++) {
		w = findname(xwin[i]);
		if (w != 0) {
			XFree(xwin);
			return w;
		}
	}
	XFree(xwin);

	return 0;
}

int
wcmp(const void *w1, const void *w2)
{
	return *(XWindow *) w1 - *(XWindow *) w2;
}

/* unicode-aware case-insensitive strcmp,  taken from golang’s gc/subr.c */

int
_cistrcmp(char *p, char *q)
{
	Rune rp, rq;

	while(*p || *q) {
		if(*p == 0)
			return +1;
		if(*q == 0)
			return -1;
		p += chartorune(&rp, p);
		q += chartorune(&rq, q);
		rp = tolowerrune(rp);
		rq = tolowerrune(rq);
		if(rp < rq)
			return -1;
		if(rp > rq)
			return +1;
	}
	return 0;
}

int
winlabelcmp(const void *w1, const void *w2)
{
	const Win *p1 = (Win *) w1;
	const Win *p2 = (Win *) w2;
	return _cistrcmp(p1->label, p2->label);
}

void
refreshwin(void)
{
	XWindow dw1, dw2, *xwin;
	XClassHint class;
	XWindowAttributes attr;
	char *label;
	char *wmname;
	int i, nw;
	uint nxwin;
	Status s;
	Atom net_wm_name;


	oldxerrorhandler = XSetErrorHandler(winwatchxerrorhandler);
	s = XQueryTree(dpy, root, &dw1, &dw2, &xwin, &nxwin);
	XFlush(dpy);
	XSync(dpy, False);
	XSetErrorHandler(oldxerrorhandler);
	if(s==0){
		if(xwin!=NULL)
			XFree(xwin);
		return;
	}
	qsort(xwin, nxwin, sizeof(xwin[0]), wcmp);

	nw = 0;
	for(i=0; i<nxwin; i++){
		memset(&attr, 0, sizeof attr);
		xwin[i] = findname(xwin[i]);
		if(xwin[i]==0)
			continue;

		oldxerrorhandler = XSetErrorHandler(winwatchxerrorhandler);
		s = XGetWindowAttributes(dpy, xwin[i], &attr);
		XFlush(dpy);
		XSync(dpy, False);
		XSetErrorHandler(oldxerrorhandler);
		if(s==0)
			continue;
		if (attr.width <= 0 ||
		    attr.override_redirect ||
		    attr.map_state != IsViewable)
			continue;

		oldxerrorhandler = XSetErrorHandler(winwatchxerrorhandler);
		s = XGetClassHint(dpy, xwin[i], &class);
		XFlush(dpy);
		XSync(dpy, False);
		XSetErrorHandler(oldxerrorhandler);

		if(s==0)
			continue;

		if (exclude!=nil && regexec(exclude, class.res_name, nil, 0)) {
			free(class.res_name);
			free(class.res_class);
			continue;
		}

		net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", FALSE);
		wmname = getproperty(xwin[i], net_wm_name);

		if(wmname==nil){
			wmname = getproperty(xwin[i], XA_WM_NAME);
			if(wmname==nil){
				free(class.res_name);
				free(class.res_class);
				continue;
			}
		}

		label = class.res_name;
		if(showwmnames==1)
			label = wmname;

		if(nw<nwin && win[nw].n==xwin[i] && strcmp(win[nw].label, label)==0) {
			nw++;
			free(wmname);
			free(class.res_name);
			free(class.res_class);
			continue;
		}

		if(nw<nwin){
			free(win[nw].label);
			win[nw].label = nil;
		}

		if(nw>=mwin){
			mwin += 8;
			win = erealloc(win, mwin * sizeof(win[0]));
		}
		win[nw].n = xwin[i];
		win[nw].label = estrdup(label);
		win[nw].dirty = 1;
		win[nw].r = Rect(0, 0, 0, 0);
		free(wmname);
		free(class.res_name);
		free(class.res_class);
		nw++;
	}

	oldxerrorhandler = XSetErrorHandler(winwatchxerrorhandler);
	XFree(xwin);
	XFlush(dpy);
	XSync(dpy, False);
	XSetErrorHandler(oldxerrorhandler);

	while(nwin>nw)
		free(win[--nwin].label);
	nwin = nw;

	if(sortlabels==1)
		qsort(win, nwin, sizeof(struct Win), winlabelcmp);
}

void
drawnowin(int i)
{
	Rectangle r;

	r = Rect(0, 0, (Dx(screen->r) - 2 * MARGIN + PAD) / cols - PAD, font->height);
	r = rectaddpt(
	        rectaddpt(r,
	            Pt(MARGIN + (PAD + Dx(r)) * (i / rows),
 	                MARGIN + (PAD + Dy(r)) * (i % rows))),
	        screen->r.min);
	draw(screen, insetrect(r, -1), lightblue, nil, ZP);
}

void
drawwin(int i)
{
	draw(screen, win[i].r, lightblue, nil, ZP);
	_string(screen, addpt(win[i].r.min, Pt(2, 0)), display->black, ZP,
	    font, win[i].label, nil, strlen(win[i].label),
	    win[i].r, nil, ZP, SoverD);
	border(screen, win[i].r, 1, display->black, ZP);
	win[i].dirty = 0;
}

int
geometry(void)
{
	int i, ncols, z;
	Rectangle r;

	z = 0;
	rows = (Dy(screen->r) - 2 * MARGIN + PAD) / (font->height + PAD);
	if(rows*cols<nwin || rows*cols>=nwin*2){
		ncols = 1;
		if(nwin>0)
			ncols = (nwin + rows - 1) / rows;
		if(ncols!=cols){
			cols = ncols;
			z = 1;
		}
	}

	r = Rect(0, 0, (Dx(screen->r) - 2 * MARGIN + PAD) / cols - PAD, font->height);
	for(i=0; i<nwin; i++)
		win[i].r =
		    rectaddpt(
		        rectaddpt(r,
		            Pt(MARGIN + (PAD + Dx(r)) * (i / rows),
		                MARGIN + (PAD + Dy(r)) * (i % rows))),
		        screen->r.min);

	return z;
}

void
redraw(Image *screen, int all)
{
	int i;

	all |= geometry();
	if(all)
		draw(screen, screen->r, lightblue, nil, ZP);
	for(i=0; i<nwin; i++)
		if(all || win[i].dirty)
			drawwin(i);
	if(!all)
		for (; i<onwin; i++)
			drawnowin(i);
	onwin = nwin;
}

void
eresized(int new)
{
	if(new && getwindow(display, Refmesg)<0)
		fprint(2, "can't reattach to window");
	geometry();
	redraw(screen, 1);
}


void
selectwin(XWindow win)
{
	XEvent ev;
	long mask;

	memset(&ev, 0, sizeof ev);
	ev.xclient.type = ClientMessage;
	ev.xclient.serial = 0;
	ev.xclient.send_event = True;
	ev.xclient.message_type = net_active_window;
	ev.xclient.window = win;
	ev.xclient.format = 32;
	mask = SubstructureRedirectMask | SubstructureNotifyMask;

	XSendEvent(dpy, root, False, mask, &ev);
	XMapRaised(dpy, win);
	XSync(dpy, False);
}

void
click(Mouse m)
{
	int i, j;

	if(m.buttons==0 || (m.buttons&~4))
		return;

	for(i=0; i<nwin; i++)
		if(ptinrect(m.xy, win[i].r))
			break;
	if(i==nwin)
		return;

	do
		m = emouse();
	while(m.buttons==4);

	if(m.buttons!=0){
		do
			m = emouse();
		while(m.buttons);
		return;
	}
	for(j=0; j<nwin; j++)
		if(ptinrect(m.xy, win[j].r))
			break;
	if(j==i)
		selectwin(win[i].n);
}

void
usage(void)
{
	fprint(2,
	    "usage: winwatch [-e exclude] [-W winsize] [-f font] [-n] [-s]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	char *fontname;
	int Etimer;
	Event e;

	sortlabels = 0;
	showwmnames = 0;
	fontname = "/lib/font/bit/lucsans/unicode.8.font";

	ARGBEGIN {
	case 'W':
		winsize = EARGF(usage());
		break;
	case 'f':
		fontname = EARGF(usage());
		break;
	case 'e':
		exclude = regcomp(EARGF(usage()));
		if(exclude==nil)
			sysfatal("Bad regexp");
		break;
	case 's':
		sortlabels = 1;
		break;
	case 'n':
		showwmnames = 1;
		break;
	default:
		usage();
	} ARGEND;

	if(argc)
	    usage();

	/* moved up from original winwatch.c for p9p because there can be only one but we want to restart when needed */
	einit(Emouse | Ekeyboard);
	Etimer = etimer(0, 1000);

	dpy = XOpenDisplay("");
	if(dpy==nil)
		sysfatal("open display: %r");

	root = DefaultRootWindow(dpy);
	net_active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);

	initdraw(0, 0, "winwatch");
	lightblue = allocimagemix(display, DPalebluegreen, DWhite);
	if(lightblue==nil)
		sysfatal("allocimagemix: %r");
	font = openfont(display, fontname);
	if(font==nil)
		sysfatal("font '%s' not found", fontname);

	/* reentry point upon X server errors */
	setjmp(savebuf);

	refreshwin();
	redraw(screen, 1);
	for(;;){
		switch(eread(Emouse|Ekeyboard|Etimer, &e)){
		case Ekeyboard:
			if(e.kbdc==0x7F || e.kbdc=='q')
				exits(0);
			break;
		case Emouse:
			if(e.mouse.buttons)
				click(e.mouse);
			/* fall through  */
		default:		/* Etimer */
			refreshwin();
			redraw(screen, 0);
			break;
		}
	}
}
