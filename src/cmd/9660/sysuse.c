/*
 * To understand this code, see Rock Ridge Interchange Protocol
 * standard 1.12 and System Use Sharing Protocol version 1.12
 * (search for rrip112.ps and susp112.ps on the web).
 *
 * Even better, go read something else.
 */

#include <u.h>
#include <libc.h>
#include <bio.h>
#include <libsec.h>
#include "iso9660.h"

static long mode(Direc*, int);
static long nlink(Direc*);
static ulong suspdirflags(Direc*, int);
static ulong CputsuspCE(Cdimg *cd, ulong offset);
static int CputsuspER(Cdimg*, int);
static int CputsuspRR(Cdimg*, int, int);
static int CputsuspSP(Cdimg*, int);
/*static int CputsuspST(Cdimg*, int); */
static int Cputrripname(Cdimg*, char*, int, char*, int);
static int CputrripSL(Cdimg*, int, int, char*, int);
static int CputrripPX(Cdimg*, Direc*, int, int);
static int CputrripTF(Cdimg*, Direc*, int, int);

/*
 * Patch the length field in a CE record.
 */
static void
setcelen(Cdimg *cd, ulong woffset, ulong len)
{
	ulong o;

	o = Cwoffset(cd);
	Cwseek(cd, woffset);
	Cputn(cd, len, 4);
	Cwseek(cd, o);
}

/*
 * Rock Ridge data is put into little blockettes, which can be
 * at most 256 bytes including a one-byte length.  Some number
 * of blockettes get packed together into a normal 2048-byte block.
 * Blockettes cannot cross block boundaries.
 *
 * A Cbuf is a blockette buffer.  Len contains
 * the length of the buffer written so far, and we can
 * write up to 254-28.
 *
 * We only have one active Cbuf at a time; cdimg.rrcontin is the byte
 * offset of the beginning of that Cbuf.
 *
 * The blockette can be at most 255 bytes.  The last 28
 * will be (in the worst case) a CE record pointing at
 * a new blockette.  If we do write 255 bytes though,
 * we'll try to pad it out to be even, and overflow.
 * So the maximum is 254-28.
 *
 * Ceoffset contains the offset to be used with setcelen
 * to patch the CE pointing at the Cbuf once we know how
 * long the Cbuf is.
 */
typedef struct Cbuf Cbuf;
struct Cbuf {
	int len;	/* written so far, of 254-28 */
	ulong ceoffset;
};

static int
freespace(Cbuf *cp)
{
	return (254-28) - cp->len;
}

static Cbuf*
ensurespace(Cdimg *cd, int n, Cbuf *co, Cbuf *cn, int dowrite)
{
	ulong end;

	if(co->len+n <= 254-28) {
		co->len += n;
		return co;
	}

	co->len += 28;
	assert(co->len <= 254);

	if(dowrite == 0) {
		cn->len = n;
		return cn;
	}

	/*
	 * the current blockette is full; update cd->rrcontin and then
 	 * write a CE record to finish it.  Unfortunately we need to
	 * figure out which block will be next before we write the CE.
	 */
	end = Cwoffset(cd)+28;

	/*
	 * if we're in a continuation blockette, update rrcontin.
	 * also, write our length into the field of the CE record
	 * that points at us.
	 */
	if(cd->rrcontin+co->len == end) {
		assert(cd->rrcontin != 0);
		assert(co == cn);
		cd->rrcontin += co->len;
		setcelen(cd, co->ceoffset, co->len);
	} else
		assert(co != cn);

	/*
	 * if the current continuation block can't fit another
	 * blockette, then start a new continuation block.
	 * rrcontin = 0 (mod Blocksize) means we just finished
	 * one, not that we've just started one.
	 */
	if(cd->rrcontin%Blocksize == 0
	|| cd->rrcontin/Blocksize != (cd->rrcontin+256)/Blocksize) {
		cd->rrcontin = cd->nextblock*Blocksize;
		cd->nextblock++;
	}

	cn->ceoffset = CputsuspCE(cd, cd->rrcontin);

	assert(Cwoffset(cd) == end);

	cn->len = n;
	Cwseek(cd, cd->rrcontin);
	assert(cd->rrcontin != 0);

	return cn;
}

/*
 * Put down the name, but we might need to break it
 * into chunks so that each chunk fits in 254-28-5 bytes.
 * What a crock.
 *
 * The new Plan 9 format uses strings of this form too,
 * since they're already there.
 */
