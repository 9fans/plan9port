#include <u.h>
#include <sys/select.h>
#include <libc.h>
#include <draw.h>
#include <cursor.h>
#include <event.h>
#include <mux.h>
#include <drawfcall.h>

typedef struct	Slave Slave;
typedef struct	Ebuf Ebuf;
extern Mouse _drawmouse;

struct Slave
{
	int	inuse;
	Ebuf	*head;		/* queue of messages for this descriptor */
	Ebuf	*tail;
	int	(*fn)(int, Event*, uchar*, int);
	Muxrpc *rpc;
	vlong nexttick;
	int fd;
	int n;
};

struct Ebuf
{
	Ebuf	*next;
	int	n;		/* number of bytes in buf */
	union {
		uchar	buf[EMAXMSG];
		Rune	rune;
		Mouse	mouse;
	} u;
};

static	Slave	eslave[MAXSLAVE];
static	int	Skeyboard = -1;
static	int	Smouse = -1;
static	int	Stimer = -1;

static	int	nslave;
static	int	newkey(ulong);
static	int	extract(int canblock);

static
Ebuf*
ebread(Slave *s)
{
	Ebuf *eb;

	while(!s->head)
		extract(1);
	eb = s->head;
	s->head = s->head->next;
	if(s->head == 0)
		s->tail = 0;
	return eb;
}

ulong
event(Event *e)
{
	return eread(~0UL, e);
}

ulong
eread(ulong keys, Event *e)
{
	Ebuf *eb;
	int i, id;

	if(keys == 0)
		return 0;
	for(;;){
		for(i=0; i<nslave; i++)
			if((keys & (1<<i)) && eslave[i].head){
				id = 1<<i;
				if(i == Smouse)
					e->mouse = emouse();
				else if(i == Skeyboard)
					e->kbdc = ekbd();
				else if(i == Stimer)
					eslave[i].head = 0;
				else{
					eb = ebread(&eslave[i]);
					e->n = eb->n;
					if(eslave[i].fn)
						id = (*eslave[i].fn)(id, e, eb->u.buf, eb->n);
					else
						memmove(e->data, eb->u.buf, eb->n);
					free(eb);
				}
				return id;
			}
		extract(1);
	}
	return 0;
}

int
ecanmouse(void)
{
	if(Smouse < 0)
		drawerror(display, "events: mouse not initialized");
	return ecanread(Emouse);
}

int
ecankbd(void)
{
	if(Skeyboard < 0)
		drawerror(display, "events: keyboard not initialzed");
	return ecanread(Ekeyboard);
}

int
ecanread(ulong keys)
{
	int i;

	for(;;){
		for(i=0; i<nslave; i++)
			if((keys & (1<<i)) && eslave[i].head)
				return 1;
		if(!extract(0))
			return 0;
	}
	return -1;
}

ulong
estartfn(ulong key, int fd, int n, int (*fn)(int, Event*, uchar*, int))
{
	int i;

	if(fd < 0)
		drawerror(display, "events: bad file descriptor");
	if(n <= 0 || n > EMAXMSG)
		n = EMAXMSG;
	i = newkey(key);
	eslave[i].fn = fn;
	eslave[i].fd = fd;
	eslave[i].n = n;
	return 1<<i;
}

ulong
estart(ulong key, int fd, int n)
{
	return estartfn(key, fd, n, nil);
}

ulong
etimer(ulong key, int n)
{
	if(Stimer != -1)
		drawerror(display, "events: timer started twice");
	Stimer = newkey(key);
	if(n <= 0)
		n = 1000;
	eslave[Stimer].n = n;
	eslave[Stimer].nexttick = nsec()+n*1000000LL;
	return 1<<Stimer;
}

void
einit(ulong keys)
{
	if(keys&Ekeyboard){
		for(Skeyboard=0; Ekeyboard & ~(1<<Skeyboard); Skeyboard++)
			;
		eslave[Skeyboard].inuse = 1;
		if(nslave <= Skeyboard)
			nslave = Skeyboard+1;
	}
	if(keys&Emouse){
		for(Smouse=0; Emouse & ~(1<<Smouse); Smouse++)
			;
		eslave[Smouse].inuse = 1;
		if(nslave <= Smouse)
			nslave = Smouse+1;
	}
}

static Ebuf*
newebuf(Slave *s, int n)
{
	Ebuf *eb;

	eb = malloc(sizeof(*eb) - sizeof(eb->u.buf) + n);
	if(eb == nil)
		drawerror(display, "events: out of memory");
	eb->n = n;
	eb->next = 0;
	if(s->head)
		s->tail = s->tail->next = eb;
	else
		s->head = s->tail = eb;
	return eb;
}

static Muxrpc*
startrpc(int type)
{
	uchar buf[100];
	Wsysmsg w;

	w.type = type;
	convW2M(&w, buf, sizeof buf);
	return muxrpcstart(display->mux, buf);
}

static int
finishrpc(Muxrpc *r, Wsysmsg *w)
{
	uchar *p;
	void *v;
	int n;

	if(!muxrpccanfinish(r, &v))
		return 0;
	p = v;
	if(p == nil)	/* eof on connection */
		exit(0);
	GET(p, n);
	convM2W(p, n, w);
	free(p);
	return 1;
}

