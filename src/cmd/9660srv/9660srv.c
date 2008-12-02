#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include "dat.h"
#include "fns.h"
#include "iso9660.h"

static void	ireset(void);
static int	iattach(Xfile*);
static void	iclone(Xfile*, Xfile*);
static void	iwalkup(Xfile*);
static void	iwalk(Xfile*, char*);
static void	iopen(Xfile*, int);
static void	icreate(Xfile*, char*, long, int);
static long	ireaddir(Xfile*, uchar*, long, long);
static long	iread(Xfile*, char*, vlong, long);
static long	iwrite(Xfile*, char*, vlong, long);
static void	iclunk(Xfile*);
static void	iremove(Xfile*);
static void	istat(Xfile*, Dir*);
static void	iwstat(Xfile*, Dir*);

static char*	nstr(uchar*, int);
static char*	rdate(uchar*, int);
static int	getcontin(Xdata*, uchar*, uchar**);
static int	getdrec(Xfile*, void*);
static void	ungetdrec(Xfile*);
static int	opendotdot(Xfile*, Xfile*);
static int	showdrec(int, int, void*);
static long	gtime(uchar*);
static long	l16(void*);
static long	l32(void*);
static void	newdrec(Xfile*, Drec*);
static int	rzdir(Xfs*, Dir*, int, Drec*);

Xfsub	isosub =
{
	ireset, iattach, iclone, iwalkup, iwalk, iopen, icreate,
	ireaddir, iread, iwrite, iclunk, iremove, istat, iwstat
};

static void
ireset(void)
{}

static int
iattach(Xfile *root)
{
	Xfs *cd = root->xf;
	Iobuf *p; Voldesc *v; Isofile *fp; Drec *dp;
	int fmt, blksize, i, n, l, haveplan9;
	Iobuf *dirp;
	uchar dbuf[256];
	Drec *rd = (Drec *)dbuf;
	uchar *q, *s;

	dirp = nil;
	blksize = 0;
	fmt = 0;
	dp = nil;
	haveplan9 = 0;
	for(i=VOLDESC;i<VOLDESC+100; i++){	/* +100 for sanity */
		p = getbuf(cd->d, i);
		v = (Voldesc*)(p->iobuf);
		if(memcmp(v->byte, "\01CD001\01", 7) == 0){		/* iso */
			if(dirp)
				putbuf(dirp);
			dirp = p;
			fmt = 'z';
			dp = (Drec*)v->z.desc.rootdir;
			blksize = l16(v->z.desc.blksize);
			chat("iso, blksize=%d...", blksize);

			v = (Voldesc*)(dirp->iobuf);
			haveplan9 = (strncmp((char*)v->z.boot.sysid, "PLAN 9", 6)==0);
			if(haveplan9){
				if(noplan9) {
					chat("ignoring plan9");
					haveplan9 = 0;
				} else {
					fmt = '9';
					chat("plan9 iso...");
				}
			}
			continue;
		}

		if(memcmp(&v->byte[8], "\01CDROM\01", 7) == 0){	/* high sierra */
			if(dirp)
				putbuf(dirp);
			dirp = p;
			fmt = 'r';
			dp = (Drec*)v->r.desc.rootdir;
			blksize = l16(v->r.desc.blksize);
			chat("high sierra, blksize=%d...", blksize);
			continue;
		}

		if(haveplan9==0 && !nojoliet
		&& memcmp(v->byte, "\02CD001\01", 7) == 0){
chat("%d %d\n", haveplan9, nojoliet);
			/*
			 * The right thing to do is walk the escape sequences looking
			 * for one of 25 2F 4[035], but Microsoft seems to not honor
			 * the format, which makes it hard to walk over.
			 */
			q = v->z.desc.escapes;
			if(q[0] == 0x25 && q[1] == 0x2F && (q[2] == 0x40 || q[2] == 0x43 || q[2] == 0x45)){	/* Joliet, it appears */
				if(dirp)
					putbuf(dirp);
				dirp = p;
				fmt = 'J';
				dp = (Drec*)v->z.desc.rootdir;
				if(blksize != l16(v->z.desc.blksize))
					fprint(2, "warning: suspicious Joliet blocksize\n");
				chat("joliet...");
				continue;
			}
		}
		putbuf(p);
		if(v->byte[0] == 0xFF)
			break;
	}

	if(fmt == 0){
		if(dirp)
			putbuf(dirp);
		return -1;
	}
	assert(dirp != nil);

	if(chatty)
		showdrec(2, fmt, dp);
	if(blksize > Sectorsize){
		chat("blksize too big...");
		putbuf(dirp);
		return -1;
	}
	if(waserror()){
		putbuf(dirp);
		nexterror();
	}
	root->len = sizeof(Isofile) - sizeof(Drec) + dp->z.reclen;
	root->ptr = fp = ealloc(root->len);

	if(haveplan9)
		root->xf->isplan9 = 1;

	fp->fmt = fmt;
	fp->blksize = blksize;
	fp->offset = 0;
	fp->doffset = 0;
	memmove(&fp->d, dp, dp->z.reclen);
	root->qid.path = l32(dp->z.addr);
	root->qid.type = QTDIR;
	putbuf(dirp);
	poperror();
	if(getdrec(root, rd) >= 0){
		n = rd->z.reclen-(34+rd->z.namelen);
		s = (uchar*)rd->z.name + rd->z.namelen;
		if((uintptr)s & 1){
			s++;
			n--;
		}
		if(n >= 7 && s[0] == 'S' && s[1] == 'P' && s[2] == 7 &&
		   s[3] == 1 && s[4] == 0xBE && s[5] == 0xEF){
			root->xf->issusp = 1;
			root->xf->suspoff = s[6];
			n -= root->xf->suspoff;
			s += root->xf->suspoff;
			for(; n >= 4; s += l, n -= l){
				l = s[2];
				if(s[0] == 'E' && s[1] == 'R'){
					if(!norock && s[4] == 10 && memcmp(s+8, "RRIP_1991A", 10) == 0)
						root->xf->isrock = 1;
					break;
				} else if(s[0] == 'C' && s[1] == 'E' && s[2] >= 28){
					n = getcontin(root->xf->d, s, &s);
					continue;
				} else if(s[0] == 'R' && s[1] == 'R'){
					if(!norock)
						root->xf->isrock = 1;
					break;
				} else if(s[0] == 'S' && s[1] == 'T')
					break;
			}
		}
	}
	if(root->xf->isrock)
		chat("Rock Ridge...");
	fp->offset = 0;
	fp->doffset = 0;
	return 0;
}

