/* Copyright (c) 1994-1996 David Hogan, see README for licence details */
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <X11/X.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#ifdef SHAPE
#include <X11/extensions/shape.h>
#endif
#include "dat.h"
#include "fns.h"
#include "patchlevel.h"

char	*version[] =
{
	"rio version 1.0, Copyright (c) 1994-1996 David Hogan, (c) 2004 Russ Cox", 0
};

Display 		*dpy;
ScreenInfo	*screens;
int 			initting;
XFontStruct 	*font;
int 			nostalgia;
char			**myargv;
char			*termprog;
char			*shell;
Bool			shape;
int 			_border = 4;
int 			_corner = 25;
int 			_inset = 1;
int 			curtime;
int 			debug;
int 			signalled;
int 			scrolling;
int 			num_screens;
int			solidsweep = 0;
int			numvirtuals = 0;
int			ffm = 0;

Atom		exit_rio;
Atom		restart_rio;
Atom		wm_state;
Atom		wm_change_state;
Atom		wm_protocols;
Atom		wm_delete;
Atom		wm_take_focus;
Atom		wm_lose_focus;
Atom		wm_colormaps;
Atom		_rio_running;
Atom		_rio_hold_mode;
Atom		wm_state_fullscreen;
Atom		wm_state;

char	*fontlist[] = {
	"lucm.latin1.9",
	"blit",
	"*-lucidatypewriter-bold-*-14-*-75-*",
	"*-lucidatypewriter-medium-*-12-*-75-*",
	"9x15bold",
	"fixed",
	"*",
	0
};

void
usage(void)
{
	fprintf(stderr, "usage: rio [-grey] [-font fname] [-s] [-term prog] [-version] [-virtuals num] [exit|restart]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int i, background, do_exit, do_restart;
	char *fname;
	int shape_event;
#ifdef SHAPE
	int dummy;
#endif

	shape_event = 0;
	myargv = argv;			/* for restart */

	do_exit = do_restart = 0;
	background = 0;
	font = 0;
	fname = 0;
	for(i = 1; i < argc; i++)
		if(strcmp(argv[i], "-nostalgia") == 0)
			nostalgia++;
		else if(strcmp(argv[i], "-grey") == 0)
			background = 1;
		else if(strcmp(argv[i], "-debug") == 0)
			debug++;
	/*
		else if(strcmp(argv[i], "-ffm") == 0)
			ffm++;
	*/
		else if(strcmp(argv[i], "-font") == 0 && i+1<argc){
			i++;
			fname = argv[i];
		}
		else if(strcmp(argv[i], "-term") == 0 && i+1<argc)
			termprog = argv[++i];
		else if(strcmp(argv[i], "-virtuals") == 0 && i+1<argc){
			numvirtuals = atoi(argv[++i]);
			if(numvirtuals < 0 || numvirtuals > 12){
				fprintf(stderr, "rio: wrong number of virtual displays, defaulting to 4\n");
				numvirtuals = 4;
			}
		} else if(strcmp(argv[i], "-version") == 0){
			fprintf(stderr, "%s", version[0]);
			if(PATCHLEVEL > 0)
				fprintf(stderr, "; patch level %d", PATCHLEVEL);
			fprintf(stderr, "\n");
			exit(0);
		}
		else if(strcmp(argv[i], "-s") == 0){
			scrolling = 1;
		}
		else if(argv[i][0] == '-')
			usage();
		else
			break;
	for(; i < argc; i++)
		if(strcmp(argv[i], "exit") == 0)
			do_exit++;
		else if(strcmp(argv[i], "restart") == 0)
			do_restart++;
		else
			usage();

	if(do_exit && do_restart)
		usage();

	shell = (char *)getenv("SHELL");
	if(shell == NULL)
		shell = DEFSHELL;

	dpy = XOpenDisplay("");
	if(dpy == 0)
		fatal("can't open display");

	initting = 1;
	XSetErrorHandler(handler);
	if(signal(SIGTERM, sighandler) == SIG_IGN)
		signal(SIGTERM, SIG_IGN);
	if(signal(SIGINT, sighandler) == SIG_IGN)
		signal(SIGINT, SIG_IGN);
	if(signal(SIGHUP, sighandler) == SIG_IGN)
		signal(SIGHUP, SIG_IGN);

	exit_rio = XInternAtom(dpy, "9WM_EXIT", False);
	restart_rio = XInternAtom(dpy, "9WM_RESTART", False);

	curtime = -1;		/* don't care */
	if(do_exit){
		sendcmessage(DefaultRootWindow(dpy), exit_rio, 0L, 1, 1);
		XSync(dpy, False);
		exit(0);
	}
	if(do_restart){
		sendcmessage(DefaultRootWindow(dpy), restart_rio, 0L, 1, 1);
		XSync(dpy, False);
		exit(0);
	}

	if(0) XSynchronize(dpy, True);

	wm_state = XInternAtom(dpy, "WM_STATE", False);
	wm_change_state = XInternAtom(dpy, "WM_CHANGE_STATE", False);
	wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wm_take_focus = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	wm_lose_focus = XInternAtom(dpy, "_9WM_LOSE_FOCUS", False);
	wm_colormaps = XInternAtom(dpy, "WM_COLORMAP_WINDOWS", False);
	_rio_running = XInternAtom(dpy, "_9WM_RUNNING", False);
	_rio_hold_mode = XInternAtom(dpy, "_9WM_HOLD_MODE", False);
	wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
	wm_state_fullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);

	if(fname != 0)
		if((font = XLoadQueryFont(dpy, fname)) == 0)
			fprintf(stderr, "rio: warning: can't load font %s\n", fname);

	if(font == 0){
		i = 0;
		for(;;){
			fname = fontlist[i++];
			if(fname == 0){
				fprintf(stderr, "rio: warning: can't find a font\n");
				break;
			}
			font = XLoadQueryFont(dpy, fname);
			if(font != 0)
				break;
		}
	}
	if(nostalgia){
		_border--;
		_inset--;
	}

#ifdef	SHAPE
	shape = XShapeQueryExtension(dpy, &shape_event, &dummy);
#endif

	num_screens = ScreenCount(dpy);
	screens = (ScreenInfo *)malloc(sizeof(ScreenInfo) * num_screens);

	for(i = 0; i < num_screens; i++)
		initscreen(&screens[i], i, background);

	initb2menu(numvirtuals);

	/* set selection so that 9term knows we're running */
	curtime = CurrentTime;
	XSetSelectionOwner(dpy, _rio_running, screens[0].menuwin, timestamp());

	XSync(dpy, False);
	initting = 0;

	nofocus();

	for(i = 0; i < num_screens; i++)
		scanwins(&screens[i]);

	keysetup();
	mainloop(shape_event);
	return 0;
}

