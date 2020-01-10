#include "std.h"
#include "dat.h"

static Ioproc *cache[5];
static int ncache;

static Ioproc*
xioproc(void)
{
	Ioproc *c;
	int i;

	for(i=0; i<ncache; i++){
		if(c = cache[i]){
			cache[i] = nil;
			return c;
		}
	}

	return ioproc();
}

static void
closexioproc(Ioproc *io)
{
	int i;

	for(i=0; i<ncache; i++)
		if(cache[i] == nil){
			cache[i] = io;
			return;
		}

	closeioproc(io);
}

int
xiodial(char *ds, char *local, char *dir, int *cfdp)
{
	int fd;
	Ioproc *io;

	if((io = xioproc()) == nil)
		return -1;
	fd = iodial(io, ds, local, dir, cfdp);
	closexioproc(io);
	return fd;
}

void
xioclose(int fd)
{
	Ioproc *io;

	if((io = xioproc()) == nil){
		close(fd);
		return;
	}

	ioclose(io, fd);
	closexioproc(io);
}

int
xiowrite(int fd, void *v, int n)
{
	int m;
	Ioproc *io;

	if((io = xioproc()) == nil)
		return -1;
	m = iowrite(io, fd, v, n);
	closexioproc(io);
	if(m != n)
		return -1;
	return n;
}

static long
_ioauthdial(va_list *arg)
{
	char *net;
	char *dom;
	int fd;

	net = va_arg(*arg, char*);
	dom = va_arg(*arg, char*);
	fd = _authdial(net, dom);
	if(fd < 0)
		fprint(2, "authdial: %r\n");
	return fd;
}

int
xioauthdial(char *net, char *dom)
{
	int fd;
	Ioproc *io;

	if((io = xioproc()) == nil)
		return -1;
	fd = iocall(io, _ioauthdial, net, dom);
	closexioproc(io);
	return fd;
}

static long
_ioasrdresp(va_list *arg)
{
	int fd;
	void *a;
	int n;

	fd = va_arg(*arg, int);
	a = va_arg(*arg, void*);
	n = va_arg(*arg, int);

	return _asrdresp(fd, a, n);
}

int
xioasrdresp(int fd, void *a, int n)
{
	Ioproc *io;

	if((io = xioproc()) == nil)
		return -1;

	n = iocall(io, _ioasrdresp, fd, a, n);
	closexioproc(io);
	return n;
}

static long
_ioasgetticket(va_list *arg)
{
	int asfd;
	char *trbuf;
	char *tbuf;

	asfd = va_arg(*arg, int);
	trbuf = va_arg(*arg, char*);
	tbuf = va_arg(*arg, char*);

	return _asgetticket(asfd, trbuf, tbuf);
}

int
xioasgetticket(int fd, char *trbuf, char *tbuf)
{
	int n;
	Ioproc *io;

	if((io = xioproc()) == nil)
		return -1;

	n = iocall(io, _ioasgetticket, fd, trbuf, tbuf);
	closexioproc(io);
	if(n != 2*TICKETLEN)
		n = -1;
	else
		n = 0;
	return n;
}
