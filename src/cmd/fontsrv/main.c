#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <thread.h>
#include <fcall.h>
#include <9p.h>
/*
 * we included thread.h in order to include 9p.h,
 * but we don't use threads, so exits is ok.
 */
#undef exits

#include "a.h"

Memsubfont *defont;

void
usage(void)
{
	fprint(2, "usage: fontsrv [-m mtpt]\n");
	fprint(2, "or fontsrv -p path\n");
	exits("usage");
}

static
void
packinfo(Fontchar *fc, uchar *p, int n)
{
	int j;

	for(j=0;  j<=n;  j++){
		p[0] = fc->x;
		p[1] = fc->x>>8;
		p[2] = fc->top;
		p[3] = fc->bottom;
		p[4] = fc->left;
		p[5] = fc->width;
		fc++;
		p += 6;
	}
}

enum
{
	Qroot = 0,
	Qfontdir,
	Qsizedir,
	Qfontfile,
	Qsubfontfile,
};

#define QTYPE(p) ((p) & 0xF)
#define QFONT(p) (((p) >> 4) & 0xFFFF)
#define QSIZE(p) (((p) >> 20) & 0xFF)
#define QANTIALIAS(p) (((p) >> 28) & 0x1)
#define QRANGE(p) (((p) >> 29) & 0xFF)
static int sizes[] = { 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 24, 28 };

static vlong
qpath(int type, int font, int size, int antialias, int range)
{
	return type | (font << 4) | (size << 20) | (antialias << 28) | ((vlong)range << 29);
}

static void
dostat(vlong path, Qid *qid, Dir *dir)
{
	char *name;
	Qid q;
	ulong mode;
	vlong length;
	XFont *f;
	char buf[100];
	
	q.type = 0;
	q.vers = 0;
	q.path = path;
	mode = 0444;
	length = 0;
	name = "???";

	switch(QTYPE(path)) {
	default:
		sysfatal("dostat %#llux", path);

	case Qroot:
		q.type = QTDIR;
		name = "/";
		break;

	case Qfontdir:
		q.type = QTDIR;
		f = &xfont[QFONT(path)];
		name = f->name;
		break;

	case Qsizedir:
		q.type = QTDIR;
		snprint(buf, sizeof buf, "%lld%s", QSIZE(path), QANTIALIAS(path) ? "a" : "");
		name = buf;
		break;
	
	case Qfontfile:
		f = &xfont[QFONT(path)];
		load(f);
		length = 11+1+11+1+f->nrange*(6+1+6+1+9+1);
		name = "font";
		break;

	case Qsubfontfile:
		snprint(buf, sizeof buf, "x%02llx00.bit", QRANGE(path));
		name = buf;
		break;
	}
	
	if(qid)
		*qid = q;
	if(dir) {
		memset(dir, 0, sizeof *dir);
		dir->name = estrdup9p(name);
		dir->muid = estrdup9p("");
		dir->uid = estrdup9p("font");
		dir->gid = estrdup9p("font");
		dir->qid = q;
		if(q.type == QTDIR)
			mode |= DMDIR | 0111;
		dir->mode = mode;
		dir->length = length;
	}
}

static char*
xwalk1(Fid *fid, char *name, Qid *qid)
{
	int i, dotdot;
	vlong path;
	char *p;
	int a, n;
	XFont *f;

	path = fid->qid.path;
	dotdot = strcmp(name, "..") == 0;
	switch(QTYPE(path)) {
	default:
	NotFound:
		return "file not found";

	case Qroot:
		if(dotdot)
			break;
		for(i=0; i<nxfont; i++) {
			if(strcmp(xfont[i].name, name) == 0) {
				path = qpath(Qfontdir, i, 0, 0, 0);
				goto Found;
			}
		}
		goto NotFound;

	case Qfontdir:
		if(dotdot) {
			path = Qroot;
			break;
		}
		n = strtol(name, &p, 10);
		if(n == 0)
			goto NotFound;
		a = 0;
		if(*p == 'a') {
			a = 1;
			p++;
		}
		if(*p != 0)
			goto NotFound;
		path += Qsizedir - Qfontdir + qpath(0, 0, n, a, 0);
		break;

	case Qsizedir:
		if(dotdot) {
			path = qpath(Qfontdir, QFONT(path), 0, 0, 0);
			break;
		}
		if(strcmp(name, "font") == 0) {
			path += Qfontfile - Qsizedir;
			break;
		}
		f = &xfont[QFONT(path)];
		load(f);
		p = name;
		if(*p != 'x')
			goto NotFound;
		p++;
		n = strtoul(p, &p, 16);
		if(p != name+5 || (n&0xFF) != 0 || strcmp(p, ".bit") != 0 || !f->range[(n>>8) & 0xFF])
			goto NotFound;
		path += Qsubfontfile - Qsizedir + qpath(0, 0, 0, 0, (n>>8) & 0xFF);
		break;
	}
Found:
	dostat(path, qid, nil);
	fid->qid = *qid;
	return nil;
}