void
initscreen(ScreenInfo *s, int i, int background)
{
	char *ds, *colon, *dot1;
	unsigned long mask;
	unsigned long gmask;
	XGCValues gv;
	XSetWindowAttributes attr;
	XVisualInfo xvi;
	XSetWindowAttributes attrs;

	s->num = i;
	s->root = RootWindow(dpy, i);
	s->def_cmap = DefaultColormap(dpy, i);
	s->min_cmaps = MinCmapsOfScreen(ScreenOfDisplay(dpy, i));
	s->depth = DefaultDepth(dpy, i);

	/*
	 * Figure out underlying screen format.
	 */
	if(XMatchVisualInfo(dpy, i, 16, TrueColor, &xvi)
	|| XMatchVisualInfo(dpy, i, 16, DirectColor, &xvi)){
		s->vis = xvi.visual;
		s->depth = 16;
	}
	else
	if(XMatchVisualInfo(dpy, i, 15, TrueColor, &xvi)
	|| XMatchVisualInfo(dpy, i, 15, DirectColor, &xvi)){
		s->vis = xvi.visual;
		s->depth = 15;
	}
	else
	if(XMatchVisualInfo(dpy, i, 24, TrueColor, &xvi)
	|| XMatchVisualInfo(dpy, i, 24, DirectColor, &xvi)){
		s->vis = xvi.visual;
		s->depth = 24;
	}
	else
	if(XMatchVisualInfo(dpy, i, 8, PseudoColor, &xvi)
	|| XMatchVisualInfo(dpy, i, 8, StaticColor, &xvi)){
		s->vis = xvi.visual;
		s->depth = 8;
	}
	else{
		s->depth = DefaultDepth(dpy, i);
		if(s->depth != 8){
			fprintf(stderr, "can't understand depth %d screen", s->depth);
			exit(1);
		}
		s->vis = DefaultVisual(dpy, i);
	}
	if(DefaultDepth(dpy, i) != s->depth){
		s->def_cmap = XCreateColormap(dpy, s->root, s->vis, AllocNone);
	}

	ds = DisplayString(dpy);
	colon = rindex(ds, ':');
	if(colon && num_screens > 1){
		strcpy(s->display, "DISPLAY=");
		strcat(s->display, ds);
		colon = s->display + 8 + (colon - ds);	/* use version in buf */
		dot1 = index(colon, '.');	/* first period after colon */
		if(!dot1)
			dot1 = colon + strlen(colon);	/* if not there, append */
		sprintf(dot1, ".%d", i);
	}
	else
		s->display[0] = '\0';

	s->black = BlackPixel(dpy, i);
	s->white = WhitePixel(dpy, i);
	s->activeholdborder = colorpixel(dpy, s, s->depth, 0x000099, s->white);
	s->inactiveholdborder = colorpixel(dpy, s, s->depth, 0x005DBB, s->black);
	s->activeborder = colorpixel(dpy, s, s->depth, 0x55AAAA, s->black);
	s->inactiveborder = colorpixel(dpy, s, s->depth, 0x9EEEEE, s->white);
	s->red = colorpixel(dpy, s, s->depth, 0xDD0000, s->white);
	s->width = WidthOfScreen(ScreenOfDisplay(dpy, i));
	s->height = HeightOfScreen(ScreenOfDisplay(dpy, i));
	s->bkup[0] = XCreatePixmap(dpy, s->root, 2*s->width, BORDER, DefaultDepth(dpy, i));
	s->bkup[1] = XCreatePixmap(dpy, s->root, BORDER, 2*s->height, DefaultDepth(dpy, i));

	gv.foreground = s->black^s->white;
	gv.background = s->white;
	gv.function = GXxor;
	gv.line_width = 0;
	gv.subwindow_mode = IncludeInferiors;
	gmask = GCForeground | GCBackground | GCFunction | GCLineWidth
		| GCSubwindowMode;
	if(font != 0){
		gv.font = font->fid;
		gmask |= GCFont;
	}
	s->gc = XCreateGC(dpy, s->root, gmask, &gv);

	gv.function = GXcopy;
	s->gccopy = XCreateGC(dpy, s->root, gmask, &gv);

	gv.foreground = s->red;
	s->gcred = XCreateGC(dpy, s->root, gmask, &gv);

	gv.foreground = colorpixel(dpy, s, s->depth, 0xEEEEEE, s->black);
	s->gcsweep = XCreateGC(dpy, s->root, gmask, &gv);

	initcurs(s);

	attr.cursor = s->arrow;
	attr.event_mask = SubstructureRedirectMask
		| SubstructureNotifyMask | ColormapChangeMask
		| ButtonPressMask | ButtonReleaseMask | PropertyChangeMask
		| KeyPressMask | EnterWindowMask;
	mask = CWCursor|CWEventMask;
	XChangeWindowAttributes(dpy, s->root, mask, &attr);
	XSync(dpy, False);

	if(background){
		XSetWindowBackgroundPixmap(dpy, s->root, s->root_pixmap);
		XClearWindow(dpy, s->root);
	} else
		system("xsetroot -solid grey30");

	attrs.border_pixel =  colorpixel(dpy, s, s->depth, 0x88CC88, s->black);
	attrs.background_pixel =  colorpixel(dpy, s, s->depth, 0xE9FFE9, s->white);
	attrs.colormap = s->def_cmap;

	s->menuwin = XCreateWindow(dpy, s->root, 0, 0, 1, 1, 2,
						s->depth,
						CopyFromParent,
						s->vis,
						CWBackPixel | CWBorderPixel | CWColormap,
						&attrs
						);


	gv.foreground = colorpixel(dpy, s, s->depth, 0xE9FFE9, s->white);
	s->gcmenubg = XCreateGC(dpy, s->menuwin, gmask, &gv);

	gv.foreground = colorpixel(dpy, s, s->depth, 0x448844, s->black);
	s->gcmenubgs = XCreateGC(dpy, s->menuwin, gmask, &gv);

	gv.foreground = s->black;
	gv.background = colorpixel(dpy, s, s->depth, 0xE9FFE9, s->white);
	s->gcmenufg = XCreateGC(dpy, s->menuwin, gmask, &gv);

	gv.foreground = colorpixel(dpy, s, s->depth, 0xE9FFE9, s->white);
	gv.background = colorpixel(dpy, s, s->depth, 0x448844, s->black);
	s->gcmenufgs = XCreateGC(dpy, s->menuwin, gmask, &gv);

	attrs.border_pixel =  s->red;
	attrs.background_pixel =  colorpixel(dpy, s, s->depth, 0xEEEEEE, s->black);
	attrs.colormap = s->def_cmap;
	s->sweepwin = XCreateWindow(dpy, s->root, 0, 0, 1, 1, 4,
						s->depth,
						CopyFromParent,
						s->vis,
						CWBackPixel | CWBorderPixel | CWColormap,
						&attrs
						);
}

