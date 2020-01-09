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
#include "mac-screen.h"

void runmsg(Client*, Wsysmsg*);
void replymsg(Client*, Wsysmsg*);
void matchkbd(Client*);
void matchmouse(Client*);

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

void
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
void
runmsg(Client *c, Wsysmsg *m)
{
	static uchar buf[65536];
	int n;
	Memimage *i;

	switch(m->type){
	case Tinit:
		memimageinit();
		i = attachscreen(c, m->label, m->winsize);
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
		setmouse(m->mouse.xy);
		replymsg(c, m);
		break;

	case Tcursor:
		if(m->arrowcursor)
			setcursor(nil, nil);
		else
			setcursor(&m->cursor, nil);
		replymsg(c, m);
		break;

	case Tcursor2:
		if(m->arrowcursor)
			setcursor(nil, nil);
		else
			setcursor(&m->cursor, &m->cursor2);
		replymsg(c, m);
		break;

	case Tbouncemouse:
	//	_xbouncemouse(&m->mouse);
		replymsg(c, m);
		break;

	case Tlabel:
		kicklabel(m->label);
		replymsg(c, m);
		break;

	case Trdsnarf:
		m->snarf = getsnarf();
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
		topwin();
		replymsg(c, m);
		break;

	case Tresize:
		resizewindow(m->rect);
		replymsg(c, m);
		break;
	}
}

/*
 * Reply to m.
 */
QLock replylock;
void
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
void
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

/*
 * Match queued mouse reads with queued mouse events.
 */
void
matchmouse(Client *c)
{
	Wsysmsg m;

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
mousetrack(Client *c, int x, int y, int b, uint ms)
{
	Mouse *m;

	if(x < c->mouserect.min.x)
		x = c->mouserect.min.x;
	if(x > c->mouserect.max.x)
		x = c->mouserect.max.x;
	if(y < c->mouserect.min.y)
		y = c->mouserect.min.y;
	if(y > c->mouserect.max.y)
		y = c->mouserect.max.y;

	qlock(&c->inputlk);
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

void
kputc(Client *c, int ch)
{
	qlock(&c->inputlk);
	c->kbd.r[c->kbd.wi++] = ch;
	if(c->kbd.wi == nelem(c->kbd.r))
		c->kbd.wi = 0;
	if(c->kbd.ri == c->kbd.wi)
		c->kbd.stall = 1;
	matchkbd(c);
	qunlock(&c->inputlk);
}

void
abortcompose(Client *c)
{
	if(c->kbd.alting)
		keystroke(c, Kalt);
}

void
keystroke(Client *c, int ch)
{
	static Rune k[10];
	static int nk;
	int i;

	if(ch == Kalt){
		c->kbd.alting = !c->kbd.alting;
		nk = 0;
		return;
	}
	if(ch == Kcmd+'r') {
		if(c->forcedpi)
			c->forcedpi = 0;
		else if(c->displaydpi >= 200)
			c->forcedpi = 100;
		else
			c->forcedpi = 225;
		resizeimg(c);
		return;
	}
	if(!c->kbd.alting){
		kputc(c, ch);
		return;
	}
	if(nk >= nelem(k))      // should not happen
		nk = 0;
	k[nk++] = ch;
	ch = _latin1(k, nk);
	if(ch > 0){
		c->kbd.alting = 0;
		kputc(c, ch);
		nk = 0;
		return;
	}
	if(ch == -1){
		c->kbd.alting = 0;
		for(i=0; i<nk; i++)
			kputc(c, k[i]);
		nk = 0;
		return;
	}
	// need more input
	return;
}
