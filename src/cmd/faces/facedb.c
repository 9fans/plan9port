#include <u.h>
#include <libc.h>
#include <draw.h>
#include <plumb.h>
#include <regexp.h>
#include <bio.h>
#include <9pclient.h>
#include "faces.h"

enum	/* number of deleted faces to cache */
{
	Nsave	= 20
};

static Facefile	*facefiles;
static int		nsaved;
static char	*facedom;
static char	*libface;
static char	*homeface;

/*
 * Loading the files is slow enough on a dial-up line to be worth this trouble
 */
typedef struct Readcache	Readcache;
struct Readcache {
	char *file;
	char *data;
	long mtime;
	long rdtime;
	Readcache *next;
};

static Readcache *rcache;

ulong
dirlen(char *s)
{
	Dir *d;
	ulong len;

	d = dirstat(s);
	if(d == nil)
		return 0;
	len = d->length;
	free(d);
	return len;
}

ulong
fsdirlen(CFsys *fs,char *s)
{
	Dir *d;
	ulong len;

	d = fsdirstat(fs,s);
	if(d == nil)
		return 0;
	len = d->length;
	free(d);
	return len;
}

ulong
dirmtime(char *s)
{
	Dir *d;
	ulong t;

	d = dirstat(s);
	if(d == nil)
		return 0;
	t = d->mtime;
	free(d);
	return t;
}

static char*
doreadfile(char *s)
{
	char *p;
	int fd, n;
	ulong len;

	len = dirlen(s);
	if(len == 0)
		return nil;

	p = malloc(len+1);
	if(p == nil)
		return nil;

	if((fd = open(s, OREAD)) < 0
	|| (n = readn(fd, p, len)) < 0) {
		close(fd);
		free(p);
		return nil;
	}

	p[n] = '\0';
	return p;
}

static char*
readfile(char *s)
{
	Readcache *r, **l;
	char *p;
	ulong mtime;

	for(l=&rcache, r=*l; r; l=&r->next, r=*l) {
		if(strcmp(r->file, s) != 0)
			continue;

		/*
		 * if it's less than 30 seconds since we read it, or it
		 * hasn't changed, send back our copy
		 */
		if(time(0) - r->rdtime < 30)
			return strdup(r->data);
		if(dirmtime(s) == r->mtime) {
			r->rdtime = time(0);
			return strdup(r->data);
		}

		/* out of date, remove this and fall out of loop */
		*l = r->next;
		free(r->file);
		free(r->data);
		free(r);
		break;
	}

	/* add to cache */
	mtime = dirmtime(s);
	if(mtime == 0)
		return nil;

	if((p = doreadfile(s)) == nil)
		return nil;

	r = malloc(sizeof(*r));
	if(r == nil)
		return nil;
	r->mtime = mtime;
	r->file = estrdup(s);
	r->data = p;
	r->rdtime = time(0);
	r->next = rcache;
	rcache = r;
	return strdup(r->data);
}

static char*
translatedomain(char *dom, char *list)
{
	static char buf[200];
	char *p, *ep, *q, *nextp, *file;
	char *bbuf, *ebuf;
	Reprog *exp;

	if(dom == nil || *dom == 0)
		return nil;

	if(list == nil || (file = readfile(list)) == nil)
		return dom;

	for(p=file; p; p=nextp) {
		if(nextp = strchr(p, '\n'))
			*nextp++ = '\0';

		if(*p == '#' || (q = strpbrk(p, " \t")) == nil || q-p > sizeof(buf)-2)
			continue;

		bbuf = buf+1;
		ebuf = buf+(1+(q-p));
		strncpy(bbuf, p, ebuf-bbuf);
		*ebuf = 0;
		if(*bbuf != '^')
			*--bbuf = '^';
		if(ebuf[-1] != '$') {
			*ebuf++ = '$';
			*ebuf = 0;
		}

		if((exp = regcomp(bbuf)) == nil){
			fprint(2, "bad regexp in machinelist: %s\n", bbuf);
			killall("regexp");
		}

		if(regexec(exp, dom, 0, 0)){
			free(exp);
			ep = p+strlen(p);
			q += strspn(q, " \t");
			if(ep-q+2 > sizeof buf) {
				fprint(2, "huge replacement in machinelist: %.*s\n", utfnlen(q, ep-q), q);
				exits("bad big replacement");
			}
			strncpy(buf, q, ep-q);
			ebuf = buf+(ep-q);
			*ebuf = 0;
			while(ebuf > buf && (ebuf[-1] == ' ' || ebuf[-1] == '\t'))
				*--ebuf = 0;
			free(file);
			return buf;
		}
		free(exp);
	}
	free(file);

	return dom;
}