static void
iclone(Xfile *of, Xfile *nf)
{
	USED(of);
	USED(nf);
}

static void
iwalkup(Xfile *f)
{
	long paddr;
	uchar dbuf[256];
	Drec *d = (Drec *)dbuf;
	Xfile pf, ppf;
	Isofile piso, ppiso;

	memset(&pf, 0, sizeof pf);
	memset(&ppf, 0, sizeof ppf);
	pf.ptr = &piso;
	ppf.ptr = &ppiso;
	if(opendotdot(f, &pf) < 0)
		error("can't open pf");
	paddr = l32(pf.ptr->d.z.addr);
	if(l32(f->ptr->d.z.addr) == paddr)
		return;
	if(opendotdot(&pf, &ppf) < 0)
		error("can't open ppf");
	while(getdrec(&ppf, d) >= 0){
		if(l32(d->z.addr) == paddr){
			newdrec(f, d);
			f->qid.path = paddr;
			f->qid.type = QTDIR;
			return;
		}
	}
	error("can't find addr of ..");
}

static int
casestrcmp(int isplan9, char *a, char *b)
{
	int ca, cb;

	if(isplan9)
		return strcmp(a, b);
	for(;;) {
		ca = *a++;
		cb = *b++;
		if(ca >= 'A' && ca <= 'Z')
			ca += 'a' - 'A';
		if(cb >= 'A' && cb <= 'Z')
			cb += 'a' - 'A';
		if(ca != cb) {
			if(ca > cb)
				return 1;
			return -1;
		}
		if(ca == 0)
			return 0;
	}
}

