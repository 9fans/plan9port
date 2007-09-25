#include "stdinc.h"
#include "dat.h"
#include "fns.h"

static char vcmagic[] = "venti config\n";

enum {
	Maxconfig = 8 * 1024,
	Maglen = sizeof vcmagic - 1,
};

int
readifile(IFile *f, char *name)
{
	Part *p;
	ZBlock *b;
	u8int *z;

	p = initpart(name, OREAD);
	if(p == nil)
		return -1;
	b = alloczblock(Maxconfig+1, 1, 0);
	if(b == nil){
		seterr(EOk, "can't alloc for %s: %R", name);
		return -1;
	}
	if(p->size > PartBlank){
		/*
		 * this is likely a real venti partition, in which case we're
		 * looking for the config file stored as 8k at end of PartBlank.
		 */
		if(readpart(p, PartBlank-Maxconfig, b->data, Maxconfig) < 0){
			seterr(EOk, "can't read %s: %r", name);
			freezblock(b);
			freepart(p);
			return -1;
		}
		b->data[Maxconfig] = '\0';
		if(memcmp(b->data, vcmagic, Maglen) != 0){
			seterr(EOk, "bad venti config magic in %s", name);
			freezblock(b);
			freepart(p);
			return -1;
		}
		/*
		 * if we change b->data+b->_size, freezblock
		 * will blow an assertion, so don't.
		 */
		b->data  += Maglen;
		b->_size -= Maglen;
		b->len   -= Maglen;
		z = memchr(b->data, '\0', b->len);
		if(z)
			b->len = z - b->data;
	}else if(p->size > Maxconfig){
		seterr(EOk, "config file is too large");
		freepart(p);
		freezblock(b);
		return -1;
	}else{
		freezblock(b);
		b = readfile(name);
		if(b == nil){
			freepart(p);
			return -1;
		}
	}
	freepart(p);
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

	b = alloczblock(size, 0, part->blocksize);
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
