#include "stdinc.h"
#include "dat.h"
#include "fns.h"

int
namecmp(char *s, char *t)
{
	return strncmp(s, t, ANameSize);
}

void
namecp(char *dst, char *src)
{
	strncpy(dst, src, ANameSize - 1);
	dst[ANameSize - 1] = '\0';
}

int
nameok(char *name)
{
	char *t;
	int c;

	if(name == nil)
		return -1;
	for(t = name; c = *t; t++)
		if(t - name >= ANameSize
		|| c < ' ' || c >= 0x7f)
			return -1;
	return 0;
}

int
stru32int(char *s, u32int *r)
{
	char *t;
	u32int n, nn, m;
	int c;

	m = TWID32 / 10;
	n = 0;
	for(t = s; ; t++){
		c = *t;
		if(c < '0' || c > '9')
			break;
		if(n > m)
			return -1;
		nn = n * 10 + c - '0';
		if(nn < n)
			return -1;
		n = nn;
	}
	*r = n;
	return s != t && *t == '\0';
}

int
stru64int(char *s, u64int *r)
{
	char *t;
	u64int n, nn, m;
	int c;

	m = TWID64 / 10;
	n = 0;
	for(t = s; ; t++){
		c = *t;
		if(c < '0' || c > '9')
			break;
		if(n > m)
			return -1;
		nn = n * 10 + c - '0';
		if(nn < n)
			return -1;
		n = nn;
	}
	*r = n;
	return s != t && *t == '\0';
}

int
vttypevalid(int type)
{
	return type < VtMaxType;
}

void
fmtzbinit(Fmt *f, ZBlock *b)
{
	f->runes = 0;
	f->start = b->data;
	f->to = f->start;
	f->stop = (char*)f->start + b->len;
	f->flush = nil;
	f->farg = nil;
	f->nfmt = 0;
	f->args = nil;
}

static int
sflush(Fmt *f)
{
	char *s;
	int n;

	n = (int)f->farg;
	n += 256;
	f->farg = (void*)n;
	s = f->start;
	f->start = realloc(s, n);
	if(f->start == nil){
		f->start = s;
		return 0;
	}
	f->to = (char*)f->start + ((char*)f->to - s);
	f->stop = (char*)f->start + n - 1;
	return 1;
}

static char*
logit(int severity, char *fmt, va_list args)
{
	Fmt f;
	int n;

	f.runes = 0;
	n = 32;
	f.start = malloc(n);
	if(f.start == nil)
		return nil;
	f.to = f.start;
	f.stop = (char*)f.start + n - 1;
	f.flush = sflush;
	f.farg = (void*)n;
	f.nfmt = 0;
	f.args = args;
	n = dofmt(&f, fmt);
	if(n < 0)
{
fprint(2, "dofmt %s failed\n", fmt);
		return nil;
}
	*(char*)f.to = '\0';

	if(argv0 == nil)
		fprint(2, "%s: err %d: %s\n", argv0, severity, f.start);
	else
		fprint(2, "err %d: %s\n", severity, f.start);
	return f.start;
}

void
seterr(int severity, char *fmt, ...)
{
	char *s;
	va_list args;

	va_start(args, fmt);
	s = logit(severity, fmt, args);
	va_end(args);
	if(s == nil)
		werrstr("error setting error");
	else{
		werrstr("%s", s);
		free(s);
	}
}

void
logerr(int severity, char *fmt, ...)
{
	char *s;
	va_list args;

	va_start(args, fmt);
	s = logit(severity, fmt, args);
	va_end(args);
	free(s);
}

u32int
now(void)
{
	return time(nil);
}

void
fatal(char *fmt, ...)
{
	Fmt f;
	char buf[256];

	f.runes = 0;
	f.start = buf;
	f.to = buf;
	f.stop = buf + sizeof(buf);
	f.flush = fmtfdflush;
	f.farg = (void*)2;
	f.nfmt = 0;
	fmtprint(&f, "fatal %s error:", argv0);
	va_start(f.args, fmt);
	dofmt(&f, fmt);
	va_end(f.args);
	fmtprint(&f, "\n");
	fmtfdflush(&f);
	if(0)
		abort();
	threadexitsall(buf);
}

ZBlock *
alloczblock(u32int size, int zeroed)
{
	ZBlock *b;
	static ZBlock z;

	b = malloc(sizeof(ZBlock) + size);
	if(b == nil){
		seterr(EOk, "out of memory");
		return nil;
	}

	*b = z;
	b->data = (u8int*)&b[1];
	b->len = size;
	if(zeroed)
		memset(b->data, 0, size);
	return b;
}

void
freezblock(ZBlock *b)
{
	free(b);
}

ZBlock*
packet2zblock(Packet *p, u32int size)
{
	ZBlock *b;

	if(p == nil)
		return nil;
	b = alloczblock(size, 0);
	if(b == nil)
		return nil;
	b->len = size;
	if(packetcopy(p, b->data, 0, size) < 0){
		freezblock(b);
		return nil;
	}
	return b;
}

Packet*
zblock2packet(ZBlock *zb, u32int size)
{
	Packet *p;

	if(zb == nil)
		return nil;
	p = packetalloc();
	packetappend(p, zb->data, size);
	return p;
}

void *
emalloc(ulong n)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		sysfatal("out of memory");
	memset(p, 0xa5, n);
if(0)print("emalloc %p-%p by %lux\n", p, (char*)p+n, getcallerpc(&n));
	return p;
}

void *
ezmalloc(ulong n)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		sysfatal("out of memory");
	memset(p, 0, n);
if(0)print("ezmalloc %p-%p by %lux\n", p, (char*)p+n, getcallerpc(&n));
	return p;
}

void *
erealloc(void *p, ulong n)
{
	p = realloc(p, n);
	if(p == nil)
		sysfatal("out of memory");
if(0)print("erealloc %p-%p by %lux\n", p, (char*)p+n, getcallerpc(&p));
	return p;
}

char *
estrdup(char *s)
{
	char *t;
	int n;

	n = strlen(s) + 1;
	t = emalloc(n);
	memmove(t, s, n);
if(0)print("estrdup %p-%p by %lux\n", t, (char*)t+n, getcallerpc(&s));
	return t;
}

ZBlock*
readfile(char *name)
{
	Part *p;
	ZBlock *b;

	p = initpart(name, 1);
	if(p == nil)
		return nil;
	b = alloczblock(p->size, 0);
	if(b == nil){
		seterr(EOk, "can't alloc %s: %r", name);
		freepart(p);
		return nil;
	}
	if(readpart(p, 0, b->data, p->size) < 0){
		seterr(EOk, "can't read %s: %r", name);
		freepart(p);
		freezblock(b);
		return nil;
	}
	freepart(p);
	return b;
}

/*
 * return floor(log2(v))
 */
int
u64log2(u64int v)
{
	int i;

	for(i = 0; i < 64; i++)
		if((v >> i) <= 1)
			break;
	return i;
}

int
vtproc(void (*fn)(void*), void *arg)
{
	proccreate(fn, arg, 256*1024);
	return 0;
}

