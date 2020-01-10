#include "a.h"

void*
emalloc(uint n)
{
	void *v;

	v = mallocz(n, 1);
	if(v == nil)
		sysfatal("out of memory");
	return v;
}

char*
estrdup(char *s)
{
	char *t;

	t = strdup(s);
	if(t == nil)
		sysfatal("out of memory");
	return t;
}

Rune*
erunestrdup(Rune *s)
{
	Rune *t;

	t = emalloc(sizeof(Rune)*(runestrlen(s)+1));
	if(t == nil)
		sysfatal("out of memory");
	runestrcpy(t, s);
	return t;
}

void*
erealloc(void *ov, uint n)
{
	void *v;

	v = realloc(ov, n);
	if(v == nil)
		sysfatal("out of memory");
	return v;
}

Rune*
erunesmprint(char *fmt, ...)
{
	Rune *s;
	va_list arg;

	va_start(arg, fmt);
	s = runevsmprint(fmt, arg);
	va_end(arg);
	if(s == nil)
		sysfatal("out of memory");
	return s;
}

char*
esmprint(char *fmt, ...)
{
	char *s;
	va_list arg;

	va_start(arg, fmt);
	s = vsmprint(fmt, arg);
	va_end(arg);
	if(s == nil)
		sysfatal("out of memory");
	return s;
}

void
warn(char *fmt, ...)
{
	va_list arg;

	fprint(2, "htmlroff: %L: ");
	va_start(arg, fmt);
	vfprint(2, fmt, arg);
	va_end(arg);
	fprint(2, "\n");
}

/*
 * For non-Unicode compilers, so we can say
 * L("asdf") and get a Rune string.  Assumes strings
 * are identified by their pointers, so no mutable strings!
 */
typedef struct Lhash Lhash;
struct Lhash
{
	char *s;
	Lhash *next;
	Rune r[1];
};
static Lhash *hash[1127];

Rune*
L(char *s)
{
	Rune *p;
	Lhash *l;
	uint h;

	h = (uintptr)s%nelem(hash);
	for(l=hash[h]; l; l=l->next)
		if(l->s == s)
			return l->r;
	l = emalloc(sizeof *l+(utflen(s)+1)*sizeof(Rune));
	p = l->r;
	l->s = s;
	while(*s)
		s += chartorune(p++, s);
	*p = 0;
	l->next = hash[h];
	hash[h] = l;
	return l->r;
}
