/*
 * Proc structure hash table indexed by proctabid() (usually getpid()).
 * No lock is necessary for lookups (important when called from signal
 * handlers).
 * 
 * To be included from other files (e.g., Linux-clone.c).
 */

#define T ((void*)-1)

enum
{
	PTABHASH = 1031,
};

static Lock ptablock;
static Proc *proctab[PTABHASH];
static Proc *theproc;
static int multi;

void
_threadmultiproc(void)
{
	if(multi == 0){
		multi = 1;
		_threadsetproc(theproc);
	}
}

void
_threadsetproc(Proc *p)
{
	int i, h;
	Proc **t;

	if(!multi){
		theproc = p;
		return;
	}
	lock(&ptablock);
	p->procid = procid();
	h = p->procid%PTABHASH;
	for(i=0; i<PTABHASH; i++){
		t = &proctab[(h+i)%PTABHASH];
		if(*t==nil || *t==T){
			*t = p;
			break;
		}
	}
	unlock(&ptablock);
	if(i == PTABHASH)
		sysfatal("too many procs - proctab is full");
}

static Proc**
_threadfindproc(int id)
{
	int i, h;
	Proc **t;

	if(!multi)
		return &theproc;

	h = id%PTABHASH;
	for(i=0; i<PTABHASH; i++){
		t = &proctab[(h+i)%PTABHASH];
		if(*t != nil && *t != T && (*t)->procid == id){
			unlock(&ptablock);
			return t;
		}
	}
	return nil;
}

Proc*
_threadgetproc(void)
{
	Proc **t;

	t = _threadfindproc(procid());
	if(t == nil)
		return nil;
	return *t;
}

Proc*
_threaddelproc(void)
{
	Proc **t, *p;

	t = _threadfindproc(procid());
	if(t == nil)
		return nil;
	p = *t;
	*t = T;
	return p;
}
