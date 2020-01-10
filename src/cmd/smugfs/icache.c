#include "a.h"

// This code is almost certainly wrong.

typedef struct Icache Icache;
struct Icache
{
	char *url;
	HTTPHeader hdr;
	char *tmpfile;
	int fd;
	Icache *next;
	Icache *prev;
	Icache *hash;
};

enum {
	NHASH = 128,
	MAXCACHE = 128,
};
static struct {
	Icache *hash[NHASH];
	Icache *head;
	Icache *tail;
	int n;
} icache;

static Icache*
icachefind(char *url)
{
	int h;
	Icache *ic;

	h = hash(url) % NHASH;
	for(ic=icache.hash[h]; ic; ic=ic->hash){
		if(strcmp(ic->url, url) == 0){
			/* move to front */
			if(ic->prev) {
				ic->prev->next = ic->next;
				if(ic->next)
					ic->next->prev = ic->prev;
				else
					icache.tail = ic->prev;
				ic->prev = nil;
				ic->next = icache.head;
				icache.head->prev = ic;
				icache.head = ic;
			}
			return ic;
		}
	}
	return nil;
}

static Icache*
icacheinsert(char *url, HTTPHeader *hdr, char *file, int fd)
{
	int h;
	Icache *ic, **l;

	if(icache.n == MAXCACHE){
		ic = icache.tail;
		icache.tail = ic->prev;
		if(ic->prev)
			ic->prev->next = nil;
		else
			icache.head = ic->prev;
		h = hash(ic->url) % NHASH;
		for(l=&icache.hash[h]; *l; l=&(*l)->hash){
			if(*l == ic){
				*l = ic->hash;
				goto removed;
			}
		}
		sysfatal("cannot find ic in cache");
	removed:
		free(ic->url);
		close(ic->fd);
		remove(ic->file);
		free(ic->file);
	}else{
		ic = emalloc(sizeof *ic);
		icache.n++;
	}

	ic->url = estrdup(url);
	ic->fd = dup(fd, -1);
	ic->file = estrdup(file);
	ic->hdr = *hdr;
	h = hash(url) % NHASH;
	ic->hash = icache.hash[h];
	icache.hash[h] = ic;
	ic->prev = nil;
	ic->next = icache.head;
	if(ic->next)
		ic->next->prev = ic;
	else
		icache.tail = ic;
	return ic;
}

void
icacheflush(char *substr)
{
	Icache **l, *ic;

	for(l=&icache.head; (ic=*l); ) {
		if(substr == nil || strstr(ic->url, substr)) {
			icache.n--;
			*l = ic->next;
			free(ic->url);
			close(ic->fd);
			remove(ic->file);
			free(ic->file);
			free(ic);
		}else
			l = &ic->next;
	}

	if(icache.head) {
		icache.head->prev = nil;
		for(ic=icache.head; ic; ic=ic->next){
			if(ic->next)
				ic->next->prev = ic;
			else
				icache.tail = ic;
		}
	}else
		icache.tail = nil;
}

int
urlfetch(char *url, HTTPHeader hdr)
{
	Icache *ic;
	char buf[50], *host, *path, *p;
	int fd, len;

	ic = icachefind(url);
	if(ic != nil){
		*hdr = ic->hdr;
		return dup(ic->fd, -1);
	}

	if(memcmp(url, "http://", 7) != 0){
		werrstr("non-http url");
		return -1;
	}
	p = strchr(url+7, '/');
	if(p == nil)
		p = url+strlen(url);
	len = p - (url+7);
	host = emalloc(len+1);
	memmove(host, url+7, len);
	host[len] = 0;
	if(*p == 0)
		p = "/";

	strcpy(buf, "/var/tmp/smugfs.XXXXXX");
	fd = opentemp(buf, ORDWR|ORCLOSE);
	if(fd < 0)
		return -1;
	if(httptofile(http, host, req, &hdr, fd) < 0){
		free(host);
		return -1;
	}
	free(host);
	icacheinsert(url, &hdr, buf, fd);
	return fd;
}