static void
iwalk(Xfile *f, char *name)
{
	Isofile *ip = f->ptr;
	uchar dbuf[256];
	char nbuf[4*Maxname];
	Drec *d = (Drec*)dbuf;
	Dir dir;
	char *p;
	int len, vers, dvers;

	vers = -1;
	if(p = strchr(name, ';')) {	/* assign = */
		len = p-name;
		if(len >= Maxname)
			len = Maxname-1;
		memmove(nbuf, name, len);
		vers = strtoul(p+1, 0, 10);
		name = nbuf;
	}
/*
	len = strlen(name);
	if(len >= Maxname){
		len = Maxname-1;
		if(name != nbuf){
			memmove(nbuf, name, len);
			name = nbuf;
		}
		name[len] = 0;
	}
*/

	chat("%d \"%s\"...", strlen(name), name);
	ip->offset = 0;
	setnames(&dir, nbuf);
	while(getdrec(f, d) >= 0) {
		dvers = rzdir(f->xf, &dir, ip->fmt, d);
		if(casestrcmp(f->xf->isplan9||f->xf->isrock, name, dir.name) != 0)
			continue;
		newdrec(f, d);
		f->qid.path = dir.qid.path;
		f->qid.type = dir.qid.type;
		USED(dvers);
		return;
	}
	USED(vers);
	error(Enonexist);
}

static void
iopen(Xfile *f, int mode)
{
	mode &= ~OCEXEC;
	if(mode != OREAD && mode != OEXEC)
		error(Eperm);
	f->ptr->offset = 0;
	f->ptr->doffset = 0;
}

static void
icreate(Xfile *f, char *name, long perm, int mode)
{
	USED(f);
	USED(name);
	USED(perm);
	USED(mode);
	error(Eperm);
}

static long
ireaddir(Xfile *f, uchar *buf, long offset, long count)
{
	Isofile *ip = f->ptr;
	Dir d;
	char names[4*Maxname];
	uchar dbuf[256];
	Drec *drec = (Drec *)dbuf;
	int n, rcnt;

	if(offset==0){
		ip->offset = 0;
		ip->doffset = 0;
	}else if(offset != ip->doffset)
		error("seek in directory not allowed");

	rcnt = 0;
	setnames(&d, names);
	while(rcnt < count && getdrec(f, drec) >= 0){
		if(drec->z.namelen == 1){
			if(drec->z.name[0] == 0)
				continue;
			if(drec->z.name[0] == 1)
				continue;
		}
		rzdir(f->xf, &d, ip->fmt, drec);
		d.qid.vers = f->qid.vers;
		if((n = convD2M(&d, buf+rcnt, count-rcnt)) <= BIT16SZ){
			ungetdrec(f);
			break;
		}
		rcnt += n;
	}
	ip->doffset += rcnt;
	return rcnt;
}

static long
iread(Xfile *f, char *buf, vlong offset, long count)
{
	int n, o, rcnt = 0;
	long size;
	vlong addr;
	Isofile *ip = f->ptr;
	Iobuf *p;

	size = l32(ip->d.z.size);
	if(offset >= size)
		return 0;
	if(offset+count > size)
		count = size - offset;
	addr = ((vlong)l32(ip->d.z.addr) + ip->d.z.attrlen)*ip->blksize + offset;
	o = addr % Sectorsize;
	addr /= Sectorsize;
	/*chat("d.addr=%ld, addr=%lld, o=%d...", l32(ip->d.z.addr), addr, o);*/
	n = Sectorsize - o;

	while(count > 0){
		if(n > count)
			n = count;
		p = getbuf(f->xf->d, addr);
		memmove(&buf[rcnt], &p->iobuf[o], n);
		putbuf(p);
		count -= n;
		rcnt += n;
		++addr;
		o = 0;
		n = Sectorsize;
	}
	return rcnt;
}

static long
iwrite(Xfile *f, char *buf, vlong offset, long count)
{
	USED(f);
	USED(buf);
	USED(offset);
	USED(count);
	error(Eperm);
	return 0;
}

static void
iclunk(Xfile *f)
{
	USED(f);
}

static void
iremove(Xfile *f)
{
	USED(f);
	error(Eperm);
}

static void
istat(Xfile *f, Dir *d)
{
	Isofile *ip = f->ptr;

	rzdir(f->xf, d, ip->fmt, &ip->d);
	d->qid.vers = f->qid.vers;
	if(d->qid.path==f->xf->rootqid.path){
		d->qid.path = 0;
		d->qid.type = QTDIR;
	}
}

