#include <u.h>
#include <libc.h>
#include <bio.h>
#include <libsec.h>

#include "iso9660.h"

static void
writelittlebig4(uchar *buf, ulong x)
{
	buf[0] = buf[7] = x;
	buf[1] = buf[6] = x>>8;
	buf[2] = buf[5] = x>>16;
	buf[3] = buf[4] = x>>24;
}

void
rewritedot(Cdimg *cd, Direc *d)
{
	uchar buf[Blocksize];
	Cdir *c;

	Creadblock(cd, buf, d->block, Blocksize);
	c = (Cdir*)buf;
	assert(c->len != 0);
	assert(c->namelen == 1 && c->name[0] == '\0');	/* dot */
	writelittlebig4(c->dloc, d->block);
	writelittlebig4(c->dlen, d->length);

	Cwseek(cd, d->block*Blocksize);
	Cwrite(cd, buf, Blocksize);
}

void
rewritedotdot(Cdimg *cd, Direc *d, Direc *dparent)
{
	uchar buf[Blocksize];
	Cdir *c;

	Creadblock(cd, buf, d->block, Blocksize);
	c = (Cdir*)buf;
	assert(c->len != 0);
	assert(c->namelen == 1 && c->name[0] == '\0');	/* dot */

	c = (Cdir*)(buf+c->len);
	assert(c->len != 0);
	assert(c->namelen == 1 && c->name[0] == '\001');	/* dotdot*/

	writelittlebig4(c->dloc, dparent->block);
	writelittlebig4(c->dlen, dparent->length);

	Cwseek(cd, d->block*Blocksize);
	Cwrite(cd, buf, Blocksize);
}

/*
 * Write each non-directory file.  We copy the file to
 * the cd image, and then if it turns out that we've
 * seen this stream of bits before, we push the next block
 * pointer back.  This ensures consistency between the MD5s
 * and the data on the CD image.  MD5 summing on one pass
 * and copying on another would not ensure this.
 */
void
writefiles(Dump *d, Cdimg *cd, Direc *direc)
{
	int i;
	uchar buf[8192], digest[MD5dlen];
	ulong length, n, start;
	Biobuf *b;
	DigestState *s;
	Dumpdir *dd;

	if(direc->mode & DMDIR) {
		for(i=0; i<direc->nchild; i++)
			writefiles(d, cd, &direc->child[i]);
		return;
	}

	assert(direc->block == 0);

	if((b = Bopen(direc->srcfile, OREAD)) == nil){
		fprint(2, "warning: cannot open '%s': %r\n", direc->srcfile);
		direc->block = 0;
		direc->length = 0;
		return;
	}

	start = cd->nextblock;
	assert(start != 0);

	Cwseek(cd, start*Blocksize);

	s = md5(nil, 0, nil, nil);
	length = 0;
	while((n = Bread(b, buf, sizeof buf)) > 0) {
		md5(buf, n, nil, s);
		Cwrite(cd, buf, n);
		length += n;
	}
	md5(nil, 0, digest, s);
	Bterm(b);
	Cpadblock(cd);

	if(length != direc->length) {
		fprint(2, "warning: %s changed size underfoot\n", direc->srcfile);
		direc->length = length;
	}

	if(length == 0)
		direc->block = 0;
	else if((dd = lookupmd5(d, digest))) {
		assert(dd->length == length);
		assert(dd->block != 0);
		direc->block = dd->block;
		cd->nextblock = start;
	} else {
		direc->block = start;
		if(chatty > 1)
			fprint(2, "lookup %.16H %lud (%s) failed\n", digest, length, direc->name);
		insertmd5(d, atom(direc->name), digest, start, length);
	}
}

/*
 * Write a directory tree.  We work from the leaves,
 * and patch the dotdot pointers afterward.
 */
