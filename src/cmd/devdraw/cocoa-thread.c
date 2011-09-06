#include <u.h>
#include <libc.h>
#include "cocoa-thread.h"

static pthread_mutex_t initlock = PTHREAD_MUTEX_INITIALIZER;

void
qlock(QLock *q)
{
	if(q->init == 0){
		pthread_mutex_lock(&initlock);
		if(q->init == 0){
			pthread_mutex_init(&q->m, nil);
			q->init = 1;
		}
		pthread_mutex_unlock(&initlock);
	}
	pthread_mutex_lock(&q->m);
}

void
qunlock(QLock *q)
{
	pthread_mutex_unlock(&q->m);
}

static void
rinit(Rendez *r)
{
	pthread_mutex_lock(&initlock);
	if(r->init == 0){
		pthread_cond_init(&r->c, nil);
		r->init = 1;
	}
	pthread_mutex_unlock(&initlock);
}

void
rsleep(Rendez *r)
{
	if(r->init == 0)
		rinit(r);
	pthread_cond_wait(&r->c, &r->l->m);
}

int
rwakeup(Rendez *r)
{
	if(r->init == 0)
		rinit(r);
	pthread_cond_signal(&r->c);

	return 0;
}
