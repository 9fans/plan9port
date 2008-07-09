#include <u.h>
#include <libc.h>

#include "zoneinfo.h"

#define SEC2MIN 60L
#define SEC2HOUR (60L*SEC2MIN)
#define SEC2DAY (24L*SEC2HOUR)

/*
 *  days per month plus days/year
 */
static	int	dmsize[] =
{
	365, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};
static	int	ldmsize[] =
{
	366, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

/*
 *  return the days/month for the given year
 */
static int *
yrsize(int y)
{
	if((y%4) == 0 && ((y%100) != 0 || (y%400) == 0))
		return ldmsize;
	else
		return dmsize;
}

/*
 * compute seconds since Jan 1 1970 GMT
 * and convert to our timezone.
 */
long
tm2sec(Tm *tm)
{
	Tinfo ti0, ti1, *ti;
	long secs;
	int i, yday, year, *d2m;

	secs = 0;

	/*
	 *  seconds per year
	 */
	year = tm->year + 1900;
	for(i = 1970; i < year; i++){
		d2m = yrsize(i);
		secs += d2m[0] * SEC2DAY;
	}

	/*
	 *  if mday is set, use mon and mday to compute yday
	 */
	if(tm->mday){
		yday = 0;
		d2m = yrsize(year);
		for(i=0; i<tm->mon; i++)
			yday += d2m[i+1];
		yday += tm->mday-1;
	}else{
		yday = tm->yday;
	}
	secs += yday * SEC2DAY;

	/*
	 * hours, minutes, seconds
	 */
	secs += tm->hour * SEC2HOUR;
	secs += tm->min * SEC2MIN;
	secs += tm->sec;

	/*
	 * Assume the local time zone if zone is not GMT
	 */
	if(strcmp(tm->zone, "GMT") != 0) {
		i = zonelookuptinfo(&ti0, secs);
		ti = &ti0;
		if (i != -1)
		if (ti->tzoff!=0) {
			/*
			 * to what local time period `secs' belongs?
			 */
			if (ti->tzoff>0) {
				/*
				 * east of GMT; check previous local time transition
				 */
				if (ti->t+ti->tzoff > secs)
				if (zonetinfo(&ti1, i-1)!=-1)
					ti = &ti1;
			} else
				/*
				 * west of GMT; check next local time transition
				 */
				if (zonetinfo(&ti1, i+1))
				if (ti1.t+ti->tzoff < secs)
					ti = &ti1;
//			fprint(2, "tt: %ld+%d %ld\n", (long)ti->t, ti->tzoff, (long)secs);
			secs -= ti->tzoff;
		}
	}
	
	if(secs < 0)
		secs = 0;
	return secs;
}
