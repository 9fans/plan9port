#include "stdinc.h"
#include "dat.h"
#include "fns.h"

int
readifile(IFile *f, char *name)
{
	ZBlock *b;

	b = readfile(name);
	if(b == nil)
		return -1;
	f->name = name;
	f->b = b;
	f->pos = 0;
	return 0;
}

void
freeifile(IFile *f)
{
	freezblock(f->b);
	f->b = nil;
	f->pos = 0;
}

int
partifile(IFile *f, Part *part, u64int start, u32int size)
{
	ZBlock *b;

	b = alloczblock(size, 0);
	if(b == nil)
		return -1;
	if(readpart(part, start, b->data, size) < 0){
		seterr(EAdmin, "can't read %s: %r", part->name);
		freezblock(b);
		return -1;
	}
	f->name = part->name;
	f->b = b;
	f->pos = 0;
	return 0;
}

/*
 * return the next non-blank input line,
 * stripped of leading white space and with # comments eliminated
 */
char*
ifileline(IFile *f)
{
	char *s, *e, *t;
	int c;

	for(;;){
		s = (char*)&f->b->data[f->pos];
		e = memchr(s, '\n', f->b->len - f->pos);
		if(e == nil)
			return nil;
		*e++ = '\0';
		f->pos = e - (char*)f->b->data;
		t = strchr(s, '#');
		if(t != nil)
			*t = '\0';
		for(; c = *s; s++)
			if(c != ' ' && c != '\t' && c != '\r')
				return s;
	}
}

int
ifilename(IFile *f, char *dst)
{
	char *s;

	s = ifileline(f);
	if(s == nil || strlen(s) >= ANameSize)
		return -1;
	namecp(dst, s);
	return 0;
}

int
ifileu32int(IFile *f, u32int *r)
{
	char *s;

	s = ifileline(f);
	if(s == nil)
		return -1;
	return stru32int(s, r);
}
