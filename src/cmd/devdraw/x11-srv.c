/*
 * Window system protocol server.
 * Use select and a single proc and single stack
 * to avoid aggravating the X11 library, which is
 * subtle and quick to anger.
 */

// #define SHOWEVENT

#include <u.h>
#include <sys/select.h>
#include <errno.h>
#ifdef SHOWEVENT
#include <stdio.h>
#endif
#include "x11-inc.h"

#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>
#include <drawfcall.h>
#include "x11-memdraw.h"
#include "devdraw.h"

#undef time

#define MouseMask (\
	ButtonPressMask|\
	ButtonReleaseMask|\
	PointerMotionMask|\
	Button1MotionMask|\
	Button2MotionMask|\
	Button3MotionMask)

#define Mask MouseMask|ExposureMask|StructureNotifyMask|KeyPressMask|KeyReleaseMask|EnterWindowMask|LeaveWindowMask|FocusChangeMask

typedef struct Kbdbuf Kbdbuf;
typedef struct Mousebuf Mousebuf;
typedef struct Fdbuf Fdbuf;
typedef struct Tagbuf Tagbuf;

struct Kbdbuf
{
	Rune r[32];
	int ri;
	int wi;
	int stall;
};

struct Mousebuf
{
	Mouse m[32];
	int ri;
	int wi;
	int stall;
	int resized;
};

struct Tagbuf
{
	int t[32];
	int ri;
	int wi;
};

struct Fdbuf
{
	uchar buf[2*MAXWMSG];
	uchar *rp;
	uchar *wp;
	uchar *ep;
};

Kbdbuf kbd;
Mousebuf mouse;
Fdbuf fdin;
Fdbuf fdout;
Tagbuf kbdtags;
Tagbuf mousetags;

void fdslide(Fdbuf*);
void runmsg(Wsysmsg*);
void replymsg(Wsysmsg*);
void runxevent(XEvent*);
void matchkbd(void);
void matchmouse(void);
int fdnoblock(int);

int chatty;
int drawsleep;
int fullscreen;

Rectangle windowrect;
Rectangle screenrect;

void
usage(void)
{
	fprint(2, "usage: devdraw (don't run directly)\n");
	exits("usage");
}

void
bell(void *v, char *msg)
{
	if(strcmp(msg, "alarm") == 0)
		drawsleep = drawsleep ? 0 : 1000;
	noted(NCONT);
}

void
main(int argc, char **argv)
{
	int n, top, firstx;
	fd_set rd, wr, xx;
	Wsysmsg m;
	XEvent event;

	/*
	 * Move the protocol off stdin/stdout so that
	 * any inadvertent prints don't screw things up.
	 */
	dup(0, 3);
	dup(1, 4);
	close(0);
	close(1);
	open("/dev/null", OREAD);
	open("/dev/null", OWRITE);

	/* reopens stdout if debugging */
	runxevent(0);

	fmtinstall('W', drawfcallfmt);

	ARGBEGIN{
	case 'D':
		chatty++;
		break;
	default:
		usage();
	}ARGEND

	/*
	 * Ignore arguments.  They're only for good ps -a listings.
	 */
	
	notify(bell);

	fdin.rp = fdin.wp = fdin.buf;
	fdin.ep = fdin.buf+sizeof fdin.buf;
	
	fdout.rp = fdout.wp = fdout.buf;
	fdout.ep = fdout.buf+sizeof fdout.buf;

	fdnoblock(3);
	fdnoblock(4);

	firstx = 1;
	_x.fd = -1;
	for(;;){
		/* set up file descriptors */
		FD_ZERO(&rd);
		FD_ZERO(&wr);
		FD_ZERO(&xx);
		/*
		 * Don't read unless there's room *and* we haven't
		 * already filled the output buffer too much.
		 */
		if(fdout.wp < fdout.buf+MAXWMSG && fdin.wp < fdin.ep)
			FD_SET(3, &rd);
		if(fdout.wp > fdout.rp)
			FD_SET(4, &wr);
		FD_SET(3, &xx);
		FD_SET(4, &xx);
		top = 4;
		if(_x.fd >= 0){
			if(firstx){
				firstx = 0;
				XSelectInput(_x.display, _x.drawable, Mask);
			}
			FD_SET(_x.fd, &rd);
			FD_SET(_x.fd, &xx);
			XFlush(_x.display);
			if(_x.fd > top)
				top = _x.fd;
		}

		if(chatty)
			fprint(2, "select %d...\n", top+1);
		/* wait for something to happen */
	    again:
		if(select(top+1, &rd, &wr, &xx, NULL) < 0){
			if(errno == EINTR)
				goto again;
			if(chatty)
				fprint(2, "select failure\n");
			exits(0);
		}
		if(chatty)
			fprint(2, "got select...\n");

		{
			/* read what we can */
			n = 1;
			while(fdin.wp < fdin.ep && (n = read(3, fdin.wp, fdin.ep-fdin.wp)) > 0)
				fdin.wp += n;
			if(n == 0){
				if(chatty)
					fprint(2, "eof\n");
				exits(0);
			}
			if(n < 0 && errno != EAGAIN)
				sysfatal("reading wsys msg: %r");

			/* pick off messages one by one */
			while((n = convM2W(fdin.rp, fdin.wp-fdin.rp, &m)) > 0){
				/* fprint(2, "<- %W\n", &m); */
				runmsg(&m);
				fdin.rp += n;
			}
			
			/* slide data to beginning of buf */
			fdslide(&fdin);
		}
		{
			/* write what we can */
			n = 1;
			while(fdout.rp < fdout.wp && (n = write(4, fdout.rp, fdout.wp-fdout.rp)) > 0)
				fdout.rp += n;
			if(n == 0)
				sysfatal("short write writing wsys");
			if(n < 0 && errno != EAGAIN)
				sysfatal("writing wsys msg: %r");

			/* slide data to beginning of buf */
			fdslide(&fdout);
		}
		{
			/*
			 * Read an X message if we can.
			 * (XPending actually calls select to make sure
			 * the display's fd is readable and then reads
			 * in any waiting data before declaring whether
			 * there are events on the queue.)
			 */
			while(XPending(_x.display)){
				XNextEvent(_x.display, &event);
				runxevent(&event);
			}
		}
	}
}

