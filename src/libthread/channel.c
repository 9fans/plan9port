#include "threadimpl.h"

static Lock chanlock;		/* central channel access lock */

static void enqueue(Alt*, Channel**);
static void dequeue(Alt*);
static int altexec(Alt*, int);

int _threadhighnentry;
int _threadnalt;

static void
setuserpc(ulong pc)
{
	Thread *t;

	t = _threadgetproc()->thread;
	if(t)
		t->userpc = pc;
}

static int
canexec(Alt *a)
{
	int i, otherop;
	Channel *c;

	c = a->c;
	/* are there senders or receivers blocked? */
	otherop = (CHANSND+CHANRCV) - a->op;
	for(i=0; i<c->nentry; i++)
		if(c->qentry[i] && c->qentry[i]->op==otherop && *c->qentry[i]->tag==nil){
			_threaddebug(DBGCHAN, "can rendez alt %p chan %p", a, c);
			return 1;
		}

	/* is there room in the channel? */
	if((a->op==CHANSND && c->n < c->s)
	|| (a->op==CHANRCV && c->n > 0)){
		_threaddebug(DBGCHAN, "can buffer alt %p chan %p", a, c);
		return 1;
	}

	return 0;
}

static void
_chanfree(Channel *c)
{
	int i, inuse;

	inuse = 0;
	for(i = 0; i < c->nentry; i++)
		if(c->qentry[i])
			inuse = 1;
	if(inuse)
		c->freed = 1;
	else{
		if(c->qentry)
			free(c->qentry);
		free(c);
	}
}

void
chanfree(Channel *c)
{
	lock(&chanlock);
	_chanfree(c);
	unlock(&chanlock);
}

int
chaninit(Channel *c, int elemsize, int elemcnt)
{
	if(elemcnt < 0 || elemsize <= 0 || c == nil)
		return -1;
	memset(c, 0, sizeof *c);
	c->e = elemsize;
	c->s = elemcnt;
	_threaddebug(DBGCHAN, "chaninit %p", c);
	return 1;
}

Channel*
chancreate(int elemsize, int elemcnt)
{
	Channel *c;

	if(elemcnt < 0 || elemsize <= 0)
		return nil;
	c = _threadmalloc(sizeof(Channel)+elemsize*elemcnt, 1);
	c->e = elemsize;
	c->s = elemcnt;
	_threaddebug(DBGCHAN, "chancreate %p", c);
	return c;
}

static int
_alt(Alt *alts)
{
	Alt *a, *xa;
	Channel *volatile c;
	int n, s;
	ulong r;
	Thread *t;

	/*
	 * The point of going splhi here is that note handlers
	 * might reasonably want to use channel operations,
	 * but that will hang if the note comes while we hold the
	 * chanlock.  Instead, we delay the note until we've dropped
	 * the lock.
	 */

	/*
	 * T might be nil here -- the scheduler sends on threadwaitchan
	 * directly (in non-blocking mode, of course!).
	 */
	t = _threadgetproc()->thread;
	if((t && t->moribund) || _threadexitsallstatus)
		yield();	/* won't return */
	s = _procsplhi();
	lock(&chanlock);

	/* test whether any channels can proceed */
	n = 0;
	a = nil;

	for(xa=alts; xa->op!=CHANEND && xa->op!=CHANNOBLK; xa++){
		xa->entryno = -1;
		if(xa->op == CHANNOP)
			continue;
		
		c = xa->c;
		if(c==nil){
			unlock(&chanlock);
			_procsplx(s);
			return -1;
		}
		if(canexec(xa))
			if(nrand(++n) == 0)
				a = xa;
	}

	if(a==nil){
		/* nothing can proceed */
		if(xa->op == CHANNOBLK){
			unlock(&chanlock);
			_procsplx(s);
_threadnalt++;
			return xa - alts;
		}

		/* enqueue on all channels. */
		c = nil;
		for(xa=alts; xa->op!=CHANEND; xa++){
			if(xa->op==CHANNOP)
				continue;
			enqueue(xa, (Channel**)&c);
		}

		/*
		 * wait for successful rendezvous.
		 * we can't just give up if the rendezvous
		 * is interrupted -- someone else might come
		 * along and try to rendezvous with us, so
		 * we need to be here.
		 */
	    Again:
		t->alt = alts;
		t->chan = Chanalt;

		unlock(&chanlock);
		_procsplx(s);
		r = _threadrendezvous((ulong)&c, 0);
		s = _procsplhi();
		lock(&chanlock);

		if(r==~0){		/* interrupted */
			if(c!=nil)		/* someone will meet us; go back */
				goto Again;
			c = (Channel*)~0;	/* so no one tries to meet us */
		}

		/* dequeue from channels, find selected one */
		a = nil;
		for(xa=alts; xa->op!=CHANEND; xa++){
			if(xa->op==CHANNOP)
				continue;
			if(xa->c == c)
				a = xa;
			dequeue(xa);
		}
		unlock(&chanlock);
		_procsplx(s);
		if(a == nil){	/* we were interrupted */
			assert(c==(Channel*)~0);
			return -1;
		}
	}else{
		altexec(a, s);	/* unlocks chanlock, does splx */
	}
	if(t)
		t->chan = Channone;
	return a - alts;
}

