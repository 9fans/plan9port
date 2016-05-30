#include	"mk.h"

#define	NHASH	8191
#define	HASHMUL	79L	/* this is a good value */
static Symtab *hash[NHASH];

int
hashof(char *s, unsigned long h)
{
	for(; *s; s++)
		h = h*HASHMUL + *s;
	return h % NHASH;
}

Symtab *
symlook(char *sym, int space, void *install)
{
	Symtab *s;
	int h = hashof(sym, space);

	for(s = hash[h]; s; s = s->next)
		if((s->space == space) && (strcmp(s->name, sym) == 0))
			return(s);

	if(install == 0)
		return(0);

	s = (Symtab *)Malloc(sizeof(Symtab));
	s->space = space;
	s->name = sym;
	s->u.ptr = install;
	s->next = hash[h];
	hash[h] = s;
	return(s);
}

void
symtraverse(int space, void (*fn)(Symtab*))
{
	Symtab **s, *ss;

	for(s = hash; s < &hash[NHASH]; s++)
		for(ss = *s; ss; ss = ss->next)
			if(ss->space == space)
				(*fn)(ss);
}

/*
void
symstat(void)
{
	Symtab **s, *ss;
	int n;
	int l[1000];

	memset((char *)l, 0, sizeof(l));
	for(s = hash; s < &hash[NHASH]; s++){
		for(ss = *s, n = 0; ss; ss = ss->next)
			n++;
		l[n]++;
	}
	for(n = 0; n < 1000; n++)
		if(l[n]) Bprint(&bout, "%ld of length %d\n", l[n], n);
}

void
syminit(void)
{
	Symtab **s, *ss, *next;

	for(s = hash; s < &hash[NHASH]; s++){
		for(ss = *s; ss; ss = next){
			next = ss->next;
			free((char *)ss);
		}
		*s = 0;
	}
}

void
symdel(char *sym, int space)
{
	Symtab *s, *ls;
	int h = hashof(sym, space);

	*/ /* multiple memory leaks */ /*

	for(s = hash[h], ls = 0; s; ls = s, s = s->next)
		if((s->space == space) && (strcmp(s->name, sym) == 0)){
			if(ls)
				ls->next = s->next;
			else
				hash[h] = s->next;
			free((char *)s);
		}
}
*/
