#include <u.h>
#include <thread_db.h>
#include <sys/ptrace.h>
#include <errno.h>
#include <sys/procfs.h>	/* psaddr_t */
#include <libc.h>
#include <mach.h>

static char *tderrstr[] =
{
[TD_OK]			"no error",
[TD_ERR]		"some error",
[TD_NOTHR]		"no matching thread found",
[TD_NOSV]		"no matching synchronization handle found",
[TD_NOLWP]		"no matching light-weight process found",
[TD_BADPH]		"invalid process handle",
[TD_BADTH]		"invalid thread handle",
[TD_BADSH]		"invalid synchronization handle",
[TD_BADTA]		"invalid thread agent",
[TD_BADKEY]		"invalid key",
[TD_NOMSG]		"no event available",
[TD_NOFPREGS]	"no floating-point register content available",
[TD_NOLIBTHREAD]	"application not linked with thread library",
[TD_NOEVENT]	"requested event is not supported",
[TD_NOEVENT]	"requested event is not supported",
[TD_NOCAPAB]	"capability not available",
[TD_DBERR]		"internal debug library error",
[TD_NOAPLIC]	"operation is not applicable",
[TD_NOTSD]		"no thread-specific data available",
[TD_MALLOC]		"out of memory",
[TD_PARTIALREG]	"not entire register set was read or written",
[TD_NOXREGS]	"X register set not available for given threads",
[TD_TLSDEFER]	"thread has not yet allocated TLS for given module",
[TD_VERSION]	"version mismatch twixt libpthread and libthread_db",
[TD_NOTLS]		"there is no TLS segment in the given module",
};

static char*
terr(int e)
{
	static char buf[50];

	if(e < 0 || e >= nelem(tderrstr) || tderrstr[e] == nil){
		snprint(buf, sizeof buf, "thread err %d", e);
		return buf;
	}
	return tderrstr[e];
}

