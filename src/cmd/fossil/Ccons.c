#include "stdinc.h"

#include "9.h"

enum {
	Nl	= 256,			/* max. command line length */
	Nq	= 8*1024,		/* amount of I/O buffered */
};

typedef struct Q {
	VtLock*	lock;
	VtRendez* full;
	VtRendez* empty;

	char	q[Nq];
	int	n;
	int	r;
	int	w;
} Q;

typedef struct Cons {
	VtLock*	lock;
	int	ref;
	int	closed;
	int	fd;
	int	srvfd;
	int	ctlfd;
	Q*	iq;		/* points to console.iq */
	Q*	oq;		/* points to console.oq */
} Cons;

char *currfsysname;

static struct {
	Q*	iq;		/* input */
	Q*	oq;		/* output */
	char	l[Nl];		/* command line assembly */
	int	nl;		/* current line length */
	int	nopens;

	char*	prompt;
	int	np;
} console;

static void
consClose(Cons* cons)
{
	vtLock(cons->lock);
	cons->closed = 1;

	cons->ref--;
	if(cons->ref > 0){
		vtLock(cons->iq->lock);
		vtWakeup(cons->iq->full);
		vtUnlock(cons->iq->lock);
		vtLock(cons->oq->lock);
		vtWakeup(cons->oq->empty);
		vtUnlock(cons->oq->lock);
		vtUnlock(cons->lock);
		return;
	}

	if(cons->ctlfd != -1){
		close(cons->ctlfd);
		cons->srvfd = -1;
	}
	if(cons->srvfd != -1){
		close(cons->srvfd);
		cons->srvfd = -1;
	}
	if(cons->fd != -1){
		close(cons->fd);
		cons->fd = -1;
	}
	vtUnlock(cons->lock);
	vtLockFree(cons->lock);
	vtMemFree(cons);
	console.nopens--;
}

static void
consIProc(void* v)
{
	Q *q;
	Cons *cons;
	int n, w;
	char buf[Nq/4];

	vtThreadSetName("consI");

	cons = v;
	q = cons->iq;
	for(;;){
		/*
		 * Can't tell the difference between zero-length read
		 * and eof, so keep calling read until we get an error.
		 */
		if(cons->closed || (n = read(cons->fd, buf, Nq/4)) < 0)
			break;
		vtLock(q->lock);
		while(Nq - q->n < n && !cons->closed)
			vtSleep(q->full);
		w = Nq - q->w;
		if(w < n){
			memmove(&q->q[q->w], buf, w);
			memmove(&q->q[0], buf + w, n - w);
		}
		else
			memmove(&q->q[q->w], buf, n);
		q->w = (q->w + n) % Nq;
		q->n += n;
		vtWakeup(q->empty);
		vtUnlock(q->lock);
	}
	consClose(cons);
}

static void
consOProc(void* v)
{
	Q *q;
	Cons *cons;
	char buf[Nq];
	int lastn, n, r;

	vtThreadSetName("consO");

	cons = v;
	q = cons->oq;
	vtLock(q->lock);
	lastn = 0;
	for(;;){
		while(lastn == q->n && !cons->closed)
			vtSleep(q->empty);
		if((n = q->n - lastn) > Nq)
			n = Nq;
		if(n > q->w){
			r = n - q->w;
			memmove(buf, &q->q[Nq - r], r);
			memmove(buf+r, &q->q[0], n - r);
		}
		else
			memmove(buf, &q->q[q->w - n], n);
		lastn = q->n;
		vtUnlock(q->lock);
		if(cons->closed || write(cons->fd, buf, n) < 0)
			break;
		vtLock(q->lock);
		vtWakeup(q->empty);
	}
	consClose(cons);
}

int
consOpen(int fd, int srvfd, int ctlfd)
{
	Cons *cons;

	cons = vtMemAllocZ(sizeof(Cons));
	cons->lock = vtLockAlloc();
	cons->fd = fd;
	cons->srvfd = srvfd;
	cons->ctlfd = ctlfd;
	cons->iq = console.iq;
	cons->oq = console.oq;
	console.nopens++;

	vtLock(cons->lock);
	cons->ref = 2;
	cons->closed = 0;
	if(vtThread(consOProc, cons) < 0){
		cons->ref--;
		vtUnlock(cons->lock);
		consClose(cons);
		return 0;
	}
	vtUnlock(cons->lock);

	if(ctlfd >= 0)
		consIProc(cons);
	else if(vtThread(consIProc, cons) < 0){
		consClose(cons);
		return 0;
	}

	return 1;
}

static int
qWrite(Q* q, char* p, int n)
{
	int w;

	vtLock(q->lock);
	if(n > Nq - q->w){
		w = Nq - q->w;
		memmove(&q->q[q->w], p, w);
		memmove(&q->q[0], p + w, n - w);
		q->w = n - w;
	}
	else{
		memmove(&q->q[q->w], p, n);
		q->w += n;
	}
	q->n += n;
	vtWakeup(q->empty);
	vtUnlock(q->lock);

	return n;
}

