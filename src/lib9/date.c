#include <u.h>
#include <libc.h>

#undef gmtime
#undef localtime
#undef asctime
#undef ctime
#undef cputime
#undef times
#undef tm2sec
#undef nsec

#include <time.h>

static Tm bigtm;

static void
tm2Tm(struct tm *tm, Tm *bigtm)
{
	memset(bigtm, 0, sizeof *bigtm);
	bigtm->sec = tm->tm_sec;
	bigtm->min = tm->tm_min;
	bigtm->hour = tm->tm_hour;
	bigtm->mday = tm->tm_mday;
	bigtm->mon = tm->tm_mon;
	bigtm->year = tm->tm_year;
	bigtm->wday = tm->tm_wday;
	strecpy(bigtm->zone, bigtm->zone+4, tm->tm_zone);
	bigtm->tzoff = tm->tm_gmtoff;
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
	tm->tm_zone = bigtm->zone;
	tm->tm_gmtoff = bigtm->tzoff;
}

Tm*
p9gmtime(long t)
{
	struct tm tm;

	tm = *gmtime(&t);
	tm2Tm(&tm, &bigtm);
	return &bigtm;
}

Tm*
p9localtime(long t)
{
	struct tm tm;

	tm = *localtime(&t);
	tm2Tm(&tm, &bigtm);
	return &bigtm;
}

long
p9tm2sec(Tm *bigtm)
{
	struct tm tm;

	Tm2tm(bigtm, &tm);
	if(strcmp(bigtm->zone, "GMT") == 0 || strcmp(bigtm->zone, "UCT") == 0)
		return timegm(&tm);
	return mktime(&tm);	/* local time zone */
}

