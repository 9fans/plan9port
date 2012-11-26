/*
 * Window system protocol server.
 */

#include <u.h>
#include <libc.h>
#include "cocoa-thread.h"
#include <draw.h>
#include <memdraw.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>
#include <drawfcall.h>
#include "cocoa-screen.h"
#include "devdraw.h"

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
	Mouse last;
	int ri;
	int wi;
	int stall;
};

struct Tagbuf
{
	int t[32];
	int ri;
	int wi;
};

Kbdbuf kbd;
Mousebuf mouse;
Tagbuf kbdtags;
Tagbuf mousetags;

void runmsg(Wsysmsg*);
void replymsg(Wsysmsg*);
void matchkbd(void);
void matchmouse(void);


QLock lk;
void
zlock(void)
{
	qlock(&lk);
}

void
zunlock(void)
{
	qunlock(&lk);
}

int trace = 0;

void
servep9p(void)
{
	uchar buf[4], *mbuf;
	int nmbuf, n, nn;
	Wsysmsg m;

	fmtinstall('W', drawfcallfmt);
	
	mbuf = nil;
	nmbuf = 0;
	while((n = read(3, buf, 4)) == 4){
		GET(buf, n);
		if(n > nmbuf){
			free(mbuf);
			mbuf = malloc(4+n);
			if(mbuf == nil)
				sysfatal("malloc: %r");
			nmbuf = n;
		}
		memmove(mbuf, buf, 4);
		nn = readn(3, mbuf+4, n-4);
		if(nn != n-4)
			sysfatal("eof during message");

		/* pick off messages one by one */
		if(convM2W(mbuf, nn+4, &m) <= 0)
			sysfatal("cannot convert message");
		if(trace) fprint(2, "<- %W\n", &m);
		runmsg(&m);
	}
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
	static uchar buf[65536];
	int n;
	Memimage *i;
	
	switch(m->type){
	case Tinit:
		memimageinit();
		i = attachscreen(m->label, m->winsize);
		_initdisplaymemimage(i);
		replymsg(m);
		break;

	case Trdmouse:
		zlock();
		mousetags.t[mousetags.wi++] = m->tag;
		if(mousetags.wi == nelem(mousetags.t))
			mousetags.wi = 0;
		if(mousetags.wi == mousetags.ri)
			sysfatal("too many queued mouse reads");
		mouse.stall = 0;
		matchmouse();
		zunlock();
		break;

	case Trdkbd:
		zlock();
		kbdtags.t[kbdtags.wi++] = m->tag;
		if(kbdtags.wi == nelem(kbdtags.t))
			kbdtags.wi = 0;
		if(kbdtags.wi == kbdtags.ri)
			sysfatal("too many queued keyboard reads");
		kbd.stall = 0;
		matchkbd();
		zunlock();
		break;

	case Tmoveto:
		setmouse(m->mouse.xy);
		replymsg(m);
		break;

	case Tcursor:
		if(m->arrowcursor)
			setcursor(nil);
		else
			setcursor(&m->cursor);
		replymsg(m);
		break;
			
	case Tbouncemouse:
	//	_xbouncemouse(&m->mouse);
		replymsg(m);
		break;

	case Tlabel:
		kicklabel(m->label);
		replymsg(m);
		break;

	case Trdsnarf:
		m->snarf = getsnarf();
		replymsg(m);
		free(m->snarf);
		break;

	case Twrsnarf:
		putsnarf(m->snarf);
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
		topwin();
		replymsg(m);
		break;
	
	case Tresize:
	//	_xresizewindow(m->rect);
		replymsg(m);
		break;
	}
}

/*
 * Reply to m.
 */
QLock replylock;
void
replymsg(Wsysmsg *m)
{
	int n;
	static uchar *mbuf;
	static int nmbuf;

	/* T -> R msg */
	if(m->type%2 == 0)
		m->type++;
		
	if(trace) fprint(2, "-> %W\n", m);
	/* copy to output buffer */
	n = sizeW2M(m);

	qlock(&replylock);
	if(n > nmbuf){
		free(mbuf);
		mbuf = malloc(n);
		if(mbuf == nil)
			sysfatal("out of memory");
		nmbuf = n;
	}
	convW2M(m, mbuf, n);
	if(write(4, mbuf, n) != n)
		sysfatal("write: %r");
	qunlock(&replylock);
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
		m.resized = mouseresized;
		/*
		if(m.resized)
			fprint(2, "sending resize\n");
		*/
		mouseresized = 0;
		mouse.ri++;
		if(mouse.ri == nelem(mouse.m))
			mouse.ri = 0;
		replymsg(&m);
	}
}

void
mousetrack(int x, int y, int b, uint ms)
{
	Mouse *m;
	
	if(x < mouserect.min.x)
		x = mouserect.min.x;
	if(x > mouserect.max.x)
		x = mouserect.max.x;
	if(y < mouserect.min.y)
		y = mouserect.min.y;
	if(y > mouserect.max.y)
		y = mouserect.max.y;

	zlock();
	// If reader has stopped reading, don't bother.
	// If reader is completely caught up, definitely queue.
	// Otherwise, queue only button change events.
	if(!mouse.stall)
	if(mouse.wi == mouse.ri || mouse.last.buttons != b){
		m = &mouse.last;
		m->xy.x = x;
		m->xy.y = y;
		m->buttons = b;
		m->msec = ms;

		mouse.m[mouse.wi] = *m;
		if(++mouse.wi == nelem(mouse.m))
			mouse.wi = 0;
		if(mouse.wi == mouse.ri){
			mouse.stall = 1;
			mouse.ri = 0;
			mouse.wi = 1;
			mouse.m[0] = *m;
		}
		matchmouse();
	}
	zunlock();
}

void
kputc(int c)
{
	zlock();
	kbd.r[kbd.wi++] = c;
	if(kbd.wi == nelem(kbd.r))
		kbd.wi = 0;
	if(kbd.ri == kbd.wi)
		kbd.stall = 1;
	matchkbd();
	zunlock();
}

static int alting;

void
abortcompose(void)
{
	if(alting)
		keystroke(Kalt);
}

void resizeimg(void);

void
keystroke(int c)
{
	static Rune k[10];
	static int nk;
	int i;

	if(c == Kalt){
		alting = !alting;
		nk = 0;
		return;
	}
	if(c == Kcmd+'r') {
		if(forcedpi)
			forcedpi = 0;
		else if(displaydpi >= 200)
			forcedpi = 100;
		else
			forcedpi = 225;
		resizeimg();
		return;
	}
	if(!alting){
		kputc(c);
		return;
	}
	if(nk >= nelem(k))      // should not happen
		nk = 0;
	k[nk++] = c;
	c = _latin1(k, nk);
	if(c > 0){
		alting = 0;
		kputc(c);
		nk = 0;
		return;
	}
	if(c == -1){
		alting = 0;
		for(i=0; i<nk; i++)
			kputc(k[i]);
		nk = 0;
		return;
	}
	// need more input
	return;
}