static int
extract(int canblock)
{
	Ebuf *eb;
	int i, n, max;
	fd_set rset, wset, xset;
	struct timeval tv, *timeout;
	Wsysmsg w;
	vlong t0;

	/*
	 * Flush draw buffer before waiting for responses.
	 * Avoid doing so if buffer is empty.
	 * Also make sure that we don't interfere with app-specific locking.
	 */
	if(display->locking){
		/*
		 * if locking is being done by program,
		 * this means it can't depend on automatic
		 * flush in emouse() etc.
		 */
		if(canqlock(&display->qlock)){
			if(display->bufp > display->buf)
				flushimage(display, 1);
			unlockdisplay(display);
		}
	}else
		if(display->bufp > display->buf)
			flushimage(display, 1);

	/*
	 * Set up for select.
	 */
	FD_ZERO(&rset);
	FD_ZERO(&wset);
	FD_ZERO(&xset);
	max = -1;
	timeout = nil;
	for(i=0; i<nslave; i++){
		if(!eslave[i].inuse)
			continue;
		if(i == Smouse){
			if(eslave[i].rpc == nil)
				eslave[i].rpc = startrpc(Trdmouse);
			if(eslave[i].rpc){
				/* if ready, don't block in select */
				if(eslave[i].rpc->p)
					canblock = 0;
				FD_SET(display->srvfd, &rset);
				FD_SET(display->srvfd, &xset);
				if(display->srvfd > max)
					max = display->srvfd;
			}
		}else if(i == Skeyboard){
			if(eslave[i].rpc == nil)
				eslave[i].rpc = startrpc(Trdkbd4);
			if(eslave[i].rpc){
				/* if ready, don't block in select */
				if(eslave[i].rpc->p)
					canblock = 0;
				FD_SET(display->srvfd, &rset);
				FD_SET(display->srvfd, &xset);
				if(display->srvfd > max)
					max = display->srvfd;
			}
		}else if(i == Stimer){
			t0 = nsec();
			if(t0 >= eslave[i].nexttick){
				tv.tv_sec = 0;
				tv.tv_usec = 0;
			}else{
				tv.tv_sec = (eslave[i].nexttick-t0)/1000000000;
				tv.tv_usec = (eslave[i].nexttick-t0)%1000000000 / 1000;
			}
			timeout = &tv;
		}else{
			FD_SET(eslave[i].fd, &rset);
			FD_SET(eslave[i].fd, &xset);
			if(eslave[i].fd > max)
				max = eslave[i].fd;
		}
	}

	if(!canblock){
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		timeout = &tv;
	}

	if(select(max+1, &rset, &wset, &xset, timeout) < 0)
		drawerror(display, "select failure");

	/*
	 * Look to see what can proceed.
	 */
	n = 0;
	for(i=0; i<nslave; i++){
		if(!eslave[i].inuse)
			continue;
		if(i == Smouse){
			if(finishrpc(eslave[i].rpc, &w)){
				eslave[i].rpc = nil;
				eb = newebuf(&eslave[i], sizeof(Mouse));
				_drawmouse = w.mouse;
				eb->u.mouse = w.mouse;
				if(w.resized)
					eresized(1);
				n++;
			}
		}else if(i == Skeyboard){
			if(finishrpc(eslave[i].rpc, &w)){
				eslave[i].rpc = nil;
				eb = newebuf(&eslave[i], sizeof(Rune)+2);	/* +8: alignment */
				eb->u.rune = w.rune;
				n++;
			}
		}else if(i == Stimer){
			t0 = nsec();
			while(t0 > eslave[i].nexttick){
				eslave[i].nexttick += eslave[i].n*1000000LL;
				eslave[i].head = (Ebuf*)1;
				n++;
			}
		}else{
			if(FD_ISSET(eslave[i].fd, &rset)){
				eb = newebuf(&eslave[i], eslave[i].n);
				eb->n = read(eslave[i].fd, eb->u.buf, eslave[i].n);
				n++;
			}
		}
	}
	return n;
}

static int
newkey(ulong key)
{
	int i;

	for(i=0; i<MAXSLAVE; i++)
		if((key & ~(1<<i)) == 0 && eslave[i].inuse == 0){
			if(nslave <= i)
				nslave = i + 1;
			eslave[i].inuse = 1;
			return i;
		}
	drawerror(display, "events: bad slave assignment");
	return 0;
}

Mouse
emouse(void)
{
	Mouse m;
	Ebuf *eb;

	if(Smouse < 0)
		drawerror(display, "events: mouse not initialized");
	eb = ebread(&eslave[Smouse]);
	m = eb->u.mouse;
	free(eb);
	return m;
}

int
ekbd(void)
{
	Ebuf *eb;
	int c;

	if(Skeyboard < 0)
		drawerror(display, "events: keyboard not initialzed");
	eb = ebread(&eslave[Skeyboard]);
	c = eb->u.rune;
	free(eb);
	return c;
}

void
emoveto(Point pt)
{
	_displaymoveto(display, pt);
}

void
esetcursor(Cursor *c)
{
	_displaycursor(display, c, nil);
}

void
esetcursor2(Cursor *c, Cursor2 *c2)
{
	_displaycursor(display, c, c2);
}

int
ereadmouse(Mouse *m)
{
	int resized;

	resized = 0;
	if(_displayrdmouse(display, m, &resized) < 0)
		return -1;
	if(resized)
		eresized(1);
	return 1;
}
