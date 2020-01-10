#include <u.h>
#include <libc.h>
#include <bio.h>
#include <libsec.h>

#include "iso9660.h"

static int readisodesc(Cdimg*, Voldesc*);
static int readjolietdesc(Cdimg*, Voldesc*);

/*
 * It's not strictly conforming; instead it's enough to
 * get us up and running; presumably the real CD writing
 * will take care of being conforming.
 *
 * Things not conforming include:
 *	- no path table
 *	- root directories are of length zero
 */
Cdimg*
createcd(char *file, Cdinfo info)
{
	int fd, xfd;
	Cdimg *cd;

	if(access(file, AEXIST) == 0){
		werrstr("file already exists");
		return nil;
	}

	if((fd = create(file, ORDWR, 0666)) < 0)
		return nil;

	cd = emalloc(sizeof *cd);
	cd->file = atom(file);

	Binit(&cd->brd, fd, OREAD);

	if((xfd = open(file, ORDWR)) < 0)
		sysfatal("can't open file again: %r");
	Binit(&cd->bwr, xfd, OWRITE);

	Crepeat(cd, 0, 16*Blocksize);
	Cputisopvd(cd, info);
	if(info.flags & CDbootable){
		cd->bootimage = info.bootimage;
		cd->flags |= CDbootable;
		Cputbootvol(cd);
	}

	if(readisodesc(cd, &cd->iso) < 0)
		assert(0);
	if(info.flags & CDplan9)
		cd->flags |= CDplan9;
	else if(info.flags & CDrockridge)
		cd->flags |= CDrockridge;
	if(info.flags & CDjoliet) {
		Cputjolietsvd(cd, info);
		if(readjolietdesc(cd, &cd->joliet) < 0)
			assert(0);
		cd->flags |= CDjoliet;
	}
	Cputendvd(cd);

	if(info.flags & CDdump){
		cd->nulldump = Cputdumpblock(cd);
		cd->flags |= CDdump;
	}
	if(cd->flags & CDbootable){
		Cputbootcat(cd);
		Cupdatebootvol(cd);
	}

	if(info.flags & CDconform)
		cd->flags |= CDconform;

	cd->flags |= CDnew;
	cd->nextblock = Cwoffset(cd) / Blocksize;
	assert(cd->nextblock != 0);

	return cd;
}

Cdimg*
opencd(char *file, Cdinfo info)
{
	int fd, xfd;
	Cdimg *cd;
	Dir *d;

	if((fd = open(file, ORDWR)) < 0) {
		if(access(file, AEXIST) == 0)
			return nil;
		return createcd(file, info);
	}

	if((d = dirfstat(fd)) == nil) {
		close(fd);
		return nil;
	}
	if(d->length == 0 || d->length % Blocksize) {
		werrstr("bad length %lld", d->length);
		close(fd);
		free(d);
		return nil;
	}

	cd = emalloc(sizeof *cd);
	cd->file = atom(file);
	cd->nextblock = d->length / Blocksize;
	assert(cd->nextblock != 0);
	free(d);

	Binit(&cd->brd, fd, OREAD);

	if((xfd = open(file, ORDWR)) < 0)
		sysfatal("can't open file again: %r");
	Binit(&cd->bwr, xfd, OWRITE);

	if(readisodesc(cd, &cd->iso) < 0) {
		free(cd);
		close(fd);
		close(xfd);
		return nil;
	}

	/* lowercase because of isostring */
	if(strstr(cd->iso.systemid, "iso9660") == nil
	&& strstr(cd->iso.systemid, "utf8") == nil) {
		werrstr("unknown systemid %s", cd->iso.systemid);
		free(cd);
		close(fd);
		close(xfd);
		return nil;
	}

	if(strstr(cd->iso.systemid, "plan 9"))
		cd->flags |= CDplan9;
	if(strstr(cd->iso.systemid, "iso9660"))
		cd->flags |= CDconform;
	if(strstr(cd->iso.systemid, "rrip"))
		cd->flags |= CDrockridge;
	if(strstr(cd->iso.systemid, "boot"))
		cd->flags |= CDbootable;
	if(readjolietdesc(cd, &cd->joliet) == 0)
		cd->flags |= CDjoliet;
	if(hasdump(cd))
		cd->flags |= CDdump;

	return cd;
}