int
fdnoblock(int fd)
{
	return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL)|O_NONBLOCK);
}

void
fdslide(Fdbuf *fb)
{
	int n;

	n = fb->wp - fb->rp;
	if(n > 0)
		memmove(fb->buf, fb->rp, n);
	fb->rp = fb->buf;
	fb->wp = fb->rp+n;
}

void
replyerror(Wsysmsg *m)
{
	char err[256];
	
	rerrstr(err, sizeof err);
	m->type = Rerror;
	m->error = err;
	replymsg(m);
}



/* 
 * Handle a single wsysmsg. 
 * Might queue for later (kbd, mouse read)
 */
void
runmsg(Wsysmsg *m)
{
	uchar buf[65536];
	int n;
	Memimage *i;
	
	switch(m->type){
	case Tinit:
		memimageinit();
		i = _xattach(m->label, m->winsize);
		_initdisplaymemimage(i);
		replymsg(m);
		break;

	case Trdmouse:
		mousetags.t[mousetags.wi++] = m->tag;
		if(mousetags.wi == nelem(mousetags.t))
			mousetags.wi = 0;
		if(mousetags.wi == mousetags.ri)
			sysfatal("too many queued mouse reads");
		/* fprint(2, "mouse unstall\n"); */
		mouse.stall = 0;
		matchmouse();
		break;

	case Trdkbd:
		kbdtags.t[kbdtags.wi++] = m->tag;
		if(kbdtags.wi == nelem(kbdtags.t))
			kbdtags.wi = 0;
		if(kbdtags.wi == kbdtags.ri)
			sysfatal("too many queued keyboard reads");
		kbd.stall = 0;
		matchkbd();
		break;

	case Tmoveto:
		_xmoveto(m->mouse.xy);
		replymsg(m);
		break;

	case Tcursor:
		if(m->arrowcursor)
			_xsetcursor(nil);
		else
			_xsetcursor(&m->cursor);
		replymsg(m);
		break;
			
	case Tbouncemouse:
		_xbouncemouse(&m->mouse);
		replymsg(m);
		break;

	case Tlabel:
		_xsetlabel(m->label);
		replymsg(m);
		break;

	case Trdsnarf:
		m->snarf = _xgetsnarf();
		replymsg(m);
		free(m->snarf);
		break;

	case Twrsnarf:
		_xputsnarf(m->snarf);
		replymsg(m);
		break;

	case Trddraw:
		n = m->count;
		if(n > sizeof buf)
			n = sizeof buf;
		n = _drawmsgread(buf, n);
		if(n < 0)
			replyerror(m);
		else{
			m->count = n;
			m->data = buf;
			replymsg(m);
		}
		break;

	case Twrdraw:
		if(_drawmsgwrite(m->data, m->count) < 0)
			replyerror(m);
		else
			replymsg(m);
		break;
	
	case Ttop:
		_xtopwindow();
		replymsg(m);
		break;
	
	case Tresize:
		_xresizewindow(m->rect);
		replymsg(m);
		break;
	}
}

/*
 * Reply to m.
 */
void
replymsg(Wsysmsg *m)
{
	int n;

	/* T -> R msg */
	if(m->type%2 == 0)
		m->type++;
		
	/* fprint(2, "-> %W\n", m); */
	/* copy to output buffer */
	n = sizeW2M(m);
	if(fdout.wp+n > fdout.ep)
		sysfatal("out of space for reply message");
	convW2M(m, fdout.wp, n);
	fdout.wp += n;
}

/*
 * Match queued kbd reads with queued kbd characters.
 */
void
matchkbd(void)
{
	Wsysmsg m;
	
	if(kbd.stall)
		return;
	while(kbd.ri != kbd.wi && kbdtags.ri != kbdtags.wi){
		m.type = Rrdkbd;
		m.tag = kbdtags.t[kbdtags.ri++];
		if(kbdtags.ri == nelem(kbdtags.t))
			kbdtags.ri = 0;
		m.rune = kbd.r[kbd.ri++];
		if(kbd.ri == nelem(kbd.r))
			kbd.ri = 0;
		replymsg(&m);
	}
}