static void
_writedirs(Cdimg *cd, Direc *d, int (*put)(Cdimg*, Direc*, int, int, int), int level)
{
	int i, l, ll;
	ulong start, next;

	if((d->mode & DMDIR) == 0)
		return;

	if(chatty)
		fprint(2, "%*s%s\n", 4*level, "", d->name);

	for(i=0; i<d->nchild; i++)
		_writedirs(cd, &d->child[i], put, level+1);

	l = 0;
	l += put(cd, d, (level == 0) ? DTrootdot : DTdot, 0, l);
	l += put(cd, nil, DTdotdot, 0, l);
	for(i=0; i<d->nchild; i++)
		l += put(cd, &d->child[i], DTiden, 0, l);

	start = cd->nextblock;
	cd->nextblock += (l+Blocksize-1)/Blocksize;
	next = cd->nextblock;

	Cwseek(cd, start*Blocksize);
	ll = 0;
	ll += put(cd, d, (level == 0) ? DTrootdot : DTdot, 1, ll);
	ll += put(cd, nil, DTdotdot, 1, ll);
	for(i=0; i<d->nchild; i++)
		ll += put(cd, &d->child[i], DTiden, 1, ll);
	assert(ll == l);
	Cpadblock(cd);
	assert(Cwoffset(cd) == next*Blocksize);

	d->block = start;
	d->length = (next - start) * Blocksize;
	rewritedot(cd, d);
	rewritedotdot(cd, d, d);

	for(i=0; i<d->nchild; i++)
		if(d->child[i].mode & DMDIR)
			rewritedotdot(cd, &d->child[i], d);
}

void
writedirs(Cdimg *cd, Direc *d, int (*put)(Cdimg*, Direc*, int, int, int))
{
	/*
	 * If we're writing a mk9660 image, then the root really
	 * is the root, so start at level 0.  If we're writing a dump image,
	 * then the "root" is really going to be two levels down once
	 * we patch in the dump hierarchy above it, so start at level non-zero.
	 */
	if(chatty)
		fprint(2, ">>> writedirs\n");
	_writedirs(cd, d, put, mk9660 ? 0 : 1);
}


/*
 * Write the dump tree.  This is like writedirs but once we get to
 * the roots of the individual days we just patch the parent dotdot blocks.
 */
static void
_writedumpdirs(Cdimg *cd, Direc *d, int (*put)(Cdimg*, Direc*, int, int, int), int level)
{
	int i;
	ulong start;

	switch(level) {
	case 0:
		/* write root, list of years, also conform.map */
		for(i=0; i<d->nchild; i++)
			if(d->child[i].mode & DMDIR)
				_writedumpdirs(cd, &d->child[i], put, level+1);
		chat("write dump root dir at %lud\n", cd->nextblock);
		goto Writedir;

	case 1:	/* write year, list of days */
		for(i=0; i<d->nchild; i++)
			_writedumpdirs(cd, &d->child[i], put, level+1);
		chat("write dump %s dir at %lud\n", d->name, cd->nextblock);
		goto Writedir;

	Writedir:
		start = cd->nextblock;
		Cwseek(cd, start*Blocksize);

		put(cd, d, (level == 0) ? DTrootdot : DTdot, 1, Cwoffset(cd));
		put(cd, nil, DTdotdot, 1, Cwoffset(cd));
		for(i=0; i<d->nchild; i++)
			put(cd, &d->child[i], DTiden, 1, Cwoffset(cd));
		Cpadblock(cd);

		d->block = start;
		d->length = (cd->nextblock - start) * Blocksize;

		rewritedot(cd, d);
		rewritedotdot(cd, d, d);

		for(i=0; i<d->nchild; i++)
			if(d->child[i].mode & DMDIR)
				rewritedotdot(cd, &d->child[i], d);
		break;

	case 2:	/* write day: already written, do nothing */
		break;

	default:
		assert(0);
	}
}

void
writedumpdirs(Cdimg *cd, Direc *d, int (*put)(Cdimg*, Direc*, int, int, int))
{
	_writedumpdirs(cd, d, put, 0);
}

