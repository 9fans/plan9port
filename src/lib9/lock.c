#include <unistd.h>
#include <sched.h>
#include <lib9.h>

int _ntas;
static int
_xtas(void *v)
{
	int x;

	_ntas++;
	x = _tas(v);
	return x;
}

int
canlock(Lock *l)
{
	return !_xtas(&l->val);
}

void
unlock(Lock *l)
{
	l->val = 0;
}

void
lock(Lock *lk)
{
	int i;

	/* once fast */
	if(!_xtas(&lk->val))
		return;
	/* a thousand times pretty fast */
	for(i=0; i<1000; i++){
		if(!_xtas(&lk->val))
			return;
		sched_yield();
	}
	/* now nice and slow */
	for(i=0; i<1000; i++){
		if(!_xtas(&lk->val))
			return;
		usleep(100*1000);
	}
	/* take your time */
	while(_xtas(&lk->val))
		usleep(1000*1000);
}