int
alt(Alt *alts)
{
	setuserpc(getcallerpc(&alts));
	return _alt(alts);
}

static int
runop(int op, Channel *c, void *v, int nb)
{
	int r;
	Alt a[2];

	/*
	 * we could do this without calling alt,
	 * but the only reason would be performance,
	 * and i'm not convinced it matters.
	 */
	a[0].op = op;
	a[0].c = c;
	a[0].v = v;
	a[1].op = CHANEND;
	if(nb)
		a[1].op = CHANNOBLK;
	switch(r=_alt(a)){
	case -1:	/* interrupted */
		return -1;
	case 1:	/* nonblocking, didn't accomplish anything */
		assert(nb);
		return 0;
	case 0:
		return 1;
	default:
		fprint(2, "ERROR: channel alt returned %d\n", r);
		abort();
		return -1;
	}
}

int
recv(Channel *c, void *v)
{
	setuserpc(getcallerpc(&c));
	return runop(CHANRCV, c, v, 0);
}

int
nbrecv(Channel *c, void *v)
{
	setuserpc(getcallerpc(&c));
	return runop(CHANRCV, c, v, 1);
}

int
send(Channel *c, void *v)
{
	setuserpc(getcallerpc(&c));
	return runop(CHANSND, c, v, 0);
}

int
nbsend(Channel *c, void *v)
{
	setuserpc(getcallerpc(&c));
	return runop(CHANSND, c, v, 1);
}

static void
channelsize(Channel *c, int sz)
{
	if(c->e != sz){
		fprint(2, "expected channel with elements of size %d, got size %d\n",
			sz, c->e);
		abort();
	}
}

int
sendul(Channel *c, ulong v)
{
	setuserpc(getcallerpc(&c));
	channelsize(c, sizeof(ulong));
	return send(c, &v);
}

ulong
recvul(Channel *c)
{
	ulong v;

	setuserpc(getcallerpc(&c));
	channelsize(c, sizeof(ulong));
	if(runop(CHANRCV, c, &v, 0) < 0)
		return ~0;
	return v;
}

int
sendp(Channel *c, void *v)
{
	setuserpc(getcallerpc(&c));
	channelsize(c, sizeof(void*));
	return runop(CHANSND, c, &v, 0);
}

void*
recvp(Channel *c)
{
	void *v;

	setuserpc(getcallerpc(&c));
	channelsize(c, sizeof(void*));
	if(runop(CHANRCV, c, &v, 0) < 0)
		return nil;
	return v;
}

int
nbsendul(Channel *c, ulong v)
{
	setuserpc(getcallerpc(&c));
	channelsize(c, sizeof(ulong));
	return runop(CHANSND, c, &v, 1);
}

ulong
nbrecvul(Channel *c)
{
	ulong v;

	setuserpc(getcallerpc(&c));
	channelsize(c, sizeof(ulong));
	if(runop(CHANRCV, c, &v, 1) == 0)
		return 0;
	return v;
}

int
nbsendp(Channel *c, void *v)
{
	setuserpc(getcallerpc(&c));
	channelsize(c, sizeof(void*));
	return runop(CHANSND, c, &v, 1);
}

void*
nbrecvp(Channel *c)
{
	void *v;

	setuserpc(getcallerpc(&c));
	channelsize(c, sizeof(void*));
	if(runop(CHANRCV, c, &v, 1) == 0)
		return nil;
	return v;
}

