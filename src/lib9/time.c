#include <u.h>
#include <sys/time.h>
#include <time.h>
#include <sys/resource.h>
#define NOPLAN9DEFINES
#include <libc.h>

long
p9times(long *t)
{
	struct rusage ru, cru;

	if(getrusage(0, &ru) < 0 || getrusage(-1, &cru) < 0)
		return -1;

	t[0] = ru.ru_utime.tv_sec*1000 + ru.ru_utime.tv_usec/1000;
	t[1] = ru.ru_stime.tv_sec*1000 + ru.ru_stime.tv_usec/1000;
	t[2] = cru.ru_utime.tv_sec*1000 + cru.ru_utime.tv_usec/1000;
	t[3] = cru.ru_stime.tv_sec*1000 + cru.ru_stime.tv_usec/1000;

	/* BUG */
	return t[0]+t[1]+t[2]+t[3];
}

double
p9cputime(void)
{
	long t[4];
	double d;

	if(p9times(t) < 0)
		return -1.0;

	d = (double)t[0]+(double)t[1]+(double)t[2]+(double)t[3];
	return d/1000.0;
}

vlong
p9nsec(void)
{
	struct timeval tv;

	if(gettimeofday(&tv, 0) < 0)
		return -1;

	return (vlong)tv.tv_sec*1000*1000*1000 + tv.tv_usec*1000;
}

long
p9time(long *tt)
{
	long t;
	t = time(0);
	if(tt)
		*tt = t;
	return t;
}

