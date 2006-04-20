#include	"mk.h"

void
setvar(char *name, void *ptr)
{
	symlook(name, S_VAR, ptr)->u.ptr = ptr;
	symlook(name, S_MAKEVAR, (void*)"");
}

static void
print1(Symtab *s)
{
	Word *w;

	Bprint(&bout, "\t%s=", s->name);
	for (w = s->u.ptr; w; w = w->next)
		Bprint(&bout, "'%s'", w->s);
	Bprint(&bout, "\n");
}

void
dumpv(char *s)
{
	Bprint(&bout, "%s:\n", s);
	symtraverse(S_VAR, print1);
}

char *
shname(char *a)
{
	Rune r;
	int n;

	while (*a) {
		n = chartorune(&r, a);
		if (!WORDCHR(r))
			break;
		a += n;
	}
	return a;
}
