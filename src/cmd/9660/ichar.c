#include <u.h>
#include <libc.h>
#include <bio.h>
#include <libsec.h>
#include <ctype.h>

#include "iso9660.h"

/*
 * ISO 9660 file names must be uppercase, digits, or underscore.
 * We use lowercase, digits, and underscore, translating lower to upper
 * in mkisostring, and upper to lower in isostring.
 * Files with uppercase letters in their names are thus nonconforming.
 * Conforming files also must have a basename
 * at most 8 letters and at most one suffix of at most 3 letters.
 */
char*
isostring(uchar *buf, int len)
{
	char *p, *q;

	p = emalloc(len+1);
	memmove(p, buf, len);
	p[len] = '\0';
	while(len > 0 && p[len-1] == ' ')
		p[--len] = '\0';
	for(q=p; *q; q++)
		*q = tolower((uchar)*q);
	q = atom(p);
	free(p);
	return q;
}

int
isisofrog(char c)
{
	if(c >= '0' && c <= '9')
		return 0;
	if(c >= 'a' && c <= 'z')
		return 0;
	if(c == '_')
		return 0;

	return 1;
}

int
isbadiso9660(char *s)
{
	char *p, *q;
	int i;

	if((p = strchr(s, '.')) != nil) {
		if(p-s > 8)
			return 1;
		for(q=s; q<p; q++)
			if(isisofrog(*q))
				return 1;
		if(strlen(p+1) > 3)
			return 1;
		for(q=p+1; *q; q++)
			if(isisofrog(*q))
				return 1;
	} else {
		if(strlen(s) > 8)
			return 1;
		for(q=s; *q; q++)
			if(isisofrog(*q))
				return 1;

		/*
		 * we rename files of the form [FD]dddddd
		 * so they don't interfere with us.
		 */
		if(strlen(s) == 7 && (s[0] == 'D' || s[0] == 'F')) {
			for(i=1; i<7; i++)
				if(s[i] < '0' || s[i] > '9')
					break;
			if(i == 7)
				return 1;
		}
	}
	return 0;
}

/*
 * ISO9660 name comparison
 *
 * The standard algorithm is as follows:
 *   Take the filenames without extensions, pad the shorter with 0x20s (spaces),
 *   and do strcmp.  If they are equal, go on.
 *   Take the extensions, pad the shorter with 0x20s (spaces),
 *   and do strcmp.  If they are equal, go on.
 *   Compare the version numbers.
 *
 * Since Plan 9 names are not allowed to contain characters 0x00-0x1F,
 * the padded comparisons are equivalent to using strcmp directly.
 * We still need to handle the base and extension differently,
 * so that .foo sorts before !foo.foo.
 */
int
isocmp(const void *va, const void *vb)
{
	int i;
	char s1[32], s2[32], *b1, *b2, *e1, *e2;
	const Direc *a, *b;

	a = va;
	b = vb;

	strecpy(s1, s1+sizeof s1, a->confname);
	b1 = s1;
	strecpy(s2, s2+sizeof s2, b->confname);
	b2 = s2;
	if((e1 = strchr(b1, '.')) != nil)
		*e1++ = '\0';
	else
		e1 = "";
	if((e2 = strchr(b2, '.')) != nil)
		*e2++ = '\0';
	else
		e2 = "";

	if((i = strcmp(b1, b2)) != 0)
		return i;

	return strcmp(e1, e2);
}

static char*
mkisostring(char *isobuf, int n, char *s)
{
	char *p, *q, *eq;

	eq = isobuf+n;
	for(p=s, q=isobuf; *p && q < eq; p++)
		if('a' <= *p && *p <= 'z')
			*q++ = *p+'A'-'a';
		else
			*q++ = *p;

	while(q < eq)
		*q++ = ' ';

	return isobuf;
}

void
Cputisopvd(Cdimg *cd, Cdinfo info)
{
	char buf[130];

	Cputc(cd, 1);				/* primary volume descriptor */
	Cputs(cd, "CD001", 5);			/* standard identifier */
	Cputc(cd, 1);				/* volume descriptor version */
	Cputc(cd, 0);				/* unused */

	assert(~info.flags & (CDplan9|CDrockridge));

	/* system identifier */
	strcpy(buf, "");
	if(info.flags & CDplan9)
		strcat(buf, "plan 9 ");
	if(info.flags & CDrockridge)
		strcat(buf, "rrip ");
	if(info.flags & CDbootable)
		strcat(buf, "boot ");
	if(info.flags & CDconform)
		strcat(buf, "iso9660");
	else
		strcat(buf, "utf8");

	struprcpy(buf, buf);
	Cputs(cd, buf, 32);

	Cputs(cd, mkisostring(buf, 32, info.volumename), 32);			/* volume identifier */

	Crepeat(cd, 0, 8);				/* unused */
	Cputn(cd, 0, 4);				/* volume space size */
	Crepeat(cd, 0, 32);				/* unused */
	Cputn(cd, 1, 2);				/* volume set size */
	Cputn(cd, 1, 2);				/* volume sequence number */
	Cputn(cd, Blocksize, 2);			/* logical block size */
	Cputn(cd, 0, 4);				/* path table size */
	Cputnl(cd, 0, 4);				/* location of Lpath */
	Cputnl(cd, 0, 4);				/* location of optional Lpath */
	Cputnm(cd, 0, 4);				/* location of Mpath */
	Cputnm(cd, 0, 4);				/* location of optional Mpath */
	Cputisodir(cd, nil, DTroot, 1, Cwoffset(cd));			/* root directory */

	Cputs(cd, mkisostring(buf, 128, info.volumeset), 128);		/* volume set identifier */
	Cputs(cd, mkisostring(buf, 128, info.publisher), 128);			/* publisher identifier */
	Cputs(cd, mkisostring(buf, 128, info.preparer), 128);			/* data preparer identifier */
	Cputs(cd, mkisostring(buf, 128, info.application), 128);		/* application identifier */

	Cputs(cd, "", 37);			/* copyright notice */
	Cputs(cd, "", 37);			/* abstract */
	Cputs(cd, "", 37);			/* bibliographic file */
	Cputdate1(cd, now);				/* volume creation date */
	Cputdate1(cd, now);				/* volume modification date */
	Cputdate1(cd, 0);				/* volume expiration date */
	Cputdate1(cd, 0);				/* volume effective date */
	Cputc(cd, 1);				/* file structure version */
	Cpadblock(cd);
}