static void
iwstat(Xfile *f, Dir *d)
{
	USED(f);
	USED(d);
	error(Eperm);
}

static int
showdrec(int fd, int fmt, void *x)
{
	Drec *d = (Drec *)x;
	int namelen;
	int syslen;

	if(d->z.reclen == 0)
		return 0;
	fprint(fd, "%d %d %ld %ld ",
		d->z.reclen, d->z.attrlen, l32(d->z.addr), l32(d->z.size));
	fprint(fd, "%s 0x%2.2x %d %d %ld ",
		rdate(d->z.date, fmt), (fmt=='z' ? d->z.flags : d->r.flags),
		d->z.unitsize, d->z.gapsize, l16(d->z.vseqno));
	fprint(fd, "%d %s", d->z.namelen, nstr(d->z.name, d->z.namelen));
	if(fmt != 'J'){
		namelen = d->z.namelen + (1-(d->z.namelen&1));
		syslen = d->z.reclen - 33 - namelen;
		if(syslen != 0)
			fprint(fd, " %s", nstr(&d->z.name[namelen], syslen));
	}
	fprint(fd, "\n");
	return d->z.reclen + (d->z.reclen&1);
}

static void
newdrec(Xfile *f, Drec *dp)
{
	Isofile *x = f->ptr;
	Isofile *n;
	int len;

	len = sizeof(Isofile) - sizeof(Drec) + dp->z.reclen;
	n = ealloc(len);
	n->fmt = x->fmt;
	n->blksize = x->blksize;
	n->offset = 0;
	n->doffset = 0;
	memmove(&n->d, dp, dp->z.reclen);
	free(x);
	f->ptr = n;
	f->len = len;
}

static void
ungetdrec(Xfile *f)
{
	Isofile *ip = f->ptr;

	if(ip->offset >= ip->odelta){
		ip->offset -= ip->odelta;
		ip->odelta = 0;
	}
}

static int
getdrec(Xfile *f, void *buf)
{
	Isofile *ip = f->ptr;
	int len = 0, boff = 0;
	ulong size;
	vlong addr;
	Iobuf *p = 0;

	if(!ip)
		return -1;
	size = l32(ip->d.z.size);
	while(ip->offset < size){
		addr = (l32(ip->d.z.addr)+ip->d.z.attrlen)*ip->blksize + ip->offset;
		boff = addr % Sectorsize;
		if(boff > Sectorsize-34){
			ip->offset += Sectorsize-boff;
			continue;
		}
		p = getbuf(f->xf->d, addr/Sectorsize);
		len = p->iobuf[boff];
		if(len >= 34)
			break;
		putbuf(p);
		p = 0;
		ip->offset += Sectorsize-boff;
	}
	if(p) {
		memmove(buf, &p->iobuf[boff], len);
		putbuf(p);
		ip->odelta = len + (len&1);
		ip->offset += ip->odelta;
		return 0;
	}
	return -1;
}

static int
opendotdot(Xfile *f, Xfile *pf)
{
	uchar dbuf[256];
	Drec *d = (Drec *)dbuf;
	Isofile *ip = f->ptr, *pip = pf->ptr;

	ip->offset = 0;
	if(getdrec(f, d) < 0){
		chat("opendotdot: getdrec(.) failed...");
		return -1;
	}
	if(d->z.namelen != 1 || d->z.name[0] != 0){
		chat("opendotdot: no . entry...");
		return -1;
	}
	if(l32(d->z.addr) != l32(ip->d.z.addr)){
		chat("opendotdot: bad . address...");
		return -1;
	}
	if(getdrec(f, d) < 0){
		chat("opendotdot: getdrec(..) failed...");
		return -1;
	}
	if(d->z.namelen != 1 || d->z.name[0] != 1){
		chat("opendotdot: no .. entry...");
		return -1;
	}

	pf->xf = f->xf;
	pip->fmt = ip->fmt;
	pip->blksize = ip->blksize;
	pip->offset = 0;
	pip->doffset = 0;
	pip->d = *d;
	return 0;
}

enum {
	Hname = 1,
	Hmode = 2,
};

