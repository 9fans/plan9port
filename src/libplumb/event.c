#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include "plumb.h"

typedef struct EQueue EQueue;

struct EQueue
{
	int		id;
	char		*buf;
	int		nbuf;
	EQueue	*next;
};

static	EQueue	*equeue;
static	Lock		eqlock;

static
int
partial(int id, Event *e, uchar *b, int n)
{
	EQueue *eq, *p;
	int nmore;

	lock(&eqlock);
	for(eq = equeue; eq != nil; eq = eq->next)
		if(eq->id == id)
			break;
	unlock(&eqlock);
	if(eq == nil)
		return 0;
	/* partial message exists for this id */
	eq->buf = realloc(eq->buf, eq->nbuf+n);
	if(eq->buf == nil)
		drawerror(display, "eplumb: cannot allocate buffer");
	memmove(eq->buf+eq->nbuf, b, n);
	eq->nbuf += n;
	e->v = plumbunpackpartial((char*)eq->buf, eq->nbuf, &nmore);
	if(nmore == 0){	/* no more to read in this message */
		lock(&eqlock);
		if(eq == equeue)
			equeue = eq->next;
		else{
			for(p = equeue; p!=nil && p->next!=eq; p = p->next)
				;
			if(p == nil)
				drawerror(display, "eplumb: bad event queue");
			p->next = eq->next;
		}
		unlock(&eqlock);
		free(eq->buf);
		free(eq);
	}
	return 1;
}

static
void
addpartial(int id, char *b, int n)
{
	EQueue *eq;

	eq = malloc(sizeof(EQueue));
	if(eq == nil)
		return;
	eq->id = id;
	eq->nbuf = n;
	eq->buf = malloc(n);
	if(eq->buf == nil){
		free(eq);
		return;
	}
	memmove(eq->buf, b, n);
	lock(&eqlock);
	eq->next = equeue;
	equeue = eq;
	unlock(&eqlock);
}

static
int
plumbevent(int id, Event *e, uchar *b, int n)
{
	int nmore;

	if(partial(id, e, b, n) == 0){
		/* no partial message already waiting for this id */
		e->v = plumbunpackpartial((char*)b, n, &nmore);
		if(nmore > 0)	/* incomplete message */
			addpartial(id, (char*)b, n);
	}
	if(e->v == nil)
		return 0;
	return id;
}

int
eplumb(int key, char *port)
{
	int fd;

	fd = plumbopen(port, OREAD|OCEXEC);
	if(fd < 0)
		return -1;
	return estartfn(key, fd, 8192, plumbevent);
}
