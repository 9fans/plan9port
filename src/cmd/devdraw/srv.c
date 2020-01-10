/*
 * Window system protocol server.
 */

#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>
#include <drawfcall.h>
#include "devdraw.h"

static void runmsg(Client*, Wsysmsg*);
static void replymsg(Client*, Wsysmsg*);
static void matchkbd(Client*);
static void matchmouse(Client*);

int trace = 0;

void
servep9p(Client *c)
{
	uchar buf[4], *mbuf;
	int nmbuf, n, nn;
	Wsysmsg m;

	fmtinstall('W', drawfcallfmt);

	mbuf = nil;
	nmbuf = 0;
	while((n = read(c->rfd, buf, 4)) == 4){
		GET(buf, n);
		if(n > nmbuf){
			free(mbuf);
			mbuf = malloc(4+n);
			if(mbuf == nil)
				sysfatal("malloc: %r");
			nmbuf = n;
		}
		memmove(mbuf, buf, 4);
		nn = readn(c->rfd, mbuf+4, n-4);
		if(nn != n-4)
			sysfatal("eof during message");

		/* pick off messages one by one */
		if(convM2W(mbuf, nn+4, &m) <= 0)
			sysfatal("cannot convert message");
		if(trace) fprint(2, "%ud [%d] <- %W\n", nsec()/1000000, threadid(), &m);
		runmsg(c, &m);
	}
}

static void
replyerror(Client *c, Wsysmsg *m)
{
	char err[256];

	rerrstr(err, sizeof err);
	m->type = Rerror;
	m->error = err;
	replymsg(c, m);
}

/*
 * Handle a single wsysmsg.
 * Might queue for later (kbd, mouse read)
 */
static void
runmsg(Client *c, Wsysmsg *m)
{
	static uchar buf[65536];
	int n;
	Memimage *i;

	switch(m->type){
	case Tinit:
		memimageinit();
		i = rpc_attachscreen(c, m->label, m->winsize);
		_initdisplaymemimage(c, i);
		replymsg(c, m);
		break;

	case Trdmouse:
		qlock(&c->inputlk);
		c->mousetags.t[c->mousetags.wi++] = m->tag;
		if(c->mousetags.wi == nelem(c->mousetags.t))
			c->mousetags.wi = 0;
		if(c->mousetags.wi == c->mousetags.ri)
			sysfatal("too many queued mouse reads");
		c->mouse.stall = 0;
		matchmouse(c);
		qunlock(&c->inputlk);
		break;

	case Trdkbd:
		qlock(&c->inputlk);
		c->kbdtags.t[c->kbdtags.wi++] = m->tag;
		if(c->kbdtags.wi == nelem(c->kbdtags.t))
			c->kbdtags.wi = 0;
		if(c->kbdtags.wi == c->kbdtags.ri)
			sysfatal("too many queued keyboard reads");
		c->kbd.stall = 0;
		matchkbd(c);
		qunlock(&c->inputlk);
		break;

	case Tmoveto:
		rpc_setmouse(c, m->mouse.xy);
		replymsg(c, m);
		break;

	case Tcursor:
		if(m->arrowcursor)
			rpc_setcursor(c, nil, nil);
		else {
			scalecursor(&m->cursor2, &m->cursor);
			rpc_setcursor(c, &m->cursor, &m->cursor2);
		}
		replymsg(c, m);
		break;

	case Tcursor2:
		if(m->arrowcursor)
			rpc_setcursor(c, nil, nil);
		else
			rpc_setcursor(c, &m->cursor, &m->cursor2);
		replymsg(c, m);
		break;

	case Tbouncemouse:
	//	_xbouncemouse(&m->mouse);
		replymsg(c, m);
		break;

	case Tlabel:
		rpc_setlabel(c, m->label);
		replymsg(c, m);
		break;

	case Trdsnarf:
		m->snarf = rpc_getsnarf();
		replymsg(c, m);
		free(m->snarf);
		break;

	case Twrsnarf:
		putsnarf(m->snarf);
		replymsg(c, m);
		break;

	case Trddraw:
		qlock(&c->inputlk);
		n = m->count;
		if(n > sizeof buf)
			n = sizeof buf;
		n = _drawmsgread(c, buf, n);
		if(n < 0)
			replyerror(c, m);
		else{
			m->count = n;
			m->data = buf;
			replymsg(c, m);
		}
		qunlock(&c->inputlk);
		break;

	case Twrdraw:
		qlock(&c->inputlk);
		if(_drawmsgwrite(c, m->data, m->count) < 0)
			replyerror(c, m);
		else
			replymsg(c, m);
		qunlock(&c->inputlk);
		break;

	case Ttop:
		rpc_topwin(c);
		replymsg(c, m);
		break;

	case Tresize:
		rpc_resizewindow(c, m->rect);
		replymsg(c, m);
		break;
	}
}

/*
 * Reply to m.
 */
QLock replylock;
static void
replymsg(Client *c, Wsysmsg *m)
{
	int n;
	static uchar *mbuf;
	static int nmbuf;

	/* T -> R msg */
	if(m->type%2 == 0)
		m->type++;

	if(trace) fprint(2, "%ud [%d] -> %W\n", nsec()/1000000, threadid(), m);
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
	if(write(c->wfd, mbuf, n) != n)
		sysfatal("write: %r");
	qunlock(&replylock);
}

/*
 * Match queued kbd reads with queued kbd characters.
 */
