#define NOPLAN9DEFINES
#include <u.h>
#include <libc.h>
#include <stdlib.h> /* setenv etc. */
#include <time.h>

static int didtz;
static int tzdelta;
static char tzone[4];

static void
dotz(void)
{
	time_t t;
	struct tm *gtm;
	struct tm tm;

	if(didtz)
		return;
	didtz = 1;
	t = time(0);
	strftime(tzone, sizeof tzone, "%Z", localtime(&t));
	tm = *localtime(&t);	/* set local time zone field */
	gtm = gmtime(&t);
	tm.tm_sec = gtm->tm_sec;
	tm.tm_min = gtm->tm_min;
	tm.tm_hour = gtm->tm_hour;
	tm.tm_mday = gtm->tm_mday;
	tm.tm_mon = gtm->tm_mon;
	tm.tm_year = gtm->tm_year;
	tm.tm_wday = gtm->tm_wday;
	tzdelta = t - mktime(&tm);
}

static void
tm2Tm(struct tm *tm, Tm *bigtm, int gmt)
{
	memset(bigtm, 0, sizeof *bigtm);
	bigtm->sec = tm->tm_sec;
	bigtm->min = tm->tm_min;
	bigtm->hour = tm->tm_hour;
	bigtm->mday = tm->tm_mday;
	bigtm->mon = tm->tm_mon;
	bigtm->year = tm->tm_year;
	bigtm->wday = tm->tm_wday;
	if(gmt){
		strcpy(bigtm->zone, "GMT");
		bigtm->tzoff = 0;
	}else{
		dotz();
		strcpy(bigtm->zone, tzone);
		bigtm->tzoff = tzdelta;
	}
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
	tm2Tm(&tm, &bigtm, 1);
	return &bigtm;
}

Tm*
p9localtime(long x)
{
	time_t t;
	struct tm tm;
	static Tm bigtm;

	t = (time_t)x;
	tm = *localtime(&t);
	tm2Tm(&tm, &bigtm, 0);
	return &bigtm;
}

long
p9tm2sec(Tm *bigtm)
{
	time_t t;
	struct tm tm;

	Tm2tm(bigtm, &tm);
	t = mktime(&tm);
	if(strcmp(bigtm->zone, "GMT") == 0 || strcmp(bigtm->zone, "UCT") == 0){
		dotz();
		t += tzdelta;
	}
	return t;
}

