#include <u.h>
#define NOPLAN9DEFINES
#include <sys/param.h>
#include <sys/time.h>
#include <sched.h>
#include <libc.h>

#if defined(__NetBSD__) || (defined(__OpenBSD__) && OpenBSD <= 200611)
#if !defined(sched_yield)
#	define sched_yield() \
		do{ struct timespec ts; \
			ts.tv_sec = 0; \
			ts.tv_nsec = 0; \
			nanosleep(&ts, 0); \
		}while(0)
#endif
#endif

int
p9sleep(long milli)
{
	struct timeval tv;

	if(milli == 0){
		sched_yield();
		return 0;
	}

	tv.tv_sec = milli/1000;
	tv.tv_usec = (milli%1000)*1000;
	return select(0, 0, 0, 0, &tv);
}

long
p9alarm(ulong milli)
{
	struct itimerval itv;
	struct itimerval oitv;

	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = 0;
	itv.it_value.tv_sec = milli/1000;
	itv.it_value.tv_usec = (milli%1000)*1000;
	if(setitimer(ITIMER_REAL, &itv, &oitv) < 0)
		return -1;
	return oitv.it_value.tv_sec*1000+oitv.it_value.tv_usec/1000;
}