ScreenInfo*
getscreen(Window w)
{
	int i;

	for(i = 0; i < num_screens; i++)
		if(screens[i].root == w)
			return &screens[i];

	return 0;
}

Time
timestamp(void)
{
	XEvent ev;

	if(curtime == CurrentTime){
		XChangeProperty(dpy, screens[0].root, _rio_running, _rio_running, 8,
				PropModeAppend, (unsigned char *)"", 0);
		XMaskEvent(dpy, PropertyChangeMask, &ev);
		curtime = ev.xproperty.time;
	}
	return curtime;
}

void
sendcmessage(Window w, Atom a, long x, int isroot, int usemask)
{
	XEvent ev;
	int status;
	long mask;

	memset(&ev, 0, sizeof(ev));
	ev.xclient.type = ClientMessage;
	ev.xclient.window = w;
	ev.xclient.message_type = a;
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = x;
	ev.xclient.data.l[1] = timestamp();
	mask = 0;
	if(usemask){
		mask |= KeyPressMask;	/* seems to be necessary */
		if(isroot)
			mask |= SubstructureRedirectMask;		/* magic! */
		else
			mask |= ExposureMask;	/* not really correct but so be it */
	}
	status = XSendEvent(dpy, w, False, mask, &ev);
	if(status == 0)
		fprintf(stderr, "rio: sendcmessage failed\n");
}

