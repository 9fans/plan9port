#include "a.h"

struct Pfd
{
	int fd;
};

static Pfd*
httpconnect(char *host)
{
	char buf[1024];
	Pfd *pfd;
	int fd;

	snprint(buf, sizeof buf, "tcp!%s!http", host);
	if((fd = dial(buf, nil, nil, nil)) < 0)
		return nil;
	pfd = emalloc(sizeof *pfd);
	pfd->fd = fd;
	return pfd;
}

static void
httpclose(Pfd *pfd)
{
	if(pfd == nil)
		return;
	close(pfd->fd);
	free(pfd);
}

static int
httpwrite(Pfd *pfd, void *v, int n)
{
	return writen(pfd->fd, v, n);
}

static int
httpread(Pfd *pfd, void *v, int n)
{
	return read(pfd->fd, v, n);
}

Protocol http = {
	httpconnect,
	httpread,
	httpwrite,
	httpclose,
};