static int
Cputplan9(Cdimg *cd, Direc *d, int dot, int dowrite)
{
	int l, n;

	if(dot != DTiden)
		return 0;

	l = 0;
	if(d->flags & Dbadname) {
		n = strlen(d->name);
		l += 1+n;
		if(dowrite) {
			Cputc(cd, n);
			Cputs(cd, d->name, n);
		}
	} else {
		l++;
		if(dowrite)
			Cputc(cd, 0);
	}

	n = strlen(d->uid);
	l += 1+n;
	if(dowrite) {
		Cputc(cd, n);
		Cputs(cd, d->uid, n);
	}

	n = strlen(d->gid);
	l += 1+n;
	if(dowrite) {
		Cputc(cd, n);
		Cputs(cd, d->gid, n);
	}

	if(l & 1) {
		l++;
		if(dowrite)
			Cputc(cd, 0);
	}
	l += 8;
	if(dowrite)
		Cputn(cd, d->mode, 4);

	return l;
}

/*
 * Write a directory entry.
 */
static int
genputdir(Cdimg *cd, Direc *d, int dot, int joliet, int dowrite, int offset)
{
	int f, n, l, lp;
	long o;

	f = 0;
	if(dot != DTiden || (d->mode & DMDIR))
		f |= 2;

	n = 1;
	if(dot == DTiden) {
		if(joliet)
			n = 2*utflen(d->confname);
		else
			n = strlen(d->confname);
	}

	l = 33+n;
	if(l & 1)
		l++;
	assert(l <= 255);

	if(joliet == 0) {
		if(cd->flags & CDplan9)
			l += Cputplan9(cd, d, dot, 0);
		else if(cd->flags & CDrockridge)
			l += Cputsysuse(cd, d, dot, 0, l);
		assert(l <= 255);
	}

	if(dowrite == 0) {
		if(Blocksize - offset%Blocksize < l)
			l += Blocksize - offset%Blocksize;
		return l;
	}

	assert(offset%Blocksize == Cwoffset(cd)%Blocksize);

	o = Cwoffset(cd);
	lp = 0;
	if(Blocksize - Cwoffset(cd)%Blocksize < l) {
		lp = Blocksize - Cwoffset(cd)%Blocksize;
		Cpadblock(cd);
	}

	Cputc(cd, l);			/* length of directory record */
	Cputc(cd, 0);			/* extended attribute record length */
	if(d) {
		if((d->mode & DMDIR) == 0)
			assert(d->length == 0 || d->block >= 18);

		Cputn(cd, d->block, 4);		/* location of extent */
		Cputn(cd, d->length, 4);		/* data length */
	} else {
		Cputn(cd, 0, 4);
		Cputn(cd, 0, 4);
	}
	Cputdate(cd, d ? d->mtime : now);		/* recorded date */
	Cputc(cd, f);			/* file flags */
	Cputc(cd, 0);			/* file unit size */
	Cputc(cd, 0);			/* interleave gap size */
	Cputn(cd, 1, 2);	       	/* volume sequence number */
	Cputc(cd, n);			/* length of file identifier */

	if(dot == DTiden) {		/* identifier */
		if(joliet)
			Cputrscvt(cd, d->confname, n);
		else
			Cputs(cd, d->confname, n);
	}else
	if(dot == DTdotdot)
		Cputc(cd, 1);
	else
		Cputc(cd, 0);

	if(Cwoffset(cd) & 1)			/* pad */
		Cputc(cd, 0);

	if(joliet == 0) {
		if(cd->flags & CDplan9)
			Cputplan9(cd, d, dot, 1);
		else if(cd->flags & CDrockridge)
			Cputsysuse(cd, d, dot, 1, Cwoffset(cd)-(o+lp));
	}

	assert(o+lp+l == Cwoffset(cd));
	return lp+l;
}

int
Cputisodir(Cdimg *cd, Direc *d, int dot, int dowrite, int offset)
{
	return genputdir(cd, d, dot, 0, dowrite, offset);
}

int
Cputjolietdir(Cdimg *cd, Direc *d, int dot, int dowrite, int offset)
{
	return genputdir(cd, d, dot, 1, dowrite, offset);
}

void
Cputendvd(Cdimg *cd)
{
	Cputc(cd, 255);				/* volume descriptor set terminator */
	Cputs(cd, "CD001", 5);			/* standard identifier */
	Cputc(cd, 1);				/* volume descriptor version */
	Cpadblock(cd);
}