static int
rootgen(int i, Dir *d, void *v)
{
	if(i >= nxfont)
		return -1;
	dostat(qpath(Qfontdir, i, 0, 0, 0), nil, d);
	return 0;
}

static int
fontgen(int i, Dir *d, void *v)
{
	vlong path;
	Fid *f;
	
	f = v;
	path = f->qid.path;
	if(i >= 2*nelem(sizes))
		return -1;
	dostat(qpath(Qsizedir, QFONT(path), sizes[i/2], i&1, 0), nil, d);
	return 0;
}

static int
sizegen(int i, Dir *d, void *v)
{
	vlong path;
	Fid *fid;
	XFont *f;
	int j;

	fid = v;
	path = fid->qid.path;
	if(i == 0) {
		path += Qfontfile - Qsizedir;
		goto Done;
	}
	i--;
	f = &xfont[QFONT(path)];
	load(f);
	for(j=0; j<nelem(f->range); j++) {
		if(f->range[j] == 0)
			continue;
		if(i == 0) {
			path += Qsubfontfile - Qsizedir;
			path += qpath(0, 0, 0, 0, j);
			goto Done;
		}
		i--;
	}
	return -1;

Done:
	dostat(path, nil, d);
	return 0;
}

static void
xattach(Req *r)
{
	dostat(0, &r->ofcall.qid, nil);
	r->fid->qid = r->ofcall.qid;
	respond(r, nil);
}

static void
xopen(Req *r)
{
	if(r->ifcall.mode != OREAD) {
		respond(r, "permission denied");
		return;
	}
	r->ofcall.qid = r->fid->qid;
	respond(r, nil);
}

void
responderrstr(Req *r)
{
	char err[ERRMAX];
	
	rerrstr(err, sizeof err);
	respond(r, err);
}

static void
xread(Req *r)
{
	int i, size, height, ascent;
	vlong path;
	Fmt fmt;
	XFont *f;
	char *data;
	Memsubfont *sf;
	Memimage *m;
	
	path = r->fid->qid.path;
	switch(QTYPE(path)) {
	case Qroot:
		dirread9p(r, rootgen, nil);
		break;
	case Qfontdir:
		dirread9p(r, fontgen, r->fid);
		break;
	case Qsizedir:
		dirread9p(r, sizegen, r->fid);
		break;
	case Qfontfile:
		fmtstrinit(&fmt);
		f = &xfont[QFONT(path)];
		load(f);
		if(f->unit == 0 && f->loadheight == nil) {
			readstr(r, "font missing\n");
			break;
		}
		height = 0;
		ascent = 0;
		if(f->unit > 0) {
			height = f->height * (int)QSIZE(path)/f->unit + 0.99999999;
			ascent = height - (int)(-f->originy * (int)QSIZE(path)/f->unit + 0.99999999);
		}
		if(f->loadheight != nil)
			f->loadheight(f, QSIZE(path), &height, &ascent);
		fmtprint(&fmt, "%11d %11d\n", height, ascent);
		for(i=0; i<nelem(f->range); i++) {
			if(f->range[i] == 0)
				continue;
			fmtprint(&fmt, "0x%04x 0x%04x x%04x.bit\n", i<<8, (i<<8) + 0xFF, i<<8);
		}
		data = fmtstrflush(&fmt);
		readstr(r, data);
		free(data);
		break;
	case Qsubfontfile:
		f = &xfont[QFONT(path)];
		load(f);
		if(r->fid->aux == nil) {
			r->fid->aux = mksubfont(f, f->name, QRANGE(path)<<8, (QRANGE(path)<<8)+0xFF, QSIZE(path), QANTIALIAS(path));
			if(r->fid->aux == nil) {
				responderrstr(r);
				return;
			}
		}
		sf = r->fid->aux;
		m = sf->bits;
		if(r->ifcall.offset < 5*12) {
			char *chan;
			if(QANTIALIAS(path))
				chan = "k8";
			else
				chan = "k1";
			data = smprint("%11s %11d %11d %11d %11d ", chan, m->r.min.x, m->r.min.y, m->r.max.x, m->r.max.y);
			readstr(r, data);
			free(data);
			break;
		}
		r->ifcall.offset -= 5*12;
		size = bytesperline(m->r, chantodepth(m->chan)) * Dy(m->r);
		if(r->ifcall.offset < size) {
			readbuf(r, byteaddr(m, m->r.min), size);
			break;
		}
		r->ifcall.offset -= size;
		data = emalloc9p(3*12+6*(sf->n+1));
		sprint(data, "%11d %11d %11d ", sf->n, sf->height, sf->ascent);
		packinfo(sf->info, (uchar*)data+3*12, sf->n);
		readbuf(r, data, 3*12+6*(sf->n+1));
		free(data);
		break;
	}
	respond(r, nil);
}

static void
xdestroyfid(Fid *fid)
{
	Memsubfont *sf;
	
	sf = fid->aux;
	if(sf == nil)
		return;

	freememimage(sf->bits);
	free(sf->info);
	free(sf);
	fid->aux = nil;
}

