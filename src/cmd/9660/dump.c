#include <u.h>
#include <libc.h>
#include <bio.h>
#include <libsec.h>
#include <ctype.h>
#include "iso9660.h"

static void
md5cd(Cdimg *cd, ulong block, ulong length, uchar *digest)
{
	int n;
	uchar buf[Blocksize];
	DigestState *s;

	s = md5(nil, 0, nil, nil);
	while(length > 0) {
		n = length;
		if(n > Blocksize)
			n = Blocksize;

		Creadblock(cd, buf, block, n);

		md5(buf, n, nil, s);

		block++;
		length -= n;
	}
	md5(nil, 0, digest, s);
}

static Dumpdir*
mkdumpdir(char *name, uchar *md5, ulong block, ulong length)
{
	Dumpdir *d;

	assert(block != 0);

	d = emalloc(sizeof *d);
	d->name = name;
	memmove(d->md5, md5, sizeof d->md5);
	d->block = block;
	d->length = length;

	return d;
}

static Dumpdir**
ltreewalkmd5(Dumpdir **l, uchar *md5)
{
	int i;

	while(*l) {
		i = memcmp(md5, (*l)->md5, MD5dlen);
		if(i < 0)
			l = &(*l)->md5left;
		else if(i == 0)
			return l;
		else
			l = &(*l)->md5right;
	}
	return l;
}

static Dumpdir**
ltreewalkblock(Dumpdir **l, ulong block)
{
	while(*l) {
		if(block < (*l)->block)
			l = &(*l)->blockleft;
		else if(block == (*l)->block)
			return l;
		else
			l = &(*l)->blockright;
	}
	return l;
}

/*
 * Add a particular file to our binary tree.
 */
static void
addfile(Cdimg *cd, Dump *d, char *name, Direc *dir)
{
	uchar md5[MD5dlen];
	Dumpdir **lblock;

	assert((dir->mode & DMDIR) == 0);

	if(dir->length == 0)
		return;

	lblock = ltreewalkblock(&d->blockroot, dir->block);
	if(*lblock != nil) {
		if((*lblock)->length == dir->length)
			return;
		fprint(2, "block %lud length %lud %s %lud %s\n", dir->block, (*lblock)->length, (*lblock)->name,
			dir->length, dir->name);
		assert(0);
	}

	md5cd(cd, dir->block, dir->length, md5);
	if(chatty > 1)
		fprint(2, "note file %.16H %lud (%s)\n", md5, dir->length, dir->name);
	insertmd5(d, name, md5, dir->block, dir->length);
}

void
insertmd5(Dump *d, char *name, uchar *md5, ulong block, ulong length)
{
	Dumpdir **lmd5;
	Dumpdir **lblock;

	lblock = ltreewalkblock(&d->blockroot, block);
	if(*lblock != nil) {
		if((*lblock)->length == length)
			return;
		fprint(2, "block %lud length %lud %lud\n", block, (*lblock)->length, length);
		assert(0);
	}

	assert(length != 0);
	*lblock = mkdumpdir(name, md5, block, length);

	lmd5 = ltreewalkmd5(&d->md5root, md5);
	if(*lmd5 != nil)
		fprint(2, "warning: data duplicated on CD\n");
	else
		*lmd5 = *lblock;
}

/*
 * Fill all the children entries for a particular directory;
 * all we care about is block, length, and whether it is a directory.
 */
void
readkids(Cdimg *cd, Direc *dir, char *(*cvt)(uchar*, int))
{
	char *dot, *dotdot;
	int m, n;
	uchar buf[Blocksize], *ebuf, *p;
	ulong b, nb;
	Cdir *c;
	Direc dx;

	assert(dir->mode & DMDIR);

	dot = atom(".");
	dotdot = atom("..");
	ebuf = buf+Blocksize;
	nb = (dir->length+Blocksize-1) / Blocksize;

	n = 0;
	for(b=0; b<nb; b++) {
		Creadblock(cd, buf, dir->block + b, Blocksize);
		p = buf;
		while(p < ebuf) {
			c = (Cdir*)p;
			if(c->len == 0)
				break;
			if(p+c->len > ebuf)
				break;
			if(parsedir(cd, &dx, p, ebuf-p, cvt) == 0 && dx.name != dot && dx.name != dotdot)
				n++;
			p += c->len;
		}
	}

	m = (n+Ndirblock-1)/Ndirblock * Ndirblock;
	dir->child = emalloc(m*sizeof dir->child[0]);
	dir->nchild = n;

	n = 0;
	for(b=0; b<nb; b++) {
		assert(n <= dir->nchild);
		Creadblock(cd, buf, dir->block + b, Blocksize);
		p = buf;
		while(p < ebuf) {
			c = (Cdir*)p;
			if(c->len == 0)
				break;
			if(p+c->len > ebuf)
				break;
			if(parsedir(cd, &dx, p, ebuf-p, cvt) == 0 && dx.name != dot && dx.name != dotdot) {
				assert(n < dir->nchild);
				dir->child[n++] = dx;
			}
			p += c->len;
		}
	}
}

