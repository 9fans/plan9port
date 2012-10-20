#include <u.h>
#include <unistd.h>
#include <fcntl.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include "ioproc.h"

static long
_ioclose(va_list *arg)
{
	int fd;

	fd = va_arg(*arg, int);
	return close(fd);
}
int
ioclose(Ioproc *io, int fd)
{
	return iocall(io, _ioclose, fd);
}

static long
_iodial(va_list *arg)
{
	char *addr, *local, *dir;
	int *cdfp, fd;

	addr = va_arg(*arg, char*);
	local = va_arg(*arg, char*);
	dir = va_arg(*arg, char*);
	cdfp = va_arg(*arg, int*);

	fd = dial(addr, local, dir, cdfp);
	return fd;
}
int
iodial(Ioproc *io, char *addr, char *local, char *dir, int *cdfp)
{
	return iocall(io, _iodial, addr, local, dir, cdfp);
}

static long
_ioopen(va_list *arg)
{
	char *path;
	int mode;

	path = va_arg(*arg, char*);
	mode = va_arg(*arg, int);
	return open(path, mode);
}
int
ioopen(Ioproc *io, char *path, int mode)
{
	return iocall(io, _ioopen, path, mode);
}

static long
_ioread(va_list *arg)
{
	int fd;
	void *a;
	long n;

	fd = va_arg(*arg, int);
	a = va_arg(*arg, void*);
	n = va_arg(*arg, long);
	return read(fd, a, n);
}
long
ioread(Ioproc *io, int fd, void *a, long n)
{
	return iocall(io, _ioread, fd, a, n);
}

static long
_ioreadn(va_list *arg)
{
	int fd;
	void *a;
	long n;

	fd = va_arg(*arg, int);
	a = va_arg(*arg, void*);
	n = va_arg(*arg, long);
	n = readn(fd, a, n);
	return n;
}
long
ioreadn(Ioproc *io, int fd, void *a, long n)
{
	return iocall(io, _ioreadn, fd, a, n);
}

static long
_iosleep(va_list *arg)
{
	long n;

	n = va_arg(*arg, long);
	return sleep(n);
}
int
iosleep(Ioproc *io, long n)
{
	return iocall(io, _iosleep, n);
}

static long
_iowrite(va_list *arg)
{
	int fd;
	void *a;
	long n, nn;

	fd = va_arg(*arg, int);
	a = va_arg(*arg, void*);
	n = va_arg(*arg, long);
	nn = write(fd, a, n);
	return nn;
}
long
iowrite(Ioproc *io, int fd, void *a, long n)
{
	n = iocall(io, _iowrite, fd, a, n);
	return n;
}

static long
_iosendfd(va_list *arg)
{
	int n, fd, fd2;

	fd = va_arg(*arg, int);
	fd2 = va_arg(*arg, int);
	n = sendfd(fd, fd2);
	return n;
}
int
iosendfd(Ioproc *io, int fd, int fd2)
{
	return iocall(io, _iosendfd, fd, fd2);
}

static long
_iorecvfd(va_list *arg)
{
	int n, fd;

	fd = va_arg(*arg, int);
	n = recvfd(fd);
	return n;
}
int
iorecvfd(Ioproc *io, int fd)
{
	return iocall(io, _iorecvfd, fd);
}

static long
_ioread9pmsg(va_list *arg)
{
	int fd;
	void *a;
	int n;

	fd = va_arg(*arg, int);
	a = va_arg(*arg, void*);
	n = va_arg(*arg, int);
	read9pmsg(fd, a, n);
	return n;
}
int
ioread9pmsg(Ioproc *io, int fd, void *a, int n)
{
	return iocall(io, _ioread9pmsg, fd, a, n);
}