static void
xstat(Req *r)
{
	dostat(r->fid->qid.path, nil, &r->d);
	respond(r, nil);
}

Srv xsrv;

int
proccreate(void (*f)(void*), void *a, unsigned i)
{
	abort();
}

int pflag;

static long dirpackage(uchar*, long, Dir**);

void
dump(char *path)
{
	char *elem, *p, *path0, *err;
	uchar buf[4096];
	Fid fid;
	Qid qid;
	Dir *d;
	Req r;
	int off, i, n;

	// root
	memset(&fid, 0, sizeof fid);
	dostat(0, &fid.qid, nil);	
	qid = fid.qid;

	path0 = path;
	while(path != nil) {
		p = strchr(path, '/');
		if(p != nil)
			*p = '\0';
		elem = path;
		if(strcmp(elem, "") != 0 && strcmp(elem, ".") != 0) {
			err = xwalk1(&fid, elem, &qid);
			if(err != nil) {
				fprint(2, "%s: %s\n", path0, err);
				exits(err);
			}
		}
		if(p)
			*p++ = '/';
		path = p;
	}
	
	memset(&r, 0, sizeof r);
	xsrv.fake = 1;

	// read and display
	off = 0;
	for(;;) {
		r.srv = &xsrv;
		r.fid = &fid;
		r.ifcall.type = Tread;
		r.ifcall.count = sizeof buf;
		r.ifcall.offset = off;
		r.ofcall.data = (char*)buf;
		r.ofcall.count = 0;
		xread(&r);
		if(r.ofcall.type != Rread) {
			fprint(2, "reading %s: %s\n", path0, r.ofcall.ename);
			exits(r.ofcall.ename);
		}
		n = r.ofcall.count;
		if(n == 0)
			break;
		if(off == 0 && pflag > 1) {
			print("\001");
		}
		off += n;
		if(qid.type & QTDIR) {
			n = dirpackage(buf, n, &d);
			for(i=0; i<n; i++)
				print("%s%s\n", d[i].name, (d[i].mode&DMDIR) ? "/" : "");
			free(d);
		} else
			write(1, buf, n);
	}
}

int
fontcmp(const void *va, const void *vb)
{
	XFont *a, *b;

	a = (XFont*)va;
	b = (XFont*)vb;
	return strcmp(a->name, b->name);
}

void
main(int argc, char **argv)
{
	char *mtpt, *srvname;

	mtpt = nil;
	srvname = "font";

	ARGBEGIN{
	case 'D':
		chatty9p++;
		break;
	case 'F':
		chattyfuse++;
		break;
	case 'm':
		mtpt = EARGF(usage());
		break;
	case 's':
		srvname = EARGF(usage());
		break;
	case 'p':
		pflag++;
		break;
	default:
		usage();
	}ARGEND
	
	xsrv.attach = xattach;
	xsrv.open = xopen;
	xsrv.read = xread;
	xsrv.stat = xstat;
	xsrv.walk1 = xwalk1;
	xsrv.destroyfid = xdestroyfid;

	fmtinstall('R', Rfmt);
	fmtinstall('P', Pfmt);
	memimageinit();
	defont = getmemdefont();
	loadfonts();
	qsort(xfont, nxfont, sizeof xfont[0], fontcmp);
	
	if(pflag) {
		if(argc != 1 || chatty9p || chattyfuse)
			usage();
		dump(argv[0]);
		exits(0);
	}

	if(pflag || argc != 0)
		usage();

	/*
	 * Check twice -- if there is an exited instance
	 * mounted there, the first access will fail but unmount it.
	 */
	if(mtpt && access(mtpt, AEXIST) < 0 && access(mtpt, AEXIST) < 0)
		sysfatal("mountpoint %s does not exist", mtpt);

	xsrv.foreground = 1;
	threadpostmountsrv(&xsrv, srvname, mtpt, 0);
}

/*
	/sys/src/libc/9sys/dirread.c
*/
static
long
dirpackage(uchar *buf, long ts, Dir **d)
{
	char *s;
	long ss, i, n, nn, m;

	*d = nil;
	if(ts <= 0)
		return 0;

	/*
	 * first find number of all stats, check they look like stats, & size all associated strings
	 */
	ss = 0;
	n = 0;
	for(i = 0; i < ts; i += m){
		m = BIT16SZ + GBIT16(&buf[i]);
		if(statcheck(&buf[i], m) < 0)
			break;
		ss += m;
		n++;
	}

	if(i != ts)
		return -1;

	*d = malloc(n * sizeof(Dir) + ss);
	if(*d == nil)
		return -1;

	/*
	 * then convert all buffers
	 */
	s = (char*)*d + n * sizeof(Dir);
	nn = 0;
	for(i = 0; i < ts; i += m){
		m = BIT16SZ + GBIT16((uchar*)&buf[i]);
		if(nn >= n || convM2D(&buf[i], m, *d + nn, s) != m){
			free(*d);
			*d = nil;
			return -1;
		}
		nn++;
		s += m;
	}

	return nn;
}

