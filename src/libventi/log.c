#include <u.h>
#include <libc.h>
#include <venti.h>

#define log	not_the_log_library_call

enum
{	/* defaults */
	LogChunkSize = 8192,
	LogSize = 65536,
};

static struct {
	QLock lk;
	VtLog *hash[1024];
} vl;

static uint
hash(char *s)
{
	uint h;
	uchar *p;

	h = 0;
	for(p=(uchar*)s; *p; p++)
		h = h*37 + *p;
	return h;
}

VtLog*
vtlogopen(char *name, uint size)
{
	uint h;
	int i, nc;
	char *p;
	VtLog *l, *last;

	h = hash(name)%nelem(vl.hash);
	qlock(&vl.lk);
	last = nil;
	for(l=vl.hash[h]; l; last=l, l=l->next)
		if(strcmp(l->name, name) == 0){
			if(last){	/* move to front */
				last->next = l->next;
				l->next = vl.hash[h];
				vl.hash[h] = l;
			}
			l->ref++;
			qunlock(&vl.lk);
			return l;
		}

	if(size == 0){
		qunlock(&vl.lk);
		return nil;
	}

	/* allocate */
	nc = (size+LogChunkSize-1)/LogChunkSize;
	l = vtmalloc(sizeof *l + nc*(sizeof(*l->chunk)+LogChunkSize) + strlen(name)+1);
	memset(l, 0, sizeof *l);
	l->chunk = (VtLogChunk*)(l+1);
	l->nchunk = nc;
	l->w = l->chunk;
	p = (char*)(l->chunk+nc);
	for(i=0; i<nc; i++){
		l->chunk[i].p = p;
		p += LogChunkSize;
		l->chunk[i].ep = p;
	}
	strcpy(p, name);
	l->name = p;
	
	/* insert */
	l->next = vl.hash[h];
	vl.hash[h] = l;
	
	l->ref++;
	qunlock(&vl.lk);
	return l;
}

void
vtlogclose(VtLog *l)
{
	if(l == nil)
		return;

	qlock(&vl.lk);
	if(--l->ref == 0)
		free(l);
	else
		assert(l->ref > 0);
	qunlock(&vl.lk);
}

void
vtlogremove(char *name)
{
	uint h;
	VtLog *last, *l;

	h = hash(name)%nelem(vl.hash);
	qlock(&vl.lk);
	last = nil;
	for(l=vl.hash[h]; l; last=l, l=l->next)
		if(strcmp(l->name, name) == 0){
			if(last)
				last->next = l->next;
			else
				vl.hash[h] = l->next;
			qunlock(&vl.lk);
			vtlogclose(l);
			return;
		}
	qunlock(&vl.lk);
}

void
vtlogvprint(VtLog *l, char *fmt, va_list arg)
{
	int n;
	char *p;
	VtLogChunk *c;

	if(l == nil)
		return;
		
	qlock(&l->lk);
	c = l->w;
	n = c->ep - c->wp;
	if(n < 512){
		c++;
		if(c == l->chunk+l->nchunk)
			c = l->chunk;
		c->wp = c->p;
		l->w = c;
	}
	p = vseprint(c->wp, c->ep, fmt, arg);
	if(p)
		c->wp = p;
	qunlock(&l->lk);
}

void
vtlogprint(VtLog *l, char *fmt, ...)
{
	va_list arg;
	
	if(l == nil)
		return;
		
	va_start(arg, fmt);
	vtlogvprint(l, fmt, arg);
	va_end(arg);
}

void
vtlog(char *name, char *fmt, ...)
{
	VtLog *l;
	va_list arg;
	
	l = vtlogopen(name, LogSize);
	va_start(arg, fmt);
	vtlogvprint(l, fmt, arg);
	va_end(arg);
	vtlogclose(l);
}

void
vtlogdump(int fd, VtLog *l)
{
	int i;
	VtLogChunk *c;

	if(l == nil)
		return;
		
	c = l->w;
	for(i=0; i<l->nchunk; i++){
		if(++c == l->chunk+l->nchunk)
			c = l->chunk;
		write(fd, c->p, c->wp-c->p);
	}
	vtlogclose(l);
}