/*
 * Free the children.  Make sure their children are free too.
 */
void
freekids(Direc *dir)
{
	int i;

	for(i=0; i<dir->nchild; i++)
		assert(dir->child[i].nchild == 0);

	free(dir->child);
	dir->child = nil;
	dir->nchild = 0;
}

/*
 * Add a whole directory and all its children to our binary tree.
 */
static void
adddir(Cdimg *cd, Dump *d, Direc *dir)
{
	int i;

	readkids(cd, dir, isostring);
	for(i=0; i<dir->nchild; i++) {
		if(dir->child[i].mode & DMDIR)
			adddir(cd, d, &dir->child[i]);
		else
			addfile(cd, d, atom(dir->name), &dir->child[i]);
	}
	freekids(dir);
}

Dumpdir*
lookupmd5(Dump *d, uchar *md5)
{
	return *ltreewalkmd5(&d->md5root, md5);
}

void
adddirx(Cdimg *cd, Dump *d, Direc *dir, int lev)
{
	int i;
	Direc dd;

	if(lev == 2){
		dd = *dir;
		adddir(cd, d, &dd);
		return;
	}
	for(i=0; i<dir->nchild; i++)
		adddirx(cd, d, &dir->child[i], lev+1);
}

Dump*
dumpcd(Cdimg *cd, Direc *dir)
{
	Dump *d;

	d = emalloc(sizeof *d);
	d->cd = cd;
	adddirx(cd, d, dir, 0);
	return d;
}

/*
static ulong
minblock(Direc *root, int lev)
{
	int i;
	ulong m, n;

	m = root->block;
	for(i=0; i<root->nchild; i++) {
		n = minblock(&root->child[i], lev-1);
		if(m > n)
			m = n;
	}
	return m;
}
*/

void
copybutname(Direc *d, Direc *s)
{
	Direc x;

	x = *d;
	*d = *s;
	d->name = x.name;
	d->confname = x.confname;
}

Direc*
createdumpdir(Direc *root, XDir *dir, char *utfname)
{
	char *p;
	Direc *d;

	if(utfname[0]=='/')
		sysfatal("bad dump name '%s'", utfname);
	p = strchr(utfname, '/');
	if(p == nil || strchr(p+1, '/'))
		sysfatal("bad dump name '%s'", utfname);
	*p++ = '\0';
	if((d = walkdirec(root, utfname)) == nil)
		d = adddirec(root, utfname, dir);
	if(walkdirec(d, p))
		sysfatal("duplicate dump name '%s/%s'", utfname, p);
	d = adddirec(d, p, dir);
	return d;
}

static void
rmdirec(Direc *d, Direc *kid)
{
	Direc *ekid;

	ekid = d->child+d->nchild;
	assert(d->child <= kid && kid < ekid);
	if(ekid != kid+1)
		memmove(kid, kid+1, (ekid-(kid+1))*sizeof(*kid));
	d->nchild--;
}

void
rmdumpdir(Direc *root, char *utfname)
{
	char *p;
	Direc *d, *dd;

	if(utfname[0]=='/')
		sysfatal("bad dump name '%s'", utfname);
	p = strchr(utfname, '/');
	if(p == nil || strchr(p+1, '/'))
		sysfatal("bad dump name '%s'", utfname);
	*p++ = '\0';
	if((d = walkdirec(root, utfname)) == nil)
		sysfatal("cannot remove %s/%s: %s does not exist", utfname, p, utfname);
	p[-1] = '/';

	if((dd = walkdirec(d, p)) == nil)
		sysfatal("cannot remove %s: does not exist", utfname);

	rmdirec(d, dd);
	if(d->nchild == 0)
		rmdirec(root, d);
}

