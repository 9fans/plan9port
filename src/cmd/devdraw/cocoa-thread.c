#include <u.h>
#include <libc.h>
#include "cocoa-thread.h"

#ifndef TRY_LIBTHREAD

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
#endif
