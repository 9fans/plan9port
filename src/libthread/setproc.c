/*
 * Avoid using threading calls for single-proc programs.
 */

#include "threadimpl.h"

static int multi;
static Proc *theproc;

void
_threadsetproc(Proc *p)
{
	if(!multi)
		theproc = p;
	else
		_kthreadsetproc(p);
}

Proc*
_threadgetproc(void)
{
	if(!multi)
		return theproc;
	return _kthreadgetproc();
}

void
_threadmultiproc(void)
{
	if(multi)
		return;

	multi = 1;
	_kthreadinit();
	_threadsetproc(theproc);
}
