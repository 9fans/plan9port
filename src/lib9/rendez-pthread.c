#include <u.h>
#include <pthread.h>
#include <libc.h>

void
_procsleep(_Procrend *rend)
{
//print("sleep %p %d\n", rend, getpid());
	pthread_cond_init(&rend->cond, 0);
	rend->asleep = 1;
	while(rend->asleep)
		pthread_cond_wait(&rend->cond, &rend->l->mutex);
	pthread_cond_destroy(&rend->cond);
}

void
_procwakeup(_Procrend *rend)
{
//print("wakeup %p\n", rend);
	rend->asleep = 0;
	pthread_cond_signal(&rend->cond);
}

