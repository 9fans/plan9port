#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

extern int drawdebug;
void
main(int argc, char **argv)
{
	char cc;
	Memimage *x;
	Point c = {208,871};
	int a = 441;
	int b = 441;
	int thick = 0;
	Point sp = {0,0};
	int alpha = 51;
	int phi = 3;
	vlong t0, t1;
	int i, n;
	vlong del;

	memimageinit();

	x = allocmemimage(Rect(0,0,1000,1000), CMAP8);
	n = atoi(argv[1]);

	t0 = nsec();
	t0 = nsec();
	t0 = nsec();
	t1 = nsec();
	del = t1-t0;
	t0 = nsec();
	for(i=0; i<n; i++)
		memarc(x, c, a, b, thick, memblack, sp, alpha, phi, SoverD);
	t1 = nsec();
	print("%lld %lld\n", t1-t0-del, del);
}

int drawdebug = 0;

void
rdb(void)
{
}

int
iprint(char *fmt, ...)
{
	int n;
	va_list va;
	char buf[1024];

	va_start(va, fmt);
	n = doprint(buf, buf+sizeof buf, fmt, va) - buf;
	va_end(va);

	write(1,buf,n);
	return 1;
}