static int
rzdir(Xfs *fs, Dir *d, int fmt, Drec *dp)
{
	int n, flags, i, j, lj, nl, vers, sysl, mode, l, have;
	uchar *s;
	char *p;
	char buf[Maxname+UTFmax+1];
	uchar *q;
	Rune r;
	enum { ONAMELEN = 28 };	/* old Plan 9 directory name length */

	have = 0;
	flags = 0;
	vers = -1;
	d->qid.path = l32(dp->z.addr);
	d->qid.type = 0;
	d->qid.vers = 0;
	n = dp->z.namelen;
	memset(d->name, 0, Maxname);
	if(n == 1) {
		switch(dp->z.name[0]){
		case 1:
			d->name[1] = '.';
			/* fall through */
		case 0:
			d->name[0] = '.';
			have = Hname;
			break;
		default:
			d->name[0] = tolower(dp->z.name[0]);
		}
	} else {
		if(fmt == 'J'){	/* Joliet, 16-bit Unicode */
			q = (uchar*)dp->z.name;
			for(i=j=lj=0; i<n && j<Maxname; i+=2){
				lj = j;
				r = (q[i]<<8)|q[i+1];
				j += runetochar(buf+j, &r);
			}
			if(j >= Maxname)
				j = lj;
			memmove(d->name, buf, j);
		}else{
			if(n >= Maxname)
				n = Maxname-1;
			for(i=0; i<n; i++)
				d->name[i] = tolower(dp->z.name[i]);
		}
	}

	sysl = dp->z.reclen-(34+dp->z.namelen);
	s = (uchar*)dp->z.name + dp->z.namelen;
	if(((uintptr)s) & 1) {
		s++;
		sysl--;
	}
	if(fs->isplan9 && sysl > 0) {
		/*
		 * get gid, uid, mode and possibly name
		 * from plan9 directory extension
		 */
		nl = *s;
		if(nl >= ONAMELEN)
			nl = ONAMELEN-1;
		if(nl) {
			memset(d->name, 0, ONAMELEN);
			memmove(d->name, s+1, nl);
		}
		s += 1 + *s;
		nl = *s;
		if(nl >= ONAMELEN)
			nl = ONAMELEN-1;
		memset(d->uid, 0, ONAMELEN);
		memmove(d->uid, s+1, nl);
		s += 1 + *s;
		nl = *s;
		if(nl >= ONAMELEN)
			nl = ONAMELEN-1;
		memset(d->gid, 0, ONAMELEN);
		memmove(d->gid, s+1, nl);
		s += 1 + *s;
		if(((uintptr)s) & 1)
			s++;
		d->mode = l32(s);
		if(d->mode & DMDIR)
			d->qid.type |= QTDIR;
	} else {
		d->mode = 0444;
		switch(fmt) {
		case 'z':
			if(fs->isrock)
				strcpy(d->gid, "ridge");
			else
				strcpy(d->gid, "iso9660");
			flags = dp->z.flags;
			break;
		case 'r':
			strcpy(d->gid, "sierra");
			flags = dp->r.flags;
			break;
		case 'J':
			strcpy(d->gid, "joliet");
			flags = dp->z.flags;
			break;
		case '9':
			strcpy(d->gid, "plan9");
			flags = dp->z.flags;
			break;
		}
		if(flags & 0x02){
			d->qid.type |= QTDIR;
			d->mode |= DMDIR|0111;
		}
		strcpy(d->uid, "cdrom");
		if(fmt!='9' && !(d->mode&DMDIR)){
			/*
			 * ISO 9660 actually requires that you always have a . and a ;,
			 * even if there is no version and no extension.  Very few writers
			 * do this.  If the version is present, we use it for qid.vers.
			 * If there is no extension but there is a dot, we strip it off.
			 * (VMS heads couldn't comprehend the dot as a file name character
			 * rather than as just a separator between name and extension.)
			 *
			 * We don't do this for directory names because directories are
			 * not allowed to have extensions and versions.
			 */
			if((p=strchr(d->name, ';')) != nil){
				vers = strtoul(p+1, 0, 0);
				d->qid.vers = vers;
				*p = '\0';
			}
			if((p=strchr(d->name, '.')) != nil && *(p+1)=='\0')
				*p = '\0';
		}
		if(fs->issusp){
			nl = 0;
			s += fs->suspoff;
			sysl -= fs->suspoff;
			for(; sysl >= 4 && have != (Hname|Hmode); sysl -= l, s += l){
				if(s[0] == 0 && ((uintptr)s & 1)){
					/* MacOS pads individual entries, contrary to spec */
					s++;
					sysl--;
				}
				l = s[2];
				if(s[0] == 'P' && s[1] == 'X' && s[3] == 1){
					/* posix file attributes */
					mode = l32(s+4);
					d->mode = mode & 0777;
					if((mode & 0170000) == 040000){
						d->mode |= DMDIR;
						d->qid.type |= QTDIR;
					}
					have |= Hmode;
				} else if(s[0] == 'N' && s[1] == 'M' && s[3] == 1){
					/* alternative name */
					if((s[4] & ~1) == 0){
						i = nl+l-5;
						if(i >= Maxname)
							i = Maxname-1;
						if((i -= nl) > 0){
							memmove(d->name+nl, s+5, i);
							nl += i;
						}
						if(s[4] == 0)
							have |= Hname;
					}
				} else if(s[0] == 'C' && s[1] == 'E' && s[2] >= 28){
					sysl = getcontin(fs->d, s, &s);
					continue;
				} else if(s[0] == 'S' && s[1] == 'T')
					break;
			}
		}
	}
	d->length = 0;
	if((d->mode & DMDIR) == 0)
		d->length = l32(dp->z.size);
	d->type = 0;
	d->dev = 0;
	d->atime = gtime(dp->z.date);
	d->mtime = d->atime;
	return vers;
}

