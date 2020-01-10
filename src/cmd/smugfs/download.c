#include "a.h"

typedef struct DEntry DEntry;
struct DEntry
{
	CEntry ce;
	HTTPHeader hdr;
	char *tmpfile;
	int fd;
};

static void
dfree(CEntry *ce)
{
	DEntry *d;

	d = (DEntry*)ce;
	if(d->tmpfile){
		remove(d->tmpfile);
		free(d->tmpfile);
		close(d->fd);
	}
}

static Cache *downloadcache;

static char*
parseurl(char *url, char **path)
{
	char *host, *p;
	int len;

	if(memcmp(url, "http://", 7) != 0)
		return nil;
	p = strchr(url+7, '/');
	if(p == nil)
		p = url+strlen(url);
	len = p - (url+7);
	host = emalloc(len+1);
	memmove(host, url+7, len);
	host[len] = 0;
	if(*p == 0)
		p = "/";
	*path = p;
	return host;
}

int
download(char *url, HTTPHeader *hdr)
{
	DEntry *d;
	char *host, *path;
	char buf[] = "/var/tmp/smugfs.XXXXXX";
	char *req;
	int fd;
	Fmt fmt;

	if(downloadcache == nil)
		downloadcache = newcache(sizeof(DEntry), 128, dfree);

	host = parseurl(url, &path);
	if(host == nil)
		return -1;

	d = (DEntry*)cachelookup(downloadcache, url, 1);
	if(d->tmpfile){
		free(host);
		*hdr = d->hdr;
		return dup(d->fd, -1);
	}
	d->fd = -1;  // paranoia

	if((fd = opentemp(buf, ORDWR|ORCLOSE)) < 0){
		free(host);
		return -1;
	}

	fmtstrinit(&fmt);
	fmtprint(&fmt, "GET %s HTTP/1.0\r\n", path);
	fmtprint(&fmt, "Host: %s\r\n", host);
	fmtprint(&fmt, "User-Agent: " USER_AGENT "\r\n");
	fmtprint(&fmt, "\r\n");
	req = fmtstrflush(&fmt);

	fprint(2, "Get %s\n", url);

	if(httptofile(&http, host, req, hdr, fd) < 0){
		free(host);
		free(req);
		return -1;
	}
	free(host);
	free(req);
	d->tmpfile = estrdup(buf);
	d->fd = dup(fd, -1);
	d->hdr = *hdr;
	return fd;
}

void
downloadflush(char *substr)
{
	if(downloadcache)
		cacheflush(downloadcache, substr);
}