Cbuf*
Cputstring(Cdimg *cd, Cbuf *cp, Cbuf *cn, char *nm, char *p, int flags, int dowrite)
{
	char buf[256], *q;
	int free;

	for(; p[0] != '\0'; p = q) {
		cp = ensurespace(cd, 5+1, cp, cn, dowrite);
		cp->len -= 5+1;
		free = freespace(cp);
		assert(5+1 <= free && free < 256);

		strncpy(buf, p, free-5);
		buf[free-5] = '\0';
		q = p+strlen(buf);
		p = buf;

		ensurespace(cd, 5+strlen(p), cp, nil, dowrite);	/* nil: better not use this. */
		Cputrripname(cd, nm, flags | (q[0] ? NMcontinue : 0), p, dowrite);
	}
	return cp;
}

/*
 * Write a Rock Ridge SUSP set of records for a directory entry.
 */
int
Cputsysuse(Cdimg *cd, Direc *d, int dot, int dowrite, int initlen)
{
	char buf[256], buf0[256], *nextpath, *p, *path, *q;
	int flags, free, m, what;
	ulong o;
	Cbuf cn, co, *cp;

	assert(cd != nil);
	assert((initlen&1) == 0);

	if(dot == DTroot)
		return 0;

	co.len = initlen;

	o = Cwoffset(cd);

	assert(dowrite==0 || Cwoffset(cd) == o+co.len-initlen);
	cp = &co;

	if (dot == DTrootdot) {
		m = CputsuspSP(cd, 0);
		cp = ensurespace(cd, m, cp, &cn, dowrite);
		CputsuspSP(cd, dowrite);

		m = CputsuspER(cd, 0);
		cp = ensurespace(cd, m, cp, &cn, dowrite);
		CputsuspER(cd, dowrite);
	}

	/*
	 * In a perfect world, we'd be able to omit the NM
	 * entries when our name was all lowercase and conformant,
	 * but OpenBSD insists on uppercasing (really, not lowercasing)
	 * the ISO9660 names.
	 */
	what = RR_PX | RR_TF | RR_NM;
	if(d != nil && (d->mode & CHLINK))
		what |= RR_SL;

	m = CputsuspRR(cd, what, 0);
	cp = ensurespace(cd, m, cp, &cn, dowrite);
	CputsuspRR(cd, what, dowrite);

	if(what & RR_PX) {
		m = CputrripPX(cd, d, dot, 0);
		cp = ensurespace(cd, m, cp, &cn, dowrite);
		CputrripPX(cd, d, dot, dowrite);
	}

	if(what & RR_NM) {
		if(dot == DTiden)
			p = d->name;
		else if(dot == DTdotdot)
			p = "..";
		else
			p = ".";

		flags = suspdirflags(d, dot);
		assert(dowrite==0 || cp != &co || Cwoffset(cd) == o+co.len-initlen);
		cp = Cputstring(cd, cp, &cn, "NM", p, flags, dowrite);
	}

	/*
	 * Put down the symbolic link.  This is even more of a crock.
	 * Not only are the individual elements potentially split,
	 * but the whole path itself can be split across SL blocks.
	 * To keep the code simple as possible (really), we write
	 * only one element per SL block, wasting 6 bytes per element.
	 */
	if(what & RR_SL) {
		for(path=d->symlink; path[0] != '\0'; path=nextpath) {
			/* break off one component */
			if((nextpath = strchr(path, '/')) == nil)
				nextpath = path+strlen(path);
			strncpy(buf0, path, nextpath-path);
			buf0[nextpath-path] = '\0';
			if(nextpath[0] == '/')
				nextpath++;
			p = buf0;

			/* write the name, perhaps broken into pieces */
			if(strcmp(p, "") == 0)
				flags = NMroot;
			else if(strcmp(p, ".") == 0)
				flags = NMcurrent;
			else if(strcmp(p, "..") == 0)
				flags = NMparent;
			else
				flags = 0;

			/* the do-while handles the empty string properly */
			do {
				/* must have room for at least 1 byte of name */
				cp = ensurespace(cd, 7+1, cp, &cn, dowrite);
				cp->len -= 7+1;
				free = freespace(cp);
				assert(7+1 <= free && free < 256);

				strncpy(buf, p, free-7);
				buf[free-7] = '\0';
				q = p+strlen(buf);
				p = buf;

				/* nil: better not need to expand */
				assert(7+strlen(p) <= free);
				ensurespace(cd, 7+strlen(p), cp, nil, dowrite);
				CputrripSL(cd, nextpath[0], flags | (q[0] ? NMcontinue : 0), p, dowrite);
				p = q;
			} while(p[0] != '\0');
		}
	}

	assert(dowrite==0 || cp != &co || Cwoffset(cd) == o+co.len-initlen);

	if(what & RR_TF) {
		m = CputrripTF(cd, d, TFcreation|TFmodify|TFaccess|TFattributes, 0);
		cp = ensurespace(cd, m, cp, &cn, dowrite);
		CputrripTF(cd, d, TFcreation|TFmodify|TFaccess|TFattributes, dowrite);
	}
	assert(dowrite==0 || cp != &co || Cwoffset(cd) == o+co.len-initlen);

	if(cp == &cn && dowrite) {
		/* seek out of continuation, but mark our place */
		cd->rrcontin = Cwoffset(cd);
		setcelen(cd, cn.ceoffset, cn.len);
		Cwseek(cd, o+co.len-initlen);
	}

	if(co.len & 1) {
		co.len++;
		if(dowrite)
			Cputc(cd, 0);
	}

	if(dowrite) {
		if(Cwoffset(cd) != o+co.len-initlen)
			fprint(2, "offset %lud o+co.len-initlen %lud\n", Cwoffset(cd), o+co.len-initlen);
		assert(Cwoffset(cd) == o+co.len-initlen);
	} else
		assert(Cwoffset(cd) == o);

	assert(co.len <= 255);
	return co.len - initlen;
}

