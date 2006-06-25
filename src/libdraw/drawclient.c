/* Copyright (c) 2006 Russ Cox */

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <mouse.h>
#include <cursor.h>
#include <drawfcall.h>
#include <mux.h>

int chattydrawclient;

static int	drawgettag(Mux *mux, void *vmsg);
static void*	drawrecv(Mux *mux);
static int	drawsend(Mux *mux, void *vmsg);
static int	drawsettag(Mux *mux, void *vmsg, uint tag);

int
_displayconnect(Display *d)
{
	int pid, p[2];
	
	fmtinstall('W', drawfcallfmt);
	fmtinstall('H', encodefmt);
	
	if(pipe(p) < 0)
		return -1;
	if((pid=fork()) < 0){
		close(p[0]);
		close(p[1]);
		return -1;
	}
	if(pid == 0){
		close(p[0]);
		dup(p[1], 0);
		dup(p[1], 1);
		/* execl("strace", "strace", "-o", "drawsrv.out", "drawsrv", nil); */
		execl("drawsrv", "drawsrv", nil);
		sysfatal("exec drawsrv: %r");
	}
	close(p[1]);
	d->srvfd = p[0];
	return 0;
}

int
_displaymux(Display *d)
{
	if((d->mux = mallocz(sizeof(*d->mux), 1)) == nil)
		return -1;

	d->mux->mintag = 1;
	d->mux->maxtag = 255;
	d->mux->send = drawsend;
	d->mux->recv = drawrecv;
	d->mux->gettag = drawgettag;
	d->mux->settag = drawsettag;
	d->mux->aux = d;
	muxinit(d->mux);
	
	return 0;
}

#define GET(p, x) \
	((x) = (((p)[0] << 24) | ((p)[1] << 16) | ((p)[2] << 8) | ((p)[3])))

static int
drawsend(Mux *mux, void *vmsg)
{
	int n;
	uchar *msg;
	Display *d;
	
	msg = vmsg;
	GET(msg, n);
	d = mux->aux;
	return write(d->srvfd, msg, n);
}

static void*
drawrecv(Mux *mux)
{
	int n;
	uchar buf[4], *p;
	Display *d;
	
	d = mux->aux;
	if((n=readn(d->srvfd, buf, 4)) != 4){
fprint(2, "readn 4 got %d: %r\n", n);
		return nil;
	}
	GET(buf, n);
	p = malloc(n);
	if(p == nil){
		fprint(2, "out of memory allocating %d in drawrecv\n", n);
		return nil;
	}
	memmove(p, buf, 4);
	if(readn(d->srvfd, p+4, n-4) != n-4){
fprint(2, "short readn\n");
		return nil;
	}
	return p;
}

static int
drawgettag(Mux *mux, void *vmsg)
{
	USED(mux);
	uchar *msg;
	
	msg = vmsg;
	return msg[4];
}

static int
drawsettag(Mux *mux, void *vmsg, uint tag)
{
	USED(mux);
	uchar *msg;
	
	msg = vmsg;
	msg[4] = tag;
	return 0;
}

static int
displayrpc(Display *d, Wsysmsg *tx, Wsysmsg *rx, void **freep)
{
	int n, nn;
	void *tpkt, *rpkt;
	
	n = sizeW2M(tx);
	tpkt = malloc(n);
	if(freep)
		*freep = nil;
	if(tpkt == nil)
		return -1;
	tx->tag = 0;
	if(chattydrawclient)
		fprint(2, "<- %W\n", tx);
	nn = convW2M(tx, tpkt, n);
	if(nn != n){
		free(tpkt);
		werrstr("drawclient: sizeW2M convW2M mismatch");
		fprint(2, "%r\n");
		return -1;
	}
	rpkt = muxrpc(d->mux, tpkt);
	free(tpkt);
	if(rpkt == nil){
		werrstr("muxrpc: %r");
		return -1;
	}
	GET((uchar*)rpkt, n);
	nn = convM2W(rpkt, n, rx);
	if(nn != n){
		free(rpkt);
		werrstr("drawclient: convM2W packet size mismatch %d %d %.*H", n, nn, n, rpkt);
		fprint(2, "%r\n");
		return -1;
	}
	if(chattydrawclient)
		fprint(2, "-> %W\n", rx);
	if(rx->type == Rerror){
		werrstr("%s", rx->error);
		free(rpkt);
		return -1;
	}
	if(rx->type != tx->type+1){
		werrstr("packet type mismatch -- tx %d rx %d",
			tx->type, rx->type);
		free(rpkt);
		return -1;
	}
	if(freep)
		*freep = rpkt;
	else
		free(rpkt);
	return 0;
}

