#include <u.h>
#include <libc.h>

/*
 * The function pointers are supplied by the thread
 * library during its initialization.  If there is no thread
 * library, there is no multithreading.
 */

int	(*_lock)(Lock*, int, ulong);
void	(*_unlock)(Lock*, ulong);
int	(*_qlock)(QLock*, int, ulong);	/* do not use */
void	(*_qunlock)(QLock*, ulong);
void	(*_rsleep)(Rendez*, ulong);	/* do not use */
int	(*_rwakeup)(Rendez*, int, ulong);
int	(*_rlock)(RWLock*, int, ulong);	/* do not use */
int	(*_wlock)(RWLock*, int, ulong);
void	(*_runlock)(RWLock*, ulong);
void	(*_wunlock)(RWLock*, ulong);

void
lock(Lock *l)
{
	if(_lock)
		(*_lock)(l, 1, getcallerpc(&l));
	else
		l->held = 1;
}

int
canlock(Lock *l)
{
	if(_lock)
		return (*_lock)(l, 0, getcallerpc(&l));
	else{
		if(l->held)
			return 0;
		l->held = 1;
		return 1;
	}
}

void
unlock(Lock *l)
{
	if(_unlock)
		(*_unlock)(l, getcallerpc(&l));
	else
		l->held = 0;
}

void
qlock(QLock *l)
{
	if(_qlock)
		(*_qlock)(l, 1, getcallerpc(&l));
	else
		l->l.held = 1;
}

int
canqlock(QLock *l)
{
	if(_qlock)
		return (*_qlock)(l, 0, getcallerpc(&l));
	else{
		if(l->l.held)
			return 0;
		l->l.held = 1;
		return 1;
	}
}

void
qunlock(QLock *l)
{
	if(_qunlock)
		(*_qunlock)(l, getcallerpc(&l));
	else
		l->l.held = 0;
}

void
rlock(RWLock *l)
{
	if(_rlock)
		(*_rlock)(l, 1, getcallerpc(&l));
	else
		l->readers++;
}

int
canrlock(RWLock *l)
{
	if(_rlock)
		return (*_rlock)(l, 0, getcallerpc(&l));
	else{
		if(l->writer)
			return 0;
		l->readers++;
		return 1;
	}
}

void
runlock(RWLock *l)
{
	if(_runlock)
		(*_runlock)(l, getcallerpc(&l));
	else
		l->readers--;
}

void
wlock(RWLock *l)
{
	if(_wlock)
		(*_wlock)(l, 1, getcallerpc(&l));
	else
		l->writer = (void*)1;
}

int
canwlock(RWLock *l)
{
	if(_wlock)
		return (*_wlock)(l, 0, getcallerpc(&l));
	else{
		if(l->writer || l->readers)
			return 0;
		l->writer = (void*)1;
		return 1;
	}
}

void
wunlock(RWLock *l)
{
	if(_wunlock)
		(*_wunlock)(l, getcallerpc(&l));
	else
		l->writer = nil;
}

void
rsleep(Rendez *r)
{
	if(_rsleep)
		(*_rsleep)(r, getcallerpc(&r));
}

int
rwakeup(Rendez *r)
{
	if(_rwakeup)
		return (*_rwakeup)(r, 0, getcallerpc(&r));
	return 0;
}

int
rwakeupall(Rendez *r)
{
	if(_rwakeup)
		return (*_rwakeup)(r, 1, getcallerpc(&r));
	return 0;
}