ulong
big(void *a, int n)
{
	uchar *p;
	ulong v;
	int i;

	p = a;
	v = 0;
	for(i=0; i<n; i++)
		v = (v<<8) | *p++;
	return v;
}

ulong
little(void *a, int n)
{
	uchar *p;
	ulong v;
	int i;

	p = a;
	v = 0;
	for(i=0; i<n; i++)
		v |= (*p++<<(i*8));
	return v;
}

void
Creadblock(Cdimg *cd, void *buf, ulong block, ulong len)
{
	assert(block != 0);	/* nothing useful there */

	Bflush(&cd->bwr);
	if(Bseek(&cd->brd, block*Blocksize, 0) != block*Blocksize)
		sysfatal("error seeking to block %lud", block);
	if(Bread(&cd->brd, buf, len) != len)
		sysfatal("error reading %lud bytes at block %lud: %r %lld", len, block, Bseek(&cd->brd, 0, 2));
}

int
parsedir(Cdimg *cd, Direc *d, uchar *buf, int len, char *(*cvtname)(uchar*, int))
{
	enum { NAMELEN = 28 };
	char name[NAMELEN];
	uchar *p;
	Cdir *c;

	memset(d, 0, sizeof *d);

	c = (Cdir*)buf;

	if(c->len > len) {
		werrstr("buffer too small");
		return -1;
	}

	if(c->namelen == 1 && c->name[0] == '\0')
		d->name = atom(".");
	else if(c->namelen == 1 && c->name[0] == '\001')
		d->name = atom("..");
	else if(cvtname)
		d->name = cvtname(c->name, c->namelen);

	d->block = little(c->dloc, 4);
	d->length = little(c->dlen, 4);

	if(c->flags & 2)
		d->mode |= DMDIR;

/*BUG: do we really need to parse the plan 9 fields? */
	/* plan 9 use fields */
	if((cd->flags & CDplan9) && cvtname == isostring
	&& (c->namelen != 1 || c->name[0] > 1)) {
		p = buf+33+c->namelen;
		if((p-buf)&1)
			p++;
		assert(p < buf+c->len);
		assert(*p < NAMELEN);
		if(*p != 0) {
			memmove(name, p+1, *p);
			name[*p] = '\0';
			d->confname = d->name;
			d->name = atom(name);
		}
		p += *p+1;
		assert(*p < NAMELEN);
		memmove(name, p+1, *p);
		name[*p] = '\0';
		d->uid = atom(name);
		p += *p+1;
		assert(*p < NAMELEN);
		memmove(name, p+1, *p);
		name[*p] = '\0';
		d->gid = atom(name);
		p += *p+1;
		if((p-buf)&1)
			p++;
		d->mode = little(p, 4);
	}

	/* BUG: rock ridge extensions */
	return 0;
}

void
setroot(Cdimg *cd, ulong block, ulong dloc, ulong dlen)
{
	assert(block != 0);

	Cwseek(cd, block*Blocksize+offsetof(Cvoldesc, rootdir[0])+offsetof(Cdir, dloc[0]));
	Cputn(cd, dloc, 4);
	Cputn(cd, dlen, 4);
}

void
setvolsize(Cdimg *cd, ulong block, ulong size)
{
	assert(block != 0);

	Cwseek(cd, block*Blocksize+offsetof(Cvoldesc, volsize[0]));
	Cputn(cd, size, 4);
}

void
setpathtable(Cdimg *cd, ulong block, ulong sz, ulong lloc, ulong bloc)
{
	assert(block != 0);

	Cwseek(cd, block*Blocksize+offsetof(Cvoldesc, pathsize[0]));
	Cputn(cd, sz, 4);
	Cputnl(cd, lloc, 4);
	Cputnl(cd, 0, 4);
	Cputnm(cd, bloc, 4);
	Cputnm(cd, 0, 4);
	assert(Cwoffset(cd) == block*Blocksize+offsetof(Cvoldesc, rootdir[0]));
}