static char*
tryfindpicture(char *dom, char *user, char *dir, char *dict)
{
	static char buf[1024];
	char *file, *p, *nextp, *q;

	if((file = readfile(dict)) == nil)
		return nil;

	snprint(buf, sizeof buf, "%s/%s", dom, user);

	for(p=file; p; p=nextp){
		if(nextp = strchr(p, '\n'))
			*nextp++ = '\0';

		if(*p == '#' || (q = strpbrk(p, " \t")) == nil)
			continue;
		*q++ = 0;

		if(strcmp(buf, p) == 0){
			q += strspn(q, " \t");
			snprint(buf, sizeof buf, "%s/%s", dir, q);
			q = buf+strlen(buf);
			while(q > buf && (q[-1] == ' ' || q[-1] == '\t'))
				*--q = 0;
			free(file);
			return estrdup(buf);
		}
	}
	free(file);
	return nil;
}

static char*
estrstrdup(char *a, char *b)
{
	char *t;

	t = emalloc(strlen(a)+strlen(b)+1);
	strcpy(t, a);
	strcat(t, b);
	return t;
}

static char*
tryfindfiledir(char *dom, char *user, char *dir)
{
	char *dict, *ndir, *x, *odom;
	int fd;
	int i, n;
	Dir *d;

	/*
	 * If this directory has a .machinelist, use it.
	 */
	x = estrstrdup(dir, "/.machinelist");
	dom = estrdup(translatedomain(dom, x));
	free(x);
	/*
	 * If this directory has a .dict, use it.
	 */
	dict = estrstrdup(dir, "/.dict");
	if(access(dict, AEXIST) >= 0){
		x = tryfindpicture(dom, user, dir, dict);
		free(dict);
		free(dom);
		return x;
	}
	free(dict);

	/*
	 * If not, recurse into subdirectories.
	 * Ignore 48x48xN directories for now.
	 */
	if((fd = open(dir, OREAD)) < 0)
		return nil;
	while((n = dirread(fd, &d)) > 0){
		for(i=0; i<n; i++){
			if((d[i].mode&DMDIR)&& strncmp(d[i].name, "48x48x", 6) != 0){
				ndir = emalloc(strlen(dir)+1+strlen(d[i].name)+1);
				strcpy(ndir, dir);
				strcat(ndir, "/");
				strcat(ndir, d[i].name);
				if((x = tryfindfiledir(dom, user, ndir)) != nil){
					free(ndir);
					free(d);
					close(fd);
					free(dom);
					return x;
				}
			}
		}
		free(d);
	}
	close(fd);

	/*
	 * Handle 48x48xN directories in the right order.
	 */
	ndir = estrstrdup(dir, "/48x48x8");
	for(i=8; i>0; i>>=1){
		ndir[strlen(ndir)-1] = i+'0';
		if(access(ndir, AEXIST) >= 0 && (x = tryfindfiledir(dom, user, ndir)) != nil){
			free(ndir);
			free(dom);
			return x;
		}
	}
	free(ndir);
	free(dom);
	return nil;
}

static char*
tryfindfile(char *dom, char *user)
{
	char *p;

	while(dom && *dom){
		if(homeface && (p = tryfindfiledir(dom, user, homeface)) != nil)
			return p;
		if((p = tryfindfiledir(dom, user, libface)) != nil)
			return p;
		if((dom = strchr(dom, '.')) == nil)
			break;
		dom++;
	}
	return nil;
}

char*
findfile(Face *f, char *dom, char *user)
{
	char *p;

	if(facedom == nil){
		facedom = getenv("facedom");
		if(facedom == nil)
			facedom = DEFAULT;
	}
	if(libface == nil)
		libface = unsharp("#9/face");
	if(homeface == nil)
		homeface = smprint("%s/lib/face", getenv("HOME"));

	if(dom == nil)
		dom = facedom;

	f->unknown = 0;
	if((p = tryfindfile(dom, user)) != nil)
		return p;
	f->unknown = 1;
	p = tryfindfile(dom, "unknown");
	if(p != nil || strcmp(dom, facedom) == 0)
		return p;
	return tryfindfile("unknown", "unknown");
}

static
void
clearsaved(void)
{
	Facefile *f, *next, **lf;

	lf = &facefiles;
	for(f=facefiles; f!=nil; f=next){
		next = f->next;
		if(f->ref > 0){
			*lf = f;
			lf = &(f->next);
			continue;
		}
		if(f->image != display->black && f->image != display->white)
			freeimage(f->image);
		free(f->file);
		free(f);
	}
	*lf = nil;
	nsaved = 0;
}

void
freefacefile(Facefile *f)
{
	if(f==nil || f->ref-->1)
		return;
	if(++nsaved > Nsave)
		clearsaved();
}

static Image*
myallocimage(ulong chan)
{
	Image *img;
	img = allocimage(display, Rect(0,0,Facesize,Facesize), chan, 0, DNofill);
	if(img == nil){
		clearsaved();
		img = allocimage(display, Rect(0,0,Facesize,Facesize), chan, 0, DNofill);
		if(img == nil)
			return nil;
	}
	return img;
}