int
_displayinit(Display *d, char *label, char *winsize)
{
	Wsysmsg tx, rx;

	tx.type = Tinit;
	tx.label = "";
	tx.winsize = "";
	return displayrpc(d, &tx, &rx, nil);
}

int
_displayrdmouse(Display *d, Mouse *m, int *resized)
{
	Wsysmsg tx, rx;

	tx.type = Trdmouse;
	if(displayrpc(d, &tx, &rx, nil) < 0)
		return -1;
	*m = rx.mouse;
	*resized = rx.resized;
	return 0;
}

int
_displayrdkbd(Display *d, Rune *r)
{
	Wsysmsg tx, rx;

	tx.type = Trdkbd;
	if(displayrpc(d, &tx, &rx, nil) < 0)
		return -1;
	*r = rx.rune;
	return 0;
}

int
_displaymoveto(Display *d, Point p)
{
	Wsysmsg tx, rx;

	tx.type = Tmoveto;
	tx.mouse.xy = p;
	return displayrpc(d, &tx, &rx, nil);
}

int
_displaycursor(Display *d, Cursor *c)
{
	Wsysmsg tx, rx;
	
	tx.type = Tcursor;
	if(c == nil){
		memset(&tx.cursor, 0, sizeof tx.cursor);
		tx.arrowcursor = 1;
	}else{
		tx.arrowcursor = 0;
		tx.cursor = *c;
	}
	return displayrpc(d, &tx, &rx, nil);
}

int
_displaybouncemouse(Display *d, Mouse *m)
{
	Wsysmsg tx, rx;
	
	tx.type = Tbouncemouse;
	tx.mouse = *m;
	return displayrpc(d, &tx, &rx, nil);
}

int
_displaylabel(Display *d, char *label)
{
	Wsysmsg tx, rx;
	
	tx.type = Tlabel;
	tx.label = label;
	return displayrpc(d, &tx, &rx, nil);
}

char*
_displayrdsnarf(Display *d)
{
	void *p;
	char *s;
	Wsysmsg tx, rx;
	
	tx.type = Trdsnarf;
	if(displayrpc(d, &tx, &rx, &p) < 0)
		return nil;
	s = strdup(rx.snarf);
	free(p);
	return s;
}

int
_displaywrsnarf(Display *d, char *snarf)
{
	Wsysmsg tx, rx;
	
	tx.type = Twrsnarf;
	tx.snarf = snarf;
	return displayrpc(d, &tx, &rx, nil);
}

int
_displayrddraw(Display *d, void *v, int n)
{
	void *p;
	Wsysmsg tx, rx;
	
	tx.type = Trddraw;
	tx.count = n;
	if(displayrpc(d, &tx, &rx, &p) < 0)
		return -1;
	memmove(v, rx.data, rx.count);
	free(p);
	return rx.count;
}

int
_displaywrdraw(Display *d, void *v, int n)
{
	Wsysmsg tx, rx;
	
	tx.type = Twrdraw;
	tx.count = n;
	tx.data = v;
	if(displayrpc(d, &tx, &rx, nil) < 0)
		return -1;
	return rx.count;
}

int
_displaytop(Display *d)
{
	Wsysmsg tx, rx;

	tx.type = Ttop;
	return displayrpc(d, &tx, &rx, nil);
}

int
_displayresize(Display *d, Rectangle r)
{
	Wsysmsg tx, rx;
	
	tx.type = Tresize;
	tx.rect = r;
	return displayrpc(d, &tx, &rx, nil);
}

