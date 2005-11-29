#include <u.h>
#include <errno.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>
#include <stabs.h>
#include <ctype.h>
#include "dat.h"

Sym *symbols;
Sym **lsym;

void
addsymx(char *fn, char *name, Type *type)
{
	Sym *s;

	s = emalloc(sizeof *s);
	s->fn = fn;
	s->name = name;
	s->type = type;
	if(lsym == nil)
		lsym = &symbols;
	*lsym = s;
	lsym = &s->next;
}

void
dumpsyms(Biobuf *b)
{
	Sym *s;
	Type *t;

	for(s=symbols; s; s=s->next){
		t = s->type;
		t = defer(t);
		if(t->ty == Pointer){
			t = t->sub;
			if(t && t->equiv)
				t = t->equiv;
		}
		if(t == nil || t->ty != Aggr)
			continue;
		Bprint(b, "complex %B %B%s%B;\n", nameof(t, 1),
			s->fn ? fixname(s->fn) : "", s->fn ? ":" : "", fixname(s->name));
	}

	symbols = nil;
	lsym = &symbols;
}