static int
getcontin(Xdata *dev, uchar *p, uchar **s)
{
	long bn, off, len;
	Iobuf *b;

	bn = l32(p+4);
	off = l32(p+12);
	len = l32(p+20);
	chat("getcontin %d...", bn);
	b = getbuf(dev, bn);
	if(b == 0){
		*s = 0;
		return 0;
	}
	*s = b->iobuf+off;
	putbuf(b);
	return len;
}

static char *
nstr(uchar *p, int n)
{
	static char buf[132];
	char *q = buf;

	while(--n >= 0){
		if(*p == '\\')
			*q++ = '\\';
		if(' ' <= *p && *p <= '~')
			*q++ = *p++;
		else
			q += sprint(q, "\\%2.2ux", *p++);
	}
	*q = 0;
	return buf;
}

static char *
rdate(uchar *p, int fmt)
{
	static char buf[64];
	int htz, s, n;

	n = sprint(buf, "%2.2d.%2.2d.%2.2d %2.2d:%2.2d:%2.2d",
		p[0], p[1], p[2], p[3], p[4], p[5]);
	if(fmt == 'z'){
		htz = p[6];
		if(htz >= 128){
			htz = 256-htz;
			s = '-';
		}else
			s = '+';
		sprint(&buf[n], " (%c%.1f)", s, (float)htz/2);
	}
	return buf;
}

static char
dmsize[12] =
{
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
};

#define dysize mydysize

static int
dysize(int y)
{

	if((y%4) == 0)
		return 366;
	return 365;
}

static long
gtime(uchar *p)	/* yMdhmsz */
{
	long t;
	int i, y, M, d, h, m, s, tz;

	y=p[0]; M=p[1]; d=p[2];
	h=p[3]; m=p[4]; s=p[5]; tz=p[6];
	USED(tz);
	y += 1900;
	if (y < 1970)
		return 0;
	if (M < 1 || M > 12)
		return 0;
	if (d < 1 || d > dmsize[M-1])
		if (!(M == 2 && d == 29 && dysize(y) == 366))
			return 0;
	if (h > 23)
		return 0;
	if (m > 59)
		return 0;
	if (s > 59)
		return 0;
	t = 0;
	for(i=1970; i<y; i++)
		t += dysize(i);
	if (dysize(y)==366 && M >= 3)
		t++;
	while(--M)
		t += dmsize[M-1];
	t += d-1;
	t = 24*t + h;
	t = 60*t + m;
	t = 60*t + s;
	return t;
}

#define	p	((uchar*)arg)

static long
l16(void *arg)
{
	long v;

	v = ((long)p[1]<<8)|p[0];
	if (v >= 0x8000L)
		v -= 0x10000L;
	return v;
}

static long
l32(void *arg)
{
	return ((((((long)p[3]<<8)|p[2])<<8)|p[1])<<8)|p[0];
}

#undef	p
