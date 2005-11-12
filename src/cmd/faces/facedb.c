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
	Nsave	= 20,
};

static Facefile	*facefiles;
static int		nsaved;
static char	*facedom;

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
translatedomain(char *dom)
{
	static char buf[200];
	char *p, *ep, *q, *nextp, *file;
	char *bbuf, *ebuf;
	Reprog *exp;

	if(dom == nil || *dom == 0)
		return nil;

	if((file = readfile(unsharp("#9/face/.machinelist"))) == nil)
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
tryfindpicture_user(char *dom, char *user, int depth)
{
	static char buf[200];
	char *p, *q, *nextp, *file;
	static char *home;

	if(home == nil)
		home = getenv("home");
	if(home == nil)
		home = getenv("HOME");
	if(home == nil)
		return nil;

	sprint(buf, "%s/lib/face/48x48x%d/.dict", home, depth);
	if((file = readfile(buf)) == nil)
		return nil;

	snprint(buf, sizeof buf, "%s/%s", dom, user);

	for(p=file; p; p=nextp) {
		if(nextp = strchr(p, '\n'))
			*nextp++ = '\0';

		if(*p == '#' || (q = strpbrk(p, " \t")) == nil)
			continue;
		*q++ = 0;

		if(strcmp(buf, p) == 0) {
			q += strspn(q, " \t");
			q = buf+snprint(buf, sizeof buf, "%s/lib/face/48x48x%d/%s", home, depth, q);
			while(q > buf && (q[-1] == ' ' || q[-1] == '\t'))
				*--q = 0;
			free(file);
			return buf;
		}
	}
	free(file);
	return nil;			
}

static char*
tryfindpicture_global(char *dom, char *user, int depth)
{
	static char buf[200];
	char *p, *q, *nextp, *file;

	sprint(buf, "#9/face/48x48x%d/.dict", depth);
	if((file = readfile(unsharp(buf))) == nil)
		return nil;

	snprint(buf, sizeof buf, "%s/%s", dom, user);

	for(p=file; p; p=nextp) {
		if(nextp = strchr(p, '\n'))
			*nextp++ = '\0';

		if(*p == '#' || (q = strpbrk(p, " \t")) == nil)
			continue;
		*q++ = 0;

		if(strcmp(buf, p) == 0) {
			q += strspn(q, " \t");
			q = buf+snprint(buf, sizeof buf, "#9/face/48x48x%d/%s", depth, q);
			while(q > buf && (q[-1] == ' ' || q[-1] == '\t'))
				*--q = 0;
			free(file);
			return unsharp(buf);
		}
	}
	free(file);
	return nil;			
}

static char*
tryfindpicture(char *dom, char *user, int depth)
{
	char* result;

	if((result = tryfindpicture_user(dom, user, depth)) != nil)
		return result;

	return tryfindpicture_global(dom, user, depth);
}

static char*
tryfindfile(char *dom, char *user, int depth)
{
	char *p, *q;

	for(;;){
		for(p=dom; p; (p=strchr(p, '.')) && p++)
			if(q = tryfindpicture(p, user, depth))
				return q;
		depth >>= 1;
		if(depth == 0)
			break;
	}
	return nil;
}

char*
findfile(Face *f, char *dom, char *user)
{
	char *p;
	int depth;

	if(facedom == nil){
		facedom = getenv("facedom");
		if(facedom == nil)
			facedom = DEFAULT;
	}

	dom = translatedomain(dom);
	if(dom == nil)
		dom = facedom;

	if(screen == nil)
		depth = 8;
	else
		depth = screen->depth;

	if(depth > 8)
		depth = 8;

	f->unknown = 0;
	if(p = tryfindfile(dom, user, depth))
		return p;
	f->unknown = 1;
	p = tryfindfile(dom, "unknown", depth);
	if(p != nil || strcmp(dom, facedom)==0)
		return p;
	return tryfindfile("unknown", "unknown", depth);
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