static char SUSPrrip[10] = "RRIP_1991A";
static char SUSPdesc[84] = "RRIP <more garbage here>";
static char SUSPsrc[135] = "RRIP <more garbage here>";

static ulong
CputsuspCE(Cdimg *cd, ulong offset)
{
	ulong o, x;

	chat("writing SUSP CE record pointing to %ld, %ld\n", offset/Blocksize, offset%Blocksize);
	o = Cwoffset(cd);
	Cputc(cd, 'C');
	Cputc(cd, 'E');
	Cputc(cd, 28);
	Cputc(cd, 1);
	Cputn(cd, offset/Blocksize, 4);
	Cputn(cd, offset%Blocksize, 4);
	x = Cwoffset(cd);
	Cputn(cd, 0, 4);
	assert(Cwoffset(cd) == o+28);

	return x;
}

static int
CputsuspER(Cdimg *cd, int dowrite)
{
	assert(cd != nil);

	if(dowrite) {
		chat("writing SUSP ER record\n");
		Cputc(cd, 'E');           /* ER field marker */
		Cputc(cd, 'R');
		Cputc(cd, 26);            /* Length          */
		Cputc(cd, 1);             /* Version         */
		Cputc(cd, 10);            /* LEN_ID          */
		Cputc(cd, 4);             /* LEN_DESC        */
		Cputc(cd, 4);             /* LEN_SRC         */
		Cputc(cd, 1);             /* EXT_VER         */
		Cputs(cd, SUSPrrip, 10);  /* EXT_ID          */
		Cputs(cd, SUSPdesc, 4);   /* EXT_DESC        */
		Cputs(cd, SUSPsrc, 4);    /* EXT_SRC         */
	}
	return 8+10+4+4;
}

static int
CputsuspRR(Cdimg *cd, int what, int dowrite)
{
	assert(cd != nil);

	if(dowrite) {
		Cputc(cd, 'R');           /* RR field marker */
		Cputc(cd, 'R');
		Cputc(cd, 5);             /* Length          */
		Cputc(cd, 1);		  /* Version number  */
		Cputc(cd, what);          /* Flags           */
	}
	return 5;
}

static int
CputsuspSP(Cdimg *cd, int dowrite)
{
	assert(cd!=0);

	if(dowrite) {
chat("writing SUSP SP record\n");
		Cputc(cd, 'S');           /* SP field marker */
		Cputc(cd, 'P');
		Cputc(cd, 7);             /* Length          */
		Cputc(cd, 1);             /* Version         */
		Cputc(cd, 0xBE);          /* Magic           */
		Cputc(cd, 0xEF);
		Cputc(cd, 0);
	}

	return 7;
}

#ifdef NOTUSED
static int
CputsuspST(Cdimg *cd, int dowrite)
{
	assert(cd!=0);

	if(dowrite) {
		Cputc(cd, 'S');           /* ST field marker */
		Cputc(cd, 'T');
		Cputc(cd, 4);             /* Length          */
		Cputc(cd, 1);             /* Version         */
	}
	return 4;
}
#endif

static ulong
suspdirflags(Direc *d, int dot)
{
	uchar flags;

	USED(d);
	flags = 0;
	switch(dot) {
	default:
		assert(0);
	case DTdot:
	case DTrootdot:
		flags |= NMcurrent;
		break;
	case DTdotdot:
		flags |= NMparent;
		break;
	case DTroot:
		flags |= NMvolroot;
		break;
	case DTiden:
		break;
	}
	return flags;
}

