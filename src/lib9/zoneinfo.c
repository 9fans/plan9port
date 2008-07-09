#include <u.h>
#include <libc.h>

/*
 * Access local time entries of zoneinfo files.
 * Formats 0 and 2 are supported, and 4-byte timestamps
 * 
 * Copyright © 2008 M. Teichgräber
 * Contributed under the terms of the Lucent Public License 1.02.
 */
#include "zoneinfo.h"

static
struct Zoneinfo
{
	int	timecnt;		/* # of transition times */
	int	typecnt;		/* # of local time types */
	int	charcnt;		/* # of characters of time zone abbreviation strings */

	uchar *ptime;
	uchar *ptype;
	uchar *ptt;
	uchar *pzone;
} z;

static uchar *tzdata;

static
uchar*
readtzfile(char *file)
{
	uchar *p;
	int fd;
	Dir *d;

	fd = open(file, OREAD);
	if (fd<0)
		return nil;
        d = dirfstat(fd);
	if (d==nil)
		return nil;
	p = malloc(d->length);
	if (p!=nil)
		readn(fd, p, d->length);
	free(d);
	close(fd);
	return p;
}
static char *zonefile;
void
tzfile(char *f)
{
	if (tzdata!=nil) {
		free(tzdata);
		tzdata = nil;
	}
	z.timecnt = 0;
	zonefile = f;
}

static
long
get4(uchar *p)
{
	return (p[0]<<24)+(p[1]<<16)+(p[2]<<8)+p[3];
}

enum {
	TTinfosz	= 4+1+1,
};

static
int
parsehead(void)
{
	uchar *p;
	int	ver;

	ver = tzdata[4];
	if (ver!=0)
	if (ver!='2')
		return -1;

	p = tzdata + 4 + 1 + 15;

	z.timecnt = get4(p+3*4);
	z.typecnt = get4(p+4*4);
	if (z.typecnt==0)
		return -1;
	z.charcnt = get4(p+5*4);
	z.ptime = p+6*4;
	z.ptype = z.ptime + z.timecnt*4;
	z.ptt = z.ptype + z.timecnt;
	z.pzone = z.ptt + z.typecnt*TTinfosz;
	return 0;
}

static
void
ttinfo(Tinfo *ti, int tti)
{
	uchar *p;
	int	i;

	i = z.ptype[tti];
	assert(i<z.typecnt);
	p = z.ptt + i*TTinfosz;
	ti->tzoff = get4(p);
	ti->dlflag = p[4];
	assert(p[5]<z.charcnt);
	ti->zone = (char*)z.pzone + p[5];
}

static
void
readtimezone(void)
{
	char *tmp;

	z.timecnt = 0;
	switch (zonefile==nil) {
	default:
		if ((tmp=getenv("timezone"))!=nil) {
			tzdata = readtzfile(tmp);
			free(tmp);
			break;
		}
		zonefile = "/etc/localtime";
		/* fall through */
	case 0:
		tzdata = readtzfile(zonefile);
	}
	if (tzdata==nil)
		return;

	if (strncmp("TZif", (char*)tzdata, 4)!=0)
		goto errfree;

	if (parsehead()==-1) {
	errfree:
		free(tzdata);
		tzdata = nil;
		z.timecnt = 0;
		return;
	}
}

static
tlong
gett4(uchar *p)
{
	long l;

	l = get4(p);
	if (l<0)
		return 0;
	return l;
}
int
zonetinfo(Tinfo *ti, int i)
{
	if (tzdata==nil)
		readtimezone();
	if (i<0 || i>=z.timecnt)
		return -1;
	ti->t = gett4(z.ptime + 4*i);
	ttinfo(ti, i);
	return i;
}

int
zonelookuptinfo(Tinfo *ti, tlong t)
{
	uchar *p;
	int	i;
	tlong	oldtt, tt;

	if (tzdata==nil)
		readtimezone();
	oldtt = 0;
	p = z.ptime;
	for (i=0; i<z.timecnt; i++) {
		tt = gett4(p);
		if (t<tt)
			break;
		oldtt = tt;
		p += 4;
	}
	if (i>0) {
		ttinfo(ti, i-1);
		ti->t = oldtt;
//		fprint(2, "t:%ld off:%d dflag:%d %s\n", (long)ti->t, ti->tzoff, ti->dlflag, ti->zone);
		return i-1;
	}
	return -1;
}

void
zonedump(int fd)
{
	int	i;
	uchar *p;
	tlong t;
	Tinfo ti;

	if (tzdata==nil)
		readtimezone();
	p = z.ptime;
	for (i=0; i<z.timecnt; i++) {
		t = gett4(p);
		ttinfo(&ti, i);
		fprint(fd, "%ld\t%d\t%d\t%s\n", (long)t, ti.tzoff, ti.dlflag, ti.zone);
		p += 4;
	}
}