static Image*
readbit(int fd, ulong chan)
{
	char buf[4096], hx[4], *p;
	uchar data[Facesize*Facesize];	/* more than enough */
	int nhx, i, n, ndata, nbit;
	Image *img;

	n = readn(fd, buf, sizeof buf);
	if(n <= 0)
		return nil;
	if(n >= sizeof buf)
		n = sizeof(buf)-1;
	buf[n] = '\0';

	n = 0;
	nhx = 0;
	nbit = chantodepth(chan);
	ndata = (Facesize*Facesize*nbit)/8;
	p = buf;
	while(n < ndata) {
		p = strpbrk(p+1, "0123456789abcdefABCDEF");
		if(p == nil)
			break;
		if(p[0] == '0' && p[1] == 'x')
			continue;

		hx[nhx] = *p;
		if(++nhx == 2) {
			hx[nhx] = 0;
			i = strtoul(hx, 0, 16);
			data[n++] = i;
			nhx = 0;
		}
	}
	if(n < ndata)
		return allocimage(display, Rect(0,0,Facesize,Facesize), CMAP8, 0, 0x88888888);

	img = myallocimage(chan);
	if(img == nil)
		return nil;
	loadimage(img, img->r, data, ndata);
	return img;
}

static Facefile*
readface(char *fn)
{
	int x, y, fd;
	uchar bits;
	uchar *p;
	Image *mask;
	Image *face;
	char buf[16];
	uchar data[Facesize*Facesize];
	uchar mdata[(Facesize*Facesize)/8];
	Facefile *f;
	Dir *d;

	for(f=facefiles; f!=nil; f=f->next){
		if(strcmp(fn, f->file) == 0){
			if(f->image == nil)
				break;
			if(time(0) - f->rdtime >= 30) {
				if(dirmtime(fn) != f->mtime){
					f = nil;
					break;
				}
				f->rdtime = time(0);
			}
			f->ref++;
			return f;
		}
	}

	if((fd = open(fn, OREAD)) < 0)
		return nil;

	if(readn(fd, buf, sizeof buf) != sizeof buf){
		close(fd);
		return nil;
	}

	seek(fd, 0, 0);

	mask = nil;
	if(buf[0] == '0' && buf[1] == 'x'){
		/* greyscale faces are just masks that we draw black through! */
		if(buf[2+8] == ',')	/* ldepth 1 */
			mask = readbit(fd, GREY2);
		else
			mask = readbit(fd, GREY1);
		face = display->black;
	}else{
		face = readimage(display, fd, 0);
		if(face == nil)
			goto Done;
		else if(face->chan == GREY4 || face->chan == GREY8){	/* greyscale: use inversion as mask */
			mask = myallocimage(face->chan);
			/* okay if mask is nil: that will copy the image white background and all */
			if(mask == nil)
				goto Done;

			/* invert greyscale image */
			draw(mask, mask->r, display->white, nil, ZP);
			gendraw(mask, mask->r, display->black, ZP, face, face->r.min);
			freeimage(face);
			face = display->black;
		}else if(face->depth == 8){	/* snarf the bytes back and do a fill. */
			mask = myallocimage(GREY1);
			if(mask == nil)
				goto Done;
			if(unloadimage(face, face->r, data, Facesize*Facesize) != Facesize*Facesize){
				freeimage(mask);
				goto Done;
			}
			bits = 0;
			p = mdata;
			for(y=0; y<Facesize; y++){
				for(x=0; x<Facesize; x++){
					bits <<= 1;
					if(data[Facesize*y+x] != 0xFF)
						bits |= 1;
					if((x&7) == 7)
						*p++ = bits&0xFF;
				}
			}
			if(loadimage(mask, mask->r, mdata, sizeof mdata) != sizeof mdata){
				freeimage(mask);
				goto Done;
			}
		}
	}

Done:
	/* always add at beginning of list, so updated files don't collide in cache */
	if(f == nil){
		f = emalloc(sizeof(Facefile));
		f->file = estrdup(fn);
		d = dirfstat(fd);
		if(d != nil){
			f->mtime = d->mtime;
			free(d);
		}
		f->next = facefiles;
		facefiles = f;
	}
	f->ref++;
	f->image = face;
	f->mask = mask;
	f->rdtime = time(0);
	close(fd);
	return f;
}

void
findbit(Face *f)
{
	char *fn;

	fn = findfile(f, f->str[Sdomain], f->str[Suser]);
	if(fn) {
		if(strstr(fn, "unknown"))
			f->unknown = 1;
		f->file = readface(fn);
	}
	if(f->file){
		f->bit = f->file->image;
		f->mask = f->file->mask;
	}else{
		/* if returns nil, this is still ok: draw(nil) works */
		f->bit = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DYellow);
		replclipr(f->bit, 1, Rect(0, 0, Facesize, Facesize));
		f->mask = nil;
	}
}
