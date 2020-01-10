#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <9pclient.h>

/* C99 nonsense */
#ifdef va_copy
#	define VA_COPY(a,b) va_copy(a,b)
#	define VA_END(a) va_end(a)
#else
#	define VA_COPY(a,b) (a) = (b)
#	define VA_END(a)
#endif

static int
fidflush(Fmt *f)
{
	int n;

	n = (char*)f->to - (char*)f->start;
	if(n && fswrite(f->farg, f->start, n) != n)
		return 0;
	f->to = f->start;
	return 1;
}

static int
fsfmtfidinit(Fmt *f, CFid *fid, char *buf, int size)
{
	f->runes = 0;
	f->start = buf;
	f->to = buf;
	f->stop = buf + size;
	f->flush = fidflush;
	f->farg = fid;
	f->nfmt = 0;
	fmtlocaleinit(f, nil, nil, nil);
	return 0;
}

int
fsprint(CFid *fd, char *fmt, ...)
{
	int n;
	va_list args;

	va_start(args, fmt);
	n = fsvprint(fd, fmt, args);
	va_end(args);
	return n;
}

int
fsvprint(CFid *fd, char *fmt, va_list args)
{
	Fmt f;
	char buf[256];
	int n;

	fsfmtfidinit(&f, fd, buf, sizeof(buf));
	VA_COPY(f.args,args);
	n = dofmt(&f, fmt);
	VA_END(f.args);
	if(n > 0 && fidflush(&f) == 0)
		return -1;
	return n;
}
