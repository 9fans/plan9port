#include <u.h>
#include <libc.h>
#include <bio.h>
#include <libsec.h>

#include "iso9660.h"

char*
jolietstring(uchar *buf, int len)
{
	char *p, *q;
	int i;
	Rune *rp;

	rp = emalloc(sizeof(Rune)*(len/2+1));
	p = emalloc(UTFmax*(len/2+1));

	for(i=0; i<len/2; i++)
		rp[i] = (buf[2*i]<<8) | buf[2*i+1];
	rp[i] = (Rune)'\0';

	snprint(p, UTFmax*(len/2+1), "%S", rp);
	q = atom(p);
	free(p);
	return q;
}

/*
 * Joliet name validity check
 *
 * Joliet names have length at most 128 bytes (64 runes),
 * and cannot contain '*', '/', ':', ';', '?', or '\'.
 */
int
isjolietfrog(Rune r)
{
	return r=='*' || r=='/' || r==':'
		|| r==';' || r=='?' || r=='\\';
}

int
isbadjoliet(char *s)
{
	Rune r[256], *p;

	if(utflen(s) > 64)
		return 1;
	strtorune(r, s);
	for(p=r; *p; p++)
		if(isjolietfrog(*p))
			return 1;
	return 0;
}

/*
 * Joliet name comparison
 *
 * The standard algorithm is the ISO9660 algorithm but
 * on the encoded Runes.  Runes are encoded in big endian
 * format, so we can just use runecmp.
 *
 * Padding is with zeros, but that still doesn't affect us.
 */

static Rune emptystring[] = { (Rune)0 };
int
jolietcmp(const void *va, const void *vb)
{
	int i;
	Rune s1[256], s2[256], *b1, *b2, *e1, *e2;	/*BUG*/
	const Direc *a, *b;

	a = va;
	b = vb;

	b1 = strtorune(s1, a->confname);
	b2 = strtorune(s2, b->confname);
	if((e1 = runechr(b1, (Rune)'.')) != nil)
		*e1++ = '\0';
	else
		e1 = emptystring;

	if((e2 = runechr(b2, (Rune)'.')) != nil)
		*e2++ = '\0';
	else
		e2 = emptystring;

	if((i = runecmp(b1, b2)) != 0)
		return i;

	return runecmp(e1, e2);
}

/*
 * Write a Joliet secondary volume descriptor.
 */
void
Cputjolietsvd(Cdimg *cd, Cdinfo info)
{
	Cputc(cd, 2);				/* secondary volume descriptor */
	Cputs(cd, "CD001", 5);			/* standard identifier */
	Cputc(cd, 1);				/* volume descriptor version */
	Cputc(cd, 0);				/* unused */

	Cputrscvt(cd, "Joliet Plan 9", 32);			/* system identifier */
	Cputrscvt(cd, info.volumename, 32);			/* volume identifier */

	Crepeat(cd, 0, 8);				/* unused */
	Cputn(cd, 0, 4);				/* volume space size */
	Cputc(cd, 0x25);				/* escape sequences: UCS-2 Level 2 */
	Cputc(cd, 0x2F);
	Cputc(cd, 0x43);

	Crepeat(cd, 0, 29);
	Cputn(cd, 1, 2);				/* volume set size */
	Cputn(cd, 1, 2);				/* volume sequence number */
	Cputn(cd, Blocksize, 2);			/* logical block size */
	Cputn(cd, 0, 4);				/* path table size */
	Cputnl(cd, 0, 4);				/* location of Lpath */
	Cputnl(cd, 0, 4);				/* location of optional Lpath */
	Cputnm(cd, 0, 4);				/* location of Mpath */
	Cputnm(cd, 0, 4);				/* location of optional Mpath */
	Cputjolietdir(cd, nil, DTroot, 1, Cwoffset(cd));			/* root directory */
	Cputrscvt(cd, info.volumeset, 128);		/* volume set identifier */
	Cputrscvt(cd, info.publisher, 128);			/* publisher identifier */
	Cputrscvt(cd, info.preparer, 128);			/* data preparer identifier */
	Cputrscvt(cd, info.application, 128);		/* application identifier */
	Cputrscvt(cd, "", 37);			/* copyright notice */
	Cputrscvt(cd, "", 37);			/* abstract */
	Cputrscvt(cd, "", 37);			/* bibliographic file */
	Cputdate1(cd, now);				/* volume creation date */
	Cputdate1(cd, now);				/* volume modification date */
	Cputdate1(cd, 0);				/* volume expiration date */
	Cputdate1(cd, 0);				/* volume effective date */
	Cputc(cd, 1);				/* file structure version */
	Cpadblock(cd);
}