char*
adddumpdir(Direc *root, ulong now, XDir *dir)
{
	char buf[40], *p;
	int n;
	Direc *dday, *dyear;
	Tm tm;

	tm = *localtime(now);

	sprint(buf, "%d", tm.year+1900);
	if((dyear = walkdirec(root, buf)) == nil) {
		dyear = adddirec(root, buf, dir);
		assert(dyear != nil);
	}

	n = 0;
	sprint(buf, "%.2d%.2d", tm.mon+1, tm.mday);
	p = buf+strlen(buf);
	while(walkdirec(dyear, buf))
		sprint(p, "%d", ++n);

	dday = adddirec(dyear, buf, dir);
	assert(dday != nil);

	sprint(buf, "%s/%s", dyear->name, dday->name);
assert(walkdirec(root, buf)==dday);
	return atom(buf);
}

/*
 * The dump directory tree is inferred from a linked list of special blocks.
 * One block is written at the end of each dump.
 * The blocks have the form
 *
 * plan 9 dump cd
 * <dump-name> <dump-time> <next-block> <conform-block> <conform-length> \
 *	<iroot-block> <iroot-length> <jroot-block> <jroot-length>
 *
 * If only the first line is present, this is the end of the chain.
 */
static char magic[] = "plan 9 dump cd\n";
ulong
Cputdumpblock(Cdimg *cd)
{
	ulong x;

	Cwseek(cd, cd->nextblock*Blocksize);
	x = Cwoffset(cd);
	Cwrite(cd, magic, sizeof(magic)-1);
	Cpadblock(cd);
	return x/Blocksize;
}

int
hasdump(Cdimg *cd)
{
	int i;
	char buf[128];

	for(i=16; i<24; i++) {
		Creadblock(cd, buf, i, sizeof buf);
		if(memcmp(buf, magic, sizeof(magic)-1) == 0)
			return i;
	}
	return 0;
}

Direc
readdumpdirs(Cdimg *cd, XDir *dir, char *(*cvt)(uchar*, int))
{
	char buf[Blocksize];
	char *p, *q, *f[16];
	int i, nf;
	ulong db, t;
	Direc *nr, root;
	XDir xd;

	mkdirec(&root, dir);
	db = hasdump(cd);
	xd = *dir;
	for(;;){
		if(db == 0)
			sysfatal("error in dump blocks");

		Creadblock(cd, buf, db, sizeof buf);
		if(memcmp(buf, magic, sizeof(magic)-1) != 0)
			break;
		p = buf+sizeof(magic)-1;
		if(p[0] == '\0')
			break;
		if((q = strchr(p, '\n')) != nil)
			*q = '\0';

		nf = tokenize(p, f, nelem(f));
		i = 5;
		if(nf < i || (cvt==jolietstring && nf < i+2))
			sysfatal("error in dump block %lud: nf=%d; p='%s'", db, nf, p);
		nr = createdumpdir(&root, &xd, f[0]);
		t = strtoul(f[1], 0, 0);
		xd.mtime = xd.ctime = xd.atime = t;
		db = strtoul(f[2], 0, 0);
		if(cvt == jolietstring)
			i += 2;
		nr->block = strtoul(f[i], 0, 0);
		nr->length = strtoul(f[i+1], 0, 0);
	}
	cd->nulldump = db;
	return root;
}

extern void addtx(char*, char*);

static int
isalldigit(char *s)
{
	while(*s)
		if(!isdigit((uchar)*s++))
			return 0;
	return 1;
}

void
readdumpconform(Cdimg *cd)
{
	char buf[Blocksize];
	char *p, *q, *f[10];
	ulong cb, nc, m, db;
	int nf;

	db = hasdump(cd);
	assert(map==nil || map->nt == 0);

	for(;;){
		if(db == 0)
			sysfatal("error0 in dump blocks");

		Creadblock(cd, buf, db, sizeof buf);
		if(memcmp(buf, magic, sizeof(magic)-1) != 0)
			break;
		p = buf+sizeof(magic)-1;
		if(p[0] == '\0')
			break;
		if((q = strchr(p, '\n')) != nil)
			*q = '\0';

		nf = tokenize(p, f, nelem(f));
		if(nf < 5)
			sysfatal("error0 in dump block %lud", db);

		db = strtoul(f[2], 0, 0);
		cb = strtoul(f[3], 0, 0);
		nc = strtoul(f[4], 0, 0);

		Crseek(cd, cb*Blocksize);
		m = cb*Blocksize+nc;
		while(Croffset(cd) < m && (p = Crdline(cd, '\n')) != nil){
			p[Clinelen(cd)-1] = '\0';
			if(tokenize(p, f, 2) != 2 || (f[0][0] != 'D' && f[0][0] != 'F')
			|| strlen(f[0]) != 7 || !isalldigit(f[0]+1))
				break;

			addtx(atom(f[1]), atom(f[0]));
		}
	}
	if(map)
		cd->nconform = map->nt;
	else
		cd->nconform = 0;
}
