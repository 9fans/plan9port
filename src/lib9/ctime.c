/*
 * This routine converts time as follows.
 * The epoch is 0000 Jan 1 1970 GMT.
 * The argument time is in seconds since then.
 * The localtime(t) entry returns a pointer to an array
 * containing
 *
 *	seconds (0-59)
 *	minutes (0-59)
 *	hours (0-23)
 *	day of month (1-31)
 *	month (0-11)
 *	year-1970
 *	weekday (0-6, Sun is 0)
 *	day of the year
 *	daylight savings flag
 *
 * The routine gets the daylight savings time from the environment.
 *
 * asctime(tvec))
 * where tvec is produced by localtime
 * returns a ptr to a character string
 * that has the ascii time in the form
 *
 *	                            \\
 *	Thu Jan 01 00:00:00 GMT 1970n0
 *	012345678901234567890123456789
 *	0	  1	    2
 *
 * ctime(t) just calls localtime, then asctime.
 */

#include <u.h>
#include <libc.h>

#include "zoneinfo.h"

static	char	dmsize[12] =
{
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

#define dysize ctimedysize
static	int	dysize(int);
static	void	ct_numb(char*, int);

char*
ctime(long t)
{
	return asctime(localtime(t));
}

Tm*
localtime(long tim)
{
	Tinfo ti;
	Tm *ct;

	if (zonelookuptinfo(&ti, tim)!=-1) {
		ct = gmtime(tim+ti.tzoff);
		strncpy(ct->zone, ti.zone, sizeof ct->zone - 1);
		ct->zone[sizeof ct->zone-1] = 0;
		ct->tzoff = ti.tzoff;
		return ct;
	}
	return gmtime(tim);
}

Tm*
gmtime(long tim)
{
	int d0, d1;
	long hms, day;
	static Tm xtime;

	/*
	 * break initial number into days
	 */
	hms = tim % 86400L;
	day = tim / 86400L;
	if(hms < 0) {
		hms += 86400L;
		day -= 1;
	}

	/*
	 * generate hours:minutes:seconds
	 */
	xtime.sec = hms % 60;
	d1 = hms / 60;
	xtime.min = d1 % 60;
	d1 /= 60;
	xtime.hour = d1;

	/*
	 * day is the day number.
	 * generate day of the week.
	 * The addend is 4 mod 7 (1/1/1970 was Thursday)
	 */

	xtime.wday = (day + 7340036L) % 7;

	/*
	 * year number
	 */
	if(day >= 0)
		for(d1 = 1970; day >= dysize(d1); d1++)
			day -= dysize(d1);
	else
		for (d1 = 1970; day < 0; d1--)
			day += dysize(d1-1);
	xtime.year = d1-1900;
	xtime.yday = d0 = day;

	/*
	 * generate month
	 */

	if(dysize(d1) == 366)
		dmsize[1] = 29;
	for(d1 = 0; d0 >= dmsize[d1]; d1++)
		d0 -= dmsize[d1];
	dmsize[1] = 28;
	xtime.mday = d0 + 1;
	xtime.mon = d1;
	strcpy(xtime.zone, "GMT");
	return &xtime;
}

char*
asctime(Tm *t)
{
	const char *ncp;
	static char cbuf[30];

	strcpy(cbuf, "Thu Jan 01 00:00:00 GMT 1970\n");
	ncp = &"SunMonTueWedThuFriSat"[t->wday*3];
	cbuf[0] = *ncp++;
	cbuf[1] = *ncp++;
	cbuf[2] = *ncp;
	ncp = &"JanFebMarAprMayJunJulAugSepOctNovDec"[t->mon*3];
	cbuf[4] = *ncp++;
	cbuf[5] = *ncp++;
	cbuf[6] = *ncp;
	ct_numb(cbuf+8, t->mday);
	ct_numb(cbuf+11, t->hour+100);
	ct_numb(cbuf+14, t->min+100);
	ct_numb(cbuf+17, t->sec+100);
	ncp = t->zone;
	cbuf[20] = *ncp++;
	cbuf[21] = *ncp++;
	cbuf[22] = *ncp;
	if(t->year >= 100) {
		cbuf[24] = '2';
		cbuf[25] = '0';
	}
	ct_numb(cbuf+26, t->year+100);
	return cbuf;
}

static
int
dysize(int y)
{

	if(y%4 == 0 && (y%100 != 0 || y%400 == 0))
		return 366;
	return 365;
}

static
void
ct_numb(char *cp, int n)
{

	cp[0] = ' ';
	if(n >= 10)
		cp[0] = (n/10)%10 + '0';
	cp[1] = n%10 + '0';
}