void
sendconfig(Client *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.event = c->window;
	ce.window = c->window;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->dx;
	ce.height = c->dy;
	ce.border_width = c->border;
	ce.above = None;
	ce.override_redirect = 0;
	XSendEvent(dpy, c->window, False, StructureNotifyMask, (XEvent*)&ce);
}

void
sighandler(void)
{
	signalled = 1;
}

void
getevent(XEvent *e)
{
	int fd;
	fd_set rfds;
	struct timeval t;

	if(!signalled){
		if(QLength(dpy) > 0){
			XNextEvent(dpy, e);
			return;
		}
		fd = ConnectionNumber(dpy);
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		t.tv_sec = t.tv_usec = 0;
		if(select(fd+1, &rfds, NULL, NULL, &t) == 1){
			XNextEvent(dpy, e);
			return;
		}
		XFlush(dpy);
		FD_SET(fd, &rfds);
		if(select(fd+1, &rfds, NULL, NULL, NULL) == 1){
			XNextEvent(dpy, e);
			return;
		}
		if(errno != EINTR || !signalled){
			perror("rio: select failed");
			exit(1);
		}
	}
	fprintf(stderr, "rio: exiting on signal\n");
	cleanup();
	exit(1);
}

void
cleanup(void)
{
	Client *c, *cc[2], *next;
	XWindowChanges wc;
	int i;

	/* order of un-reparenting determines final stacking order... */
	cc[0] = cc[1] = 0;
	for(c = clients; c; c = next){
		next = c->next;
		i = normal(c);
		c->next = cc[i];
		cc[i] = c;
	}

	for(i = 0; i < 2; i++){
		for(c = cc[i]; c; c = c->next){
			if(!withdrawn(c)){
				XReparentWindow(dpy, c->window, c->screen->root,
						c->x, c->y);
			}
			wc.border_width = c->border;
			XConfigureWindow(dpy, c->window, CWBorderWidth, &wc);
		}
	}

	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, timestamp());
	for(i = 0; i < num_screens; i++)
		cmapnofocus(&screens[i]);
	XCloseDisplay(dpy);
}
