#include <u.h>
#include <stdlib.h> /* setenv etc. */
#define NOPLAN9DEFINES
#include <libc.h>
#include <time.h>

#define _HAVETIMEGM 1
#define _HAVETMZONE 1
#define _HAVETMTZOFF 1

#if defined(__linux__)
#	undef _HAVETMZONE
#	undef _HAVETMTZOFF

#elif defined(__sun__)
#	undef _HAVETIMEGM
#	undef _HAVETMZONE
#	undef _HAVETMTZOFF

#endif

static Tm bigtm;

static void
tm2Tm(struct tm *tm, Tm *bigtm)
{
	char *s;

	memset(bigtm, 0, sizeof *bigtm);
	bigtm->sec = tm->tm_sec;
	bigtm->min = tm->tm_min;
	bigtm->hour = tm->tm_hour;
	bigtm->mday = tm->tm_mday;
	bigtm->mon = tm->tm_mon;
	bigtm->year = tm->tm_year;
	bigtm->wday = tm->tm_wday;
	strftime(bigtm->zone, sizeof bigtm->zone, "%Z", tm);
#ifdef _HAVETZOFF
	bigtm->tzoff = tm->tm_gmtoff;
#endif
	
	if(bigtm->zone[0] == 0){
		s = getenv("TIMEZONE");
		if(s){
			strecpy(bigtm->zone, bigtm->zone+4, s);
			free(s);
		}
	}
}

static void
Tm2tm(Tm *bigtm, struct tm *tm)
{
	memset(tm, 0, sizeof *tm);
	tm->tm_sec = bigtm->sec;
	tm->tm_min = bigtm->min;
	tm->tm_hour = bigtm->hour;
	tm->tm_mday = bigtm->mday;
	tm->tm_mon = bigtm->mon;
	tm->tm_year = bigtm->year;
	tm->tm_wday = bigtm->wday;
#ifdef _HAVETMZONE
	tm->tm_zone = bigtm->zone;
#endif
#ifdef _HAVETZOFF
	tm->tm_gmtoff = bigtm->tzoff;
#endif
}

Tm*
p9gmtime(long x)
{
	time_t t;
	struct tm tm;

	t = (time_t)x;
	tm = *gmtime(&t);
	tm2Tm(&tm, &bigtm);
	return &bigtm;
}

Tm*
p9localtime(long x)
{
	time_t t;
	struct tm tm;

	t = (time_t)x;
	tm = *localtime(&t);
	tm2Tm(&tm, &bigtm);
	return &bigtm;
}

#if !defined(_HAVETIMEGM)
static time_t
timegm(struct tm *tm)
{
	time_t ret;
	char *tz;
	char *s;

	tz = getenv("TZ");
	putenv("TZ=");
	tzset();
	ret = mktime(tm);
	if(tz){
		s = smprint("TZ=%s", tz);
		if(s)
			putenv(s);
	}
	return ret;
}
#endif

long
p9tm2sec(Tm *bigtm)
{
	struct tm tm;

	Tm2tm(bigtm, &tm);
	if(strcmp(bigtm->zone, "GMT") == 0 || strcmp(bigtm->zone, "UCT") == 0)
		return timegm(&tm);
	return mktime(&tm);	/* local time zone */
}

