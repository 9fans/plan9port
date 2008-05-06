#define NOPLAN9DEFINES
#include <u.h>
#include <libc.h>
#include <stdlib.h> /* setenv etc. */
#include <time.h>

static int
dotz(time_t t, char *tzone)
{
	struct tm *gtm;
	struct tm tm;

	strftime(tzone, 32, "%Z", localtime(&t));
	tm = *localtime(&t);	/* set local time zone field */
	gtm = gmtime(&t);
	tm.tm_sec = gtm->tm_sec;
	tm.tm_min = gtm->tm_min;
	tm.tm_hour = gtm->tm_hour;
	tm.tm_mday = gtm->tm_mday;
	tm.tm_mon = gtm->tm_mon;
	tm.tm_year = gtm->tm_year;
	tm.tm_wday = gtm->tm_wday;
	return t - mktime(&tm);
}

static void
tm2Tm(struct tm *tm, Tm *bigtm, int tzoff, char *zone)
{
	memset(bigtm, 0, sizeof *bigtm);
	bigtm->sec = tm->tm_sec;
	bigtm->min = tm->tm_min;
	bigtm->hour = tm->tm_hour;
	bigtm->mday = tm->tm_mday;
	bigtm->mon = tm->tm_mon;
	bigtm->year = tm->tm_year;
	bigtm->wday = tm->tm_wday;
	bigtm->tzoff = tzoff;
	strncpy(bigtm->zone, zone, 3);
	bigtm->zone[3] = 0;
}

static void
Tm2tm(Tm *bigtm, struct tm *tm)
{
	/* initialize with current time to get local time zone! (tm_isdst) */
	time_t t;
	time(&t);
	*tm = *localtime(&t);

	tm->tm_sec = bigtm->sec;
	tm->tm_min = bigtm->min;
	tm->tm_hour = bigtm->hour;
	tm->tm_mday = bigtm->mday;
	tm->tm_mon = bigtm->mon;
	tm->tm_year = bigtm->year;
	tm->tm_wday = bigtm->wday;
}

Tm*
p9gmtime(long x)
{
	time_t t;
	struct tm tm;
	static Tm bigtm;
	
	t = (time_t)x;
	tm = *gmtime(&t);
	tm2Tm(&tm, &bigtm, 0, "GMT");
	return &bigtm;
}

Tm*
p9localtime(long x)
{
	time_t t;
	struct tm tm;
	static Tm bigtm;
	char tzone[32];

	t = (time_t)x;
	tm = *localtime(&t);
	tm2Tm(&tm, &bigtm, dotz(t, tzone), tzone);
	return &bigtm;
}

long
p9tm2sec(Tm *bigtm)
{
	time_t t;
	struct tm tm;
	char tzone[32];

	Tm2tm(bigtm, &tm);
	t = mktime(&tm);
	if(strcmp(bigtm->zone, "GMT") == 0 || strcmp(bigtm->zone, "UCT") == 0){
		t += dotz(t, tzone);
	}
	return t;
}

