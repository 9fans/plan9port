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
static void serveproc(void*);
static void listenproc(void*);
Client *client0;

int trace = 0;
static char *srvname;
static int afd;
static char adir[40];

static void
usage(void)
{
	fprint(2, "usage: devdraw (don't run directly)\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char **argv)
{
	char *p;

	ARGBEGIN{
	case 'D':		/* for good ps -a listings */
		break;
	case 'f':		/* fall through for backward compatibility */
	case 'g':
	case 'b':
		break;
	case 's':
		// TODO: Update usage, man page.
		srvname = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	memimageinit();
	fmtinstall('H', encodefmt);
	if((p = getenv("DEVDRAWTRACE")) != nil)
		trace = atoi(p);

	if(srvname == nil) {
		client0 = mallocz(sizeof(Client), 1);
		if(client0 == nil){
			fprint(2, "initdraw: allocating client0: out of memory");
			abort();
		}
		client0->displaydpi = 100;
		client0->rfd = 3;
		client0->wfd = 4;

		/*
		 * Move the protocol off stdin/stdout so that
		 * any inadvertent prints don't screw things up.
		 */
		dup(0,3);
		dup(1,4);
		close(0);
		close(1);
		open("/dev/null", OREAD);
		open("/dev/null", OWRITE);
	}

	fmtinstall('W', drawfcallfmt);
	gfx_main();
}

void
gfx_started(void)
{
	char *ns, *addr;

	if(srvname == nil) {
		// Legacy mode: serving single client on pipes.
		proccreate(serveproc, client0, 0);
		return;
	}

	// Server mode.
	if((ns = getns()) == nil)
		sysfatal("out of memory");

	addr = smprint("unix!%s/%s", ns, srvname);
	free(ns);
	if(addr == nil)
		sysfatal("out of memory");

	if((afd = announce(addr, adir)) < 0)
		sysfatal("announce %s: %r", addr);

	proccreate(listenproc, nil, 0);
}

static void
listenproc(void *v)
{
	Client *c;
	int fd;
	char dir[40];

	USED(v);

	for(;;) {
		fd = listen(adir, dir);
		if(fd < 0)
			sysfatal("listen: %r");
		c = mallocz(sizeof(Client), 1);
		if(c == nil){
			fprint(2, "initdraw: allocating client0: out of memory");
			abort();
		}
		c->displaydpi = 100;
		c->rfd = fd;
		c->wfd = fd;
		proccreate(serveproc, c, 0);
	}
}

static void
serveproc(void *v)
{
	Client *c;
	uchar buf[4], *mbuf;
	int nmbuf, n, nn;
	Wsysmsg m;

	c = v;
	mbuf = nil;
	nmbuf = 0;
	while((n = read(c->rfd, buf, 4)) == 4){
		GET(buf, n);
		if(n > nmbuf){
			free(mbuf);
			mbuf = malloc(4+n);
			if(mbuf == nil)
				sysfatal("out of memory");
			nmbuf = n;
		}
		memmove(mbuf, buf, 4);
		nn = readn(c->rfd, mbuf+4, n-4);
		if(nn != n-4) {
			fprint(2, "serveproc: eof during message\n");
			break;
		}

		/* pick off messages one by one */
		if(convM2W(mbuf, nn+4, &m) <= 0) {
			fprint(2, "serveproc: cannot convert message\n");
			break;
		}
		if(trace) fprint(2, "%ud [%d] <- %W\n", nsec()/1000000, threadid(), &m);
		runmsg(c, &m);
	}

	if(c == client0) {
		rpc_shutdown();
		threadexitsall(nil);
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
	case Tctxt:
		c->wsysid = strdup(m->id);
		replymsg(c, m);
		break;

	case Tinit:
		i = rpc_attach(c, m->label, m->winsize);
		if(i == nil) {
			replyerror(c, m);
			break;
		}
		draw_initdisplaymemimage(c, i);
		replymsg(c, m);
		break;

	case Trdmouse:
		qlock(&c->eventlk);
		if((c->mousetags.wi+1)%nelem(c->mousetags.t) == c->mousetags.ri) {
			qunlock(&c->eventlk);
			werrstr("too many queued mouse reads");
			replyerror(c, m);
			break;
		}
		c->mousetags.t[c->mousetags.wi++] = m->tag;
		if(c->mousetags.wi == nelem(c->mousetags.t))
			c->mousetags.wi = 0;
		c->mouse.stall = 0;
		matchmouse(c);
		qunlock(&c->eventlk);
		break;

	case Trdkbd:
	case Trdkbd4:
		qlock(&c->eventlk);
		if((c->kbdtags.wi+1)%nelem(c->kbdtags.t) == c->kbdtags.ri) {
			qunlock(&c->eventlk);
			werrstr("too many queued keyboard reads");
			replyerror(c, m);
			break;
		}
		c->kbdtags.t[c->kbdtags.wi++] = (m->tag<<1) | (m->type==Trdkbd4);
		if(c->kbdtags.wi == nelem(c->kbdtags.t))
			c->kbdtags.wi = 0;
		c->kbd.stall = 0;
		matchkbd(c);
		qunlock(&c->eventlk);
		break;

	case Tmoveto:
		c->impl->rpc_setmouse(c, m->mouse.xy);
		replymsg(c, m);
		break;

	case Tcursor:
		if(m->arrowcursor)
			c->impl->rpc_setcursor(c, nil, nil);
		else {
			scalecursor(&m->cursor2, &m->cursor);
			c->impl->rpc_setcursor(c, &m->cursor, &m->cursor2);
		}
		replymsg(c, m);
		break;

	case Tcursor2:
		if(m->arrowcursor)
			c->impl->rpc_setcursor(c, nil, nil);
		else
			c->impl->rpc_setcursor(c, &m->cursor, &m->cursor2);
		replymsg(c, m);
		break;

	case Tbouncemouse:
		c->impl->rpc_bouncemouse(c, m->mouse);
		replymsg(c, m);
		break;

	case Tlabel:
		c->impl->rpc_setlabel(c, m->label);
		replymsg(c, m);
		break;

	case Trdsnarf:
		m->snarf = rpc_getsnarf();
		replymsg(c, m);
		free(m->snarf);
		break;

	case Twrsnarf:
		rpc_putsnarf(m->snarf);
		replymsg(c, m);
		break;

	case Trddraw:
		n = m->count;
		if(n > sizeof buf)
			n = sizeof buf;
		n = draw_dataread(c, buf, n);
		if(n < 0)
			replyerror(c, m);
		else{
			m->count = n;
			m->data = buf;
			replymsg(c, m);
		}
		break;

	case Twrdraw:
		if(draw_datawrite(c, m->data, m->count) < 0)
			replyerror(c, m);
		else
			replymsg(c, m);
		break;

	case Ttop:
		c->impl->rpc_topwin(c);
		replymsg(c, m);
		break;

	case Tresize:
		c->impl->rpc_resizewindow(c, m->rect);
		replymsg(c, m);
		break;
	}
}

/*
 * Reply to m.
 */
static void
replymsg(Client *c, Wsysmsg *m)
{
	int n;

	/* T -> R msg */
	if(m->type%2 == 0)
		m->type++;

	if(trace) fprint(2, "%ud [%d] -> %W\n", nsec()/1000000, threadid(), m);
	/* copy to output buffer */
	n = sizeW2M(m);

	qlock(&c->wfdlk);
	if(n > c->nmbuf){
		free(c->mbuf);
		c->mbuf = malloc(n);
		if(c->mbuf == nil)
			sysfatal("out of memory");
		c->nmbuf = n;
	}
	convW2M(m, c->mbuf, n);
	if(write(c->wfd, c->mbuf, n) != n)
		fprint(2, "client write: %r\n");
	qunlock(&c->wfdlk);
}

/*
 * Match queued kbd reads with queued kbd characters.
 */
static void
matchkbd(Client *c)
{
	int tag;
	Wsysmsg m;

	if(c->kbd.stall)
		return;
	while(c->kbd.ri != c->kbd.wi && c->kbdtags.ri != c->kbdtags.wi){
		tag = c->kbdtags.t[c->kbdtags.ri++];
		m.type = Rrdkbd;
		if(tag&1)
			m.type = Rrdkbd4;
		m.tag = tag>>1;
		if(c->kbdtags.ri == nelem(c->kbdtags.t))
			c->kbdtags.ri = 0;
		m.rune = c->kbd.r[c->kbd.ri++];
		if(c->kbd.ri == nelem(c->kbd.r))
			c->kbd.ri = 0;
		replymsg(c, &m);
	}
}

// matchmouse matches queued mouse reads with queued mouse events.
// It must be called with c->eventlk held.
static void
matchmouse(Client *c)
{
	Wsysmsg m;

	if(canqlock(&c->eventlk)) {
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
gfx_mouseresized(Client *c)
{
	gfx_mousetrack(c, -1, -1, -1, -1);
}

void
gfx_mousetrack(Client *c, int x, int y, int b, uint ms)
{
	Mouse *m;

	qlock(&c->eventlk);
	if(x == -1 && y == -1 && b == -1 && ms == -1) {
		Mouse *copy;
		// repeat last mouse event for resize
		if(c->mouse.ri == 0)
			copy = &c->mouse.m[nelem(c->mouse.m)-1];
		else
			copy = &c->mouse.m[c->mouse.ri-1];
		x = copy->xy.x;
		y = copy->xy.y;
		b = copy->buttons;
		ms = copy->msec;
		c->mouse.resized = 1;
	}
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
	qunlock(&c->eventlk);
}

// kputc adds ch to the keyboard buffer.
// It must be called with c->eventlk held.
static void
kputc(Client *c, int ch)
{
	if(canqlock(&c->eventlk)) {
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
	qlock(&c->eventlk);
	if(c->kbd.alting) {
		c->kbd.alting = 0;
		c->kbd.nk = 0;
	}
	qunlock(&c->eventlk);
}

// gfx_keystroke records a single-rune keystroke.
// It is called from the graphics thread with no locks held.
void
gfx_keystroke(Client *c, int ch)
{
	int i;

	qlock(&c->eventlk);
	if(ch == Kalt){
		c->kbd.alting = !c->kbd.alting;
		c->kbd.nk = 0;
		qunlock(&c->eventlk);
		return;
	}
	if(ch == Kcmd+'r') {
		if(c->forcedpi)
			c->forcedpi = 0;
		else if(c->displaydpi >= 200)
			c->forcedpi = 100;
		else
			c->forcedpi = 225;
		qunlock(&c->eventlk);
		c->impl->rpc_resizeimg(c);
		return;
	}
	if(!c->kbd.alting){
		kputc(c, ch);
		qunlock(&c->eventlk);
		return;
	}
	if(c->kbd.nk >= nelem(c->kbd.k))      // should not happen
		c->kbd.nk = 0;
	c->kbd.k[c->kbd.nk++] = ch;
	ch = latin1(c->kbd.k, c->kbd.nk);
	if(ch > 0){
		c->kbd.alting = 0;
		kputc(c, ch);
		c->kbd.nk = 0;
		qunlock(&c->eventlk);
		return;
	}
	if(ch == -1){
		c->kbd.alting = 0;
		for(i=0; i<c->kbd.nk; i++)
			kputc(c, c->kbd.k[i]);
		c->kbd.nk = 0;
		qunlock(&c->eventlk);
		return;
	}
	// need more input
	qunlock(&c->eventlk);
	return;
}