static int
Cputrripname(Cdimg *cd, char *nm, int flags, char *name, int dowrite)
{
	int l;

	l = strlen(name);
	if(dowrite) {
		Cputc(cd, nm[0]);                   /* NM field marker */
		Cputc(cd, nm[1]);
		Cputc(cd, l+5);        /* Length          */
		Cputc(cd, 1);                     /* Version         */
		Cputc(cd, flags);                 /* Flags           */
		Cputs(cd, name, l);    /* Alternate name  */
	}
	return 5+l;
}

static int
CputrripSL(Cdimg *cd, int contin, int flags, char *name, int dowrite)
{
	int l;

	l = strlen(name);
	if(dowrite) {
		Cputc(cd, 'S');
		Cputc(cd, 'L');
		Cputc(cd, l+7);
		Cputc(cd, 1);
		Cputc(cd, contin ? 1 : 0);
		Cputc(cd, flags);
		Cputc(cd, l);
		Cputs(cd, name, l);
	}
	return 7+l;
}

static int
CputrripPX(Cdimg *cd, Direc *d, int dot, int dowrite)
{
	assert(cd!=0);

	if(dowrite) {
		Cputc(cd, 'P');             /* PX field marker */
		Cputc(cd, 'X');
		Cputc(cd, 36);              /* Length          */
		Cputc(cd, 1);               /* Version         */

		Cputn(cd, mode(d, dot), 4); /* POSIX File mode */
		Cputn(cd, nlink(d), 4);     /* POSIX st_nlink  */
		Cputn(cd, d?d->uidno:0, 4);  /* POSIX st_uid    */
		Cputn(cd, d?d->gidno:0, 4);  /* POSIX st_gid    */
	}

	return 36;
}

static int
CputrripTF(Cdimg *cd, Direc *d, int type, int dowrite)
{
	int i, length;

	assert(cd!=0);
	assert(!(type & TFlongform));

	length = 0;
	for(i=0; i<7; i++)
		if (type & (1<<i))
			length++;
	assert(length == 4);

	if(dowrite) {
		Cputc(cd, 'T');				/* TF field marker */
		Cputc(cd, 'F');
		Cputc(cd, 5+7*length);		/* Length		 */
		Cputc(cd, 1);				/* Version		 */
		Cputc(cd, type);					/* Flags (types)	 */

		if (type & TFcreation)
			Cputdate(cd, d?d->ctime:0);
		if (type & TFmodify)
			Cputdate(cd, d?d->mtime:0);
		if (type & TFaccess)
			Cputdate(cd, d?d->atime:0);
		if (type & TFattributes)
			Cputdate(cd, d?d->ctime:0);

	/*	if (type & TFbackup) */
	/*		Cputdate(cd, 0); */
	/*	if (type & TFexpiration) */
	/*		Cputdate(cd, 0); */
	/*	if (type & TFeffective) */
	/*		Cputdate(cd, 0); */
	}
	return 5+7*length;
}


#define NONPXMODES  (DMDIR & DMAPPEND & DMEXCL & DMMOUNT)
#define POSIXMODEMASK (0177777)
#ifndef S_IFMT
#define S_IFMT  (0170000)
#endif
#ifndef S_IFDIR
#define S_IFDIR (0040000)
#endif
#ifndef S_IFREG
#define S_IFREG (0100000)
#endif
#ifndef S_IFLNK
#define S_IFLNK (0120000)
#endif
#undef  ISTYPE
#define ISTYPE(mode, mask)  (((mode) & S_IFMT) == (mask))
#ifndef S_ISDIR
#define S_ISDIR(mode) ISTYPE(mode, S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(mode) ISTYPE(mode, S_IREG)
#endif
#ifndef S_ISLNK
#define S_ISLNK(mode) ISTYPE(mode, S_ILNK)
#endif


static long
mode(Direc *d, int dot)
{
	long mode;

	if (!d)
		return 0;

	if ((dot != DTroot) && (dot != DTrootdot)) {
		mode = (d->mode & ~(NONPXMODES));
		if (d->mode & DMDIR)
			mode |= S_IFDIR;
		else if (d->mode & CHLINK)
			mode |= S_IFLNK;
		else
			mode |= S_IFREG;
	} else
		mode = S_IFDIR | (0755);

	mode &= POSIXMODEMASK;

	/* Botch: not all POSIX types supported yet */
	assert(mode & (S_IFDIR|S_IFREG));

chat("writing PX record mode field %ulo with dot %d and name \"%s\"\n", mode, dot, d->name);

	return mode;
}

static long
nlink(Direc *d)   /* Trump up the nlink field for POSIX compliance */
{
	int i;
	long n;

	if (!d)
		return 0;

	n = 1;
	if (d->mode & DMDIR)   /* One for "." and one more for ".." */
		n++;

	for(i=0; i<d->nchild; i++)
		if (d->child[i].mode & DMDIR)
			n++;

	return n;
}
