/*
 * On Linux 2.6 and later, we can use pthreads (in fact, we must),
 * but on earlier Linux, pthreads are incompatible with using our
 * own coroutines in libthread.  In order to make binaries that work
 * on either system, we detect the pthread library in use and call
 * the appropriate functions.
 */

#include <u.h>
#include <signal.h>
#include <pthread.h>
#include <libc.h>

#define _procsleep _procsleep_signal
#define _procwakeup _procwakeup_signal
#include "rendez-signal.c"

#undef _procsleep
#undef _procwakeup
#define _procsleep _procsleep_pthread
#define _procwakeup _procwakeup_pthread
#include "rendez-pthread.c"

#undef _procsleep
#undef _procwakeup

int
_islinuxnptl(void)
{
	static char buf[100];
	static int isnptl = -1;

	if(isnptl == -1){
		if(confstr(_CS_GNU_LIBPTHREAD_VERSION, buf, sizeof buf) > 0
		&& strncmp(buf, "NPTL", 4) == 0)
			isnptl = 1;
		else
			isnptl = 0;
	}
	return isnptl;
}

void
_procsleep(_Procrend *r)
{
	if(_islinuxnptl())
		return _procsleep_pthread(r);
	else
		return _procsleep_signal(r);
}

void
_procwakeup(_Procrend *r)
{
	if(_islinuxnptl())
		return _procwakeup_pthread(r);
	else
		return _procwakeup_signal(r);
}