static void
matchkbd(Client *c)
{
	Wsysmsg m;

	if(c->kbd.stall)
		return;
	while(c->kbd.ri != c->kbd.wi && c->kbdtags.ri != c->kbdtags.wi){
		m.type = Rrdkbd;
		m.tag = c->kbdtags.t[c->kbdtags.ri++];
		if(c->kbdtags.ri == nelem(c->kbdtags.t))
			c->kbdtags.ri = 0;
		m.rune = c->kbd.r[c->kbd.ri++];
		if(c->kbd.ri == nelem(c->kbd.r))
			c->kbd.ri = 0;
		replymsg(c, &m);
	}
}

// matchmouse matches queued mouse reads with queued mouse events.
// It must be called with c->inputlk held.
static void
matchmouse(Client *c)
{
	Wsysmsg m;

	if(canqlock(&c->inputlk)) {
		fprint(2, "misuse of matchmouse\n");
		abort();
	}

	while(c->mouse.ri != c->mouse.wi && c->mousetags.ri != c->mousetags.wi){
		m.type = Rrdmouse;
		m.tag = c->mousetags.t[c->mousetags.ri++];
		if(c->mousetags.ri == nelem(c->mousetags.t))
			c->mousetags.ri = 0;
		m.mouse = c->mouse.m[c->mouse.ri];
		m.resized = c->mouse.resized;
		c->mouse.resized = 0;
		/*
		if(m.resized)
			fprint(2, "sending resize\n");
		*/
		c->mouse.ri++;
		if(c->mouse.ri == nelem(c->mouse.m))
			c->mouse.ri = 0;
		replymsg(c, &m);
	}
}

void
gfx_mousetrack(Client *c, int x, int y, int b, uint ms)
{
	Mouse *m;

	qlock(&c->inputlk);
	if(x < c->mouserect.min.x)
		x = c->mouserect.min.x;
	if(x > c->mouserect.max.x)
		x = c->mouserect.max.x;
	if(y < c->mouserect.min.y)
		y = c->mouserect.min.y;
	if(y > c->mouserect.max.y)
		y = c->mouserect.max.y;

	// If reader has stopped reading, don't bother.
	// If reader is completely caught up, definitely queue.
	// Otherwise, queue only button change events.
	if(!c->mouse.stall)
	if(c->mouse.wi == c->mouse.ri || c->mouse.last.buttons != b){
		m = &c->mouse.last;
		m->xy.x = x;
		m->xy.y = y;
		m->buttons = b;
		m->msec = ms;

		c->mouse.m[c->mouse.wi] = *m;
		if(++c->mouse.wi == nelem(c->mouse.m))
			c->mouse.wi = 0;
		if(c->mouse.wi == c->mouse.ri){
			c->mouse.stall = 1;
			c->mouse.ri = 0;
			c->mouse.wi = 1;
			c->mouse.m[0] = *m;
		}
		matchmouse(c);
	}
	qunlock(&c->inputlk);
}

// kputc adds ch to the keyboard buffer.
// It must be called with c->inputlk held.
static void
kputc(Client *c, int ch)
{
	if(canqlock(&c->inputlk)) {
		fprint(2, "misuse of kputc\n");
		abort();
	}

	c->kbd.r[c->kbd.wi++] = ch;
	if(c->kbd.wi == nelem(c->kbd.r))
		c->kbd.wi = 0;
	if(c->kbd.ri == c->kbd.wi)
		c->kbd.stall = 1;
	matchkbd(c);
}

// gfx_abortcompose stops any pending compose sequence,
// because a mouse button has been clicked.
// It is called from the graphics thread with no locks held.
void
gfx_abortcompose(Client *c)
{
	qlock(&c->inputlk);
	if(c->kbd.alting) {
		c->kbd.alting = 0;
		c->kbd.nk = 0;
	}
	qunlock(&c->inputlk);
}

// gfx_keystroke records a single-rune keystroke.
// It is called from the graphics thread with no locks held.
void
gfx_keystroke(Client *c, int ch)
{
	int i;

	qlock(&c->inputlk);
	if(ch == Kalt){
		c->kbd.alting = !c->kbd.alting;
		c->kbd.nk = 0;
		qunlock(&c->inputlk);
		return;
	}
	if(ch == Kcmd+'r') {
		if(c->forcedpi)
			c->forcedpi = 0;
		else if(c->displaydpi >= 200)
			c->forcedpi = 100;
		else
			c->forcedpi = 225;
		qunlock(&c->inputlk);
		rpc_resizeimg(c);
		return;
	}
	if(!c->kbd.alting){
		kputc(c, ch);
		qunlock(&c->inputlk);
		return;
	}
	if(c->kbd.nk >= nelem(c->kbd.k))      // should not happen
		c->kbd.nk = 0;
	c->kbd.k[c->kbd.nk++] = ch;
	ch = _latin1(c->kbd.k, c->kbd.nk);
	if(ch > 0){
		c->kbd.alting = 0;
		kputc(c, ch);
		c->kbd.nk = 0;
		qunlock(&c->inputlk);
		return;
	}
	if(ch == -1){
		c->kbd.alting = 0;
		for(i=0; i<c->kbd.nk; i++)
			kputc(c, c->kbd.k[i]);
		c->kbd.nk = 0;
		qunlock(&c->inputlk);
		return;
	}
	// need more input
	qunlock(&c->inputlk);
	return;
}