/*
 * Match queued mouse reads with queued mouse events.
 */
void
matchmouse(void)
{
	Wsysmsg m;
	
	while(mouse.ri != mouse.wi && mousetags.ri != mousetags.wi){
		m.type = Rrdmouse;
		m.tag = mousetags.t[mousetags.ri++];
		if(mousetags.ri == nelem(mousetags.t))
			mousetags.ri = 0;
		m.mouse = mouse.m[mouse.ri];
		m.resized = mouse.resized;
		/*
		if(m.resized)
			fprint(2, "sending resize\n");
		*/
		mouse.resized = 0;
		mouse.ri++;
		if(mouse.ri == nelem(mouse.m))
			mouse.ri = 0;
		replymsg(&m);
	}
}

static int kbuttons;
static int altdown;
static int kstate;

static void
sendmouse(Mouse m)
{
	m.buttons |= kbuttons;
	mouse.m[mouse.wi] = m;
	mouse.wi++;
	if(mouse.wi == nelem(mouse.m))
		mouse.wi = 0;
	if(mouse.wi == mouse.ri){
		mouse.stall = 1;
		mouse.ri = 0;
		mouse.wi = 1;
		mouse.m[0] = m;
		/* fprint(2, "mouse stall\n"); */
	}
	matchmouse();
}

/*
 * Handle an incoming X event.
 */
void
runxevent(XEvent *xev)
{
	int c;
	KeySym k;
	static Mouse m;
	XButtonEvent *be;
	XKeyEvent *ke;

#ifdef SHOWEVENT
	static int first = 1;
	if(first){
		dup(create("/tmp/devdraw.out", OWRITE, 0666), 1);
		setbuf(stdout, 0);
		first = 0;
	}
#endif

	if(xev == 0)
		return;

#ifdef SHOWEVENT
	print("\n");
	ShowEvent(xev);
#endif

	switch(xev->type){
	case Expose:
		_xexpose(xev);
		break;
	
	case DestroyNotify:
		if(_xdestroy(xev))
			exits(0);
		break;

	case ConfigureNotify:
		if(_xconfigure(xev)){
			mouse.resized = 1;
			_xreplacescreenimage();
			sendmouse(m);
		}
		break;

	case ButtonPress:
		be = (XButtonEvent*)xev;
		if(be->button == 1) {
			if(kstate & ControlMask)
				be->button = 2;
			else if(kstate & Mod1Mask)
				be->button = 3;
		}
		// fall through
	case ButtonRelease:
		altdown = 0;
		// fall through
	case MotionNotify:
		if(mouse.stall)
			return;
		if(_xtoplan9mouse(xev, &m) < 0)
			return;
		sendmouse(m);
		break;
	
	case KeyRelease:
	case KeyPress:
		ke = (XKeyEvent*)xev;
		XLookupString(ke, NULL, 0, &k, NULL);
		c = ke->state;
		switch(k) {
		case XK_Alt_L:
		case XK_Meta_L:	/* Shift Alt on PCs */
		case XK_Alt_R:
		case XK_Meta_R:	/* Shift Alt on PCs */
		case XK_Multi_key:
			if(xev->type == KeyPress)
				altdown = 1;
			else if(altdown) {
				altdown = 0;
				sendalt();
			}
			break;
		}

		switch(k) {
		case XK_Control_L:
			if(xev->type == KeyPress)
				c |= ControlMask;
			else
				c &= ~ControlMask;
			goto kbutton;
		case XK_Alt_L:
		case XK_Shift_L:
			if(xev->type == KeyPress)
				c |= Mod1Mask;
			else
				c &= ~Mod1Mask;
		kbutton:
			kstate = c;
			if(m.buttons || kbuttons) {
				altdown = 0; // used alt
				kbuttons = 0;
				if(c & ControlMask)
					kbuttons |= 2;
				if(c & Mod1Mask)
					kbuttons |= 4;
				sendmouse(m);
				break;
			}
		}

		if(xev->type != KeyPress)
			break;
		if(k == XK_F11){
			fullscreen = !fullscreen;
			_xmovewindow(fullscreen ? screenrect : windowrect);
			return;
		}
		if(kbd.stall)
			return;
		if((c = _xtoplan9kbd(xev)) < 0)
			return;
		kbd.r[kbd.wi++] = c;
		if(kbd.wi == nelem(kbd.r))
			kbd.wi = 0;
		if(kbd.ri == kbd.wi)
			kbd.stall = 1;
		matchkbd();
		break;
	
	case FocusOut:
		/*
		 * Some key combinations (e.g. Alt-Tab) can cause us
		 * to see the key down event without the key up event,
		 * so clear out the keyboard state when we lose the focus.
		 */
		kstate = 0;
		altdown = 0;
		abortcompose();
		break;
	
	case SelectionRequest:
		_xselect(xev);
		break;
	}
}