static void
parsedesc(Voldesc *v, Cvoldesc *cv, char *(*string)(uchar*, int))
{
	v->systemid = string(cv->systemid, sizeof cv->systemid);

	v->pathsize = little(cv->pathsize, 4);
	v->lpathloc = little(cv->lpathloc, 4);
	v->mpathloc = little(cv->mpathloc, 4);

	v->volumeset = string(cv->volumeset, sizeof cv->volumeset);
	v->publisher = string(cv->publisher, sizeof cv->publisher);
	v->preparer = string(cv->preparer, sizeof cv->preparer);
	v->application = string(cv->application, sizeof cv->application);

	v->abstract = string(cv->abstract, sizeof cv->abstract);
	v->biblio = string(cv->biblio, sizeof cv->biblio);
	v->notice = string(cv->notice, sizeof cv->notice);
}

static int
readisodesc(Cdimg *cd, Voldesc *v)
{
	static uchar magic[] = { 0x01, 'C', 'D', '0', '0', '1', 0x01, 0x00 };
	Cvoldesc cv;

	memset(v, 0, sizeof *v);

	Creadblock(cd, &cv, 16, sizeof cv);
	if(memcmp(cv.magic, magic, sizeof magic) != 0) {
		werrstr("bad pvd magic");
		return -1;
	}

	if(little(cv.blocksize, 2) != Blocksize) {
		werrstr("block size not %d", Blocksize);
		return -1;
	}

	cd->iso9660pvd = 16;
	parsedesc(v, &cv, isostring);

	return parsedir(cd, &v->root, cv.rootdir, sizeof cv.rootdir, isostring);
}

static int
readjolietdesc(Cdimg *cd, Voldesc *v)
{
	int i;
	static uchar magic[] = { 0x02, 'C', 'D', '0', '0', '1', 0x01, 0x00 };
	Cvoldesc cv;

	memset(v, 0, sizeof *v);

	for(i=16; i<24; i++) {
		Creadblock(cd, &cv, i, sizeof cv);
		if(memcmp(cv.magic, magic, sizeof magic) != 0)
			continue;
		if(cv.charset[0] != 0x25 || cv.charset[1] != 0x2F
		|| (cv.charset[2] != 0x40 && cv.charset[2] != 0x43 && cv.charset[2] != 0x45))
			continue;
		break;
	}

	if(i==24) {
		werrstr("could not find Joliet SVD");
		return -1;
	}

	if(little(cv.blocksize, 2) != Blocksize) {
		werrstr("block size not %d", Blocksize);
		return -1;
	}

	cd->jolietsvd = i;
	parsedesc(v, &cv, jolietstring);

	return parsedir(cd, &v->root, cv.rootdir, sizeof cv.rootdir, jolietstring);
}

/*
 * CD image buffering routines.
 */
void
Cputc(Cdimg *cd, int c)
{
	assert(Boffset(&cd->bwr) >= 16*Blocksize || c == 0);

if(Boffset(&cd->bwr) == 0x9962)
if(c >= 256) abort();
	if(Bputc(&cd->bwr, c) < 0)
		sysfatal("Bputc: %r");
	Bflush(&cd->brd);
}

void
Cputnl(Cdimg *cd, ulong val, int size)
{
	switch(size) {
	default:
		sysfatal("bad size %d in bputnl", size);
	case 2:
		Cputc(cd, val);
		Cputc(cd, val>>8);
		break;
	case 4:
		Cputc(cd, val);
		Cputc(cd, val>>8);
		Cputc(cd, val>>16);
		Cputc(cd, val>>24);
		break;
	}
}

void
Cputnm(Cdimg *cd, ulong val, int size)
{
	switch(size) {
	default:
		sysfatal("bad size %d in bputnl", size);
	case 2:
		Cputc(cd, val>>8);
		Cputc(cd, val);
		break;
	case 4:
		Cputc(cd, val>>24);
		Cputc(cd, val>>16);
		Cputc(cd, val>>8);
		Cputc(cd, val);
		break;
	}
}

void
Cputn(Cdimg *cd, long val, int size)
{
	Cputnl(cd, val, size);
	Cputnm(cd, val, size);
}

/*
 * ASCII/UTF string writing
 */
void
Crepeat(Cdimg *cd, int c, int n)
{
	while(n-- > 0)
		Cputc(cd, c);
}