static Q*
qAlloc(void)
{
	Q *q;

	q = vtMemAllocZ(sizeof(Q));
	q->lock = vtLockAlloc();
	q->full = vtRendezAlloc(q->lock);
	q->empty = vtRendezAlloc(q->lock);
	q->n = q->r = q->w = 0;

	return q;
}

static void
consProc(void*)
{
	Q *q;
	int argc, i, n, r;
	char *argv[20], buf[Nq], *lp, *wbuf;
	char procname[64];

	snprint(procname, sizeof procname, "cons %s", currfsysname);
	vtThreadSetName(procname);

	q = console.iq;
	qWrite(console.oq, console.prompt, console.np);
	vtLock(q->lock);
	for(;;){
		while((n = q->n) == 0)
			vtSleep(q->empty);
		r = Nq - q->r;
		if(r < n){
			memmove(buf, &q->q[q->r], r);
			memmove(buf + r, &q->q[0], n - r);
		}
		else
			memmove(buf, &q->q[q->r], n);
		q->r = (q->r + n) % Nq;
		q->n -= n;
		vtWakeup(q->full);
		vtUnlock(q->lock);

		for(i = 0; i < n; i++){
			switch(buf[i]){
			case '\004':				/* ^D */
				if(console.nl == 0){
					qWrite(console.oq, "\n", 1);
					break;
				}
				/*FALLTHROUGH*/
			default:
				if(console.nl < Nl-1){
					qWrite(console.oq, &buf[i], 1);
					console.l[console.nl++] = buf[i];
				}
				continue;
			case '\b':
				if(console.nl != 0){
					qWrite(console.oq, &buf[i], 1);
					console.nl--;
				}
				continue;
			case '\n':
				qWrite(console.oq, &buf[i], 1);
				break;
			case '\025':				/* ^U */
				qWrite(console.oq, "^U\n", 3);
				console.nl = 0;
				break;
			case '\027':				/* ^W */
				console.l[console.nl] = '\0';
				wbuf = vtMemAlloc(console.nl+1);
				memmove(wbuf, console.l, console.nl+1);
				argc = tokenize(wbuf, argv, nelem(argv));
				if(argc > 0)
					argc--;
				console.nl = 0;
				lp = console.l;
				for(i = 0; i < argc; i++)
					lp += sprint(lp, "%q ", argv[i]);
				console.nl = lp - console.l;
				vtMemFree(wbuf);
				qWrite(console.oq, "^W\n", 3);
				if(console.nl == 0)
					break;
				qWrite(console.oq, console.l, console.nl);
				continue;
			case '\177':
				qWrite(console.oq, "\n", 1);
				console.nl = 0;
				break;
			}

			console.l[console.nl] = '\0';
			if(console.nl != 0)
				cliExec(console.l);

			console.nl = 0;
			qWrite(console.oq, console.prompt, console.np);
		}

		vtLock(q->lock);
	}
}

int
consWrite(char* buf, int len)
{
	if(console.oq == nil)
		return write(2, buf, len);
	if(console.nopens == 0)
		write(2, buf, len);
	return qWrite(console.oq, buf, len);
}

int
consPrompt(char* prompt)
{
	char buf[ERRMAX];

	if(prompt == nil)
		prompt = "prompt";

	vtMemFree(console.prompt);
	console.np = snprint(buf, sizeof(buf), "%s: ", prompt);
	console.prompt = vtStrDup(buf);

	return console.np;
}

int
consTTY(void)
{
	int ctl, fd;
	char *name, *p;

	name = "/dev/cons";
	if((fd = open(name, ORDWR)) < 0){
		name = "#c/cons";
		if((fd = open(name, ORDWR)) < 0){
			vtSetError("consTTY: open %s: %r", name);
			return 0;
		}
	}

	p = smprint("%sctl", name);
	if((ctl = open(p, OWRITE)) < 0){
		close(fd);
		vtSetError("consTTY: open %s: %r", p);
		free(p);
		return 0;
	}
	if(write(ctl, "rawon", 5) < 0){
		close(ctl);
		close(fd);
		vtSetError("consTTY: write %s: %r", p);
		free(p);
		return 0;
	}
	free(p);

	if(consOpen(fd, fd, ctl) == 0){
		close(ctl);
		close(fd);
		return 0;
	}

	return 1;
}

int
consInit(void)
{
	console.iq = qAlloc();
	console.oq = qAlloc();
	console.nl = 0;

	consPrompt(nil);

	if(vtThread(consProc, nil) < 0){
		vtFatal("can't start console proc");
		return 0;
	}

	return 1;
}