static int
emptyentry(Channel *c)
{
	int i, extra;

	assert((c->nentry==0 && c->qentry==nil) || (c->nentry && c->qentry));

	for(i=0; i<c->nentry; i++)
		if(c->qentry[i]==nil)
			return i;

	extra = 16;
	c->nentry += extra;
if(c->nentry > _threadhighnentry) _threadhighnentry = c->nentry;
	c->qentry = realloc((void*)c->qentry, c->nentry*sizeof(c->qentry[0]));
	if(c->qentry == nil)
		sysfatal("realloc channel entries: %r");
	_threadmemset(&c->qentry[i], 0, extra*sizeof(c->qentry[0]));
	return i;
}

static void
enqueue(Alt *a, Channel **c)
{
	int i;

	_threaddebug(DBGCHAN, "Queueing alt %p on channel %p", a, a->c);
	a->tag = c;
	i = emptyentry(a->c);
	a->c->qentry[i] = a;
}

static void
dequeue(Alt *a)
{
	int i;
	Channel *c;

	c = a->c;
	for(i=0; i<c->nentry; i++)
		if(c->qentry[i]==a){
			_threaddebug(DBGCHAN, "Dequeuing alt %p from channel %p", a, a->c);
			c->qentry[i] = nil;
			if(c->freed)
				_chanfree(c);
			return;
		}
}

static void*
altexecbuffered(Alt *a, int willreplace)
{
	uchar *v;
	Channel *c;

	c = a->c;
	/* use buffered channel queue */
	if(a->op==CHANRCV && c->n > 0){
		_threaddebug(DBGCHAN, "buffer recv alt %p chan %p", a, c);
		v = c->v + c->e*(c->f%c->s);
		if(!willreplace)
			c->n--;
		c->f++;
		return v;
	}
	if(a->op==CHANSND && c->n < c->s){
		_threaddebug(DBGCHAN, "buffer send alt %p chan %p", a, c);
		v = c->v + c->e*((c->f+c->n)%c->s);
		if(!willreplace)
			c->n++;
		return v;
	}
	abort();
	return nil;
}

static void
altcopy(void *dst, void *src, int sz)
{
	if(dst){
		if(src)
			memmove(dst, src, sz);
		else
			_threadmemset(dst, 0, sz);
	}
}

static int
altexec(Alt *a, int spl)
{
	volatile Alt *b;
	int i, n, otherop;
	Channel *c;
	void *me, *waiter, *buf;

	c = a->c;

	/* rendezvous with others */
	otherop = (CHANSND+CHANRCV) - a->op;
	n = 0;
	b = nil;
	me = a->v;
	for(i=0; i<c->nentry; i++)
		if(c->qentry[i] && c->qentry[i]->op==otherop && *c->qentry[i]->tag==nil)
			if(nrand(++n) == 0)
				b = c->qentry[i];
	if(b != nil){
		_threaddebug(DBGCHAN, "rendez %s alt %p chan %p alt %p", a->op==CHANRCV?"recv":"send", a, c, b);
		waiter = b->v;
		if(c->s && c->n){
			/*
			 * if buffer is full and there are waiters
			 * and we're meeting a waiter,
			 * we must be receiving.
			 *
			 * we use the value in the channel buffer,
			 * copy the waiter's value into the channel buffer
			 * on behalf of the waiter, and then wake the waiter.
			 */
			if(a->op!=CHANRCV)
				abort();
			buf = altexecbuffered(a, 1);
			altcopy(me, buf, c->e);
			altcopy(buf, waiter, c->e);
		}else{
			if(a->op==CHANRCV)
				altcopy(me, waiter, c->e);
			else
				altcopy(waiter, me, c->e);
		}
		*b->tag = c;	/* commits us to rendezvous */
		_threaddebug(DBGCHAN, "unlocking the chanlock");
		unlock(&chanlock);
		_procsplx(spl);
		_threaddebug(DBGCHAN, "chanlock is %lud", *(ulong*)(void*)&chanlock);
		while(_threadrendezvous((ulong)b->tag, 0) == ~0)
			;
		return 1;
	}

	buf = altexecbuffered(a, 0);
	if(a->op==CHANRCV)
		altcopy(me, buf, c->e);
	else
		altcopy(buf, me, c->e);

	unlock(&chanlock);
	_procsplx(spl);
	return 1;
}