void
Cputs(Cdimg *cd, char *s, int size)
{
	int n;

	if(s == nil) {
		Crepeat(cd, ' ', size);
		return;
	}

	for(n=0; n<size && *s; n++)
		Cputc(cd, *s++);
	if(n<size)
		Crepeat(cd, ' ', size-n);
}

void
Cwrite(Cdimg *cd, void *buf, int n)
{
	assert(Boffset(&cd->bwr) >= 16*Blocksize);

	if(Bwrite(&cd->bwr, buf, n) != n)
		sysfatal("Bwrite: %r");
	Bflush(&cd->brd);
}

void
Cputr(Cdimg *cd, Rune r)
{
	Cputc(cd, r>>8);
	Cputc(cd, r);
}

void
Crepeatr(Cdimg *cd, Rune r, int n)
{
	int i;

	for(i=0; i<n; i++)
		Cputr(cd, r);
}

void
Cputrs(Cdimg *cd, Rune *s, int osize)
{
	int n, size;

	size = osize/2;
	if(s == nil)
		Crepeatr(cd, (Rune)' ', size);
	else {
		for(n=0; *s && n<size; n++)
			Cputr(cd, *s++);
		if(n<size)
			Crepeatr(cd, ' ', size-n);
	}
	if(osize&1)
		Cputc(cd, 0);	/* what else can we do? */
}

void
Cputrscvt(Cdimg *cd, char *s, int size)
{
	Rune r[256];

	strtorune(r, s);
	Cputrs(cd, strtorune(r, s), size);
}

void
Cpadblock(Cdimg *cd)
{
	int n;
	ulong nb;

	n = Blocksize - (Boffset(&cd->bwr) % Blocksize);
	if(n != Blocksize)
		Crepeat(cd, 0, n);

	nb = Boffset(&cd->bwr)/Blocksize;
	assert(nb != 0);
	if(nb > cd->nextblock)
		cd->nextblock = nb;
}

void
Cputdate(Cdimg *cd, ulong ust)
{
	Tm *tm;

	if(ust == 0) {
		Crepeat(cd, 0, 7);
		return;
	}
	tm = gmtime(ust);
	Cputc(cd, tm->year);
	Cputc(cd, tm->mon+1);
	Cputc(cd, tm->mday);
	Cputc(cd, tm->hour);
	Cputc(cd, tm->min);
	Cputc(cd, tm->sec);
	Cputc(cd, 0);
}

void
Cputdate1(Cdimg *cd, ulong ust)
{
	Tm *tm;
	char str[20];

	if(ust == 0) {
		Crepeat(cd, '0', 16);
		Cputc(cd, 0);
		return;
	}
	tm = gmtime(ust);
	sprint(str, "%.4d%.2d%.2d%.2d%.2d%.4d",
		tm->year+1900,
		tm->mon+1,
		tm->mday,
		tm->hour,
		tm->min,
		tm->sec*100);
	Cputs(cd, str, 16);
	Cputc(cd, 0);
}

void
Cwseek(Cdimg *cd, ulong offset)
{
	Bseek(&cd->bwr, offset, 0);
}

ulong
Cwoffset(Cdimg *cd)
{
	return Boffset(&cd->bwr);
}

void
Cwflush(Cdimg *cd)
{
	Bflush(&cd->bwr);
}

ulong
Croffset(Cdimg *cd)
{
	return Boffset(&cd->brd);
}

void
Crseek(Cdimg *cd, ulong offset)
{
	Bseek(&cd->brd, offset, 0);
}

int
Cgetc(Cdimg *cd)
{
	int c;

	Cwflush(cd);
	if((c = Bgetc(&cd->brd)) == Beof) {
		fprint(2, "getc at %lud\n", Croffset(cd));
		assert(0);
		/*sysfatal("Bgetc: %r"); */
	}
	return c;
}

void
Cread(Cdimg *cd, void *buf, int n)
{
	Cwflush(cd);
	if(Bread(&cd->brd, buf, n) != n)
		sysfatal("Bread: %r");
}

char*
Crdline(Cdimg *cd, int c)
{
	Cwflush(cd);
	return Brdline(&cd->brd, c);
}

int
Clinelen(Cdimg *cd)
{
	return Blinelen(&cd->brd);
}
