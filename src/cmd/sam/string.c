#include "sam.h"

#define	MINSIZE	16		/* minimum number of chars allocated */
#define	MAXSIZE	256		/* maximum number of chars for an empty string */


void
Strinit(String *p)
{
	p->s = emalloc(MINSIZE*RUNESIZE);
	p->n = 0;
	p->size = MINSIZE;
}

void
Strinit0(String *p)
{
	p->s = emalloc(MINSIZE*RUNESIZE);
	p->s[0] = 0;
	p->n = 1;
	p->size = MINSIZE;
}

void
Strclose(String *p)
{
	free(p->s);
}

void
Strzero(String *p)
{
	if(p->size > MAXSIZE){
		p->s = erealloc(p->s, RUNESIZE*MAXSIZE); /* throw away the garbage */
		p->size = MAXSIZE;
	}
	p->n = 0;
}

int
Strlen(Rune *r)
{
	Rune *s;

	for(s=r; *s; s++)
		;
	return s-r;
}

void
Strdupl(String *p, Rune *s)	/* copies the null */
{
	p->n = Strlen(s)+1;
	Strinsure(p, p->n);
	memmove(p->s, s, p->n*RUNESIZE);
}

void
Strduplstr(String *p, String *q)	/* will copy the null if there's one there */
{
	Strinsure(p, q->n);
	p->n = q->n;
	memmove(p->s, q->s, q->n*RUNESIZE);
}

void
Straddc(String *p, int c)
{
	Strinsure(p, p->n+1);
	p->s[p->n++] = c;
}

void
Strinsure(String *p, ulong n)
{
	if(n > STRSIZE)
		error(Etoolong);
	if(p->size < n){	/* p needs to grow */
		n += 100;
		p->s = erealloc(p->s, n*RUNESIZE);
		p->size = n;
	}
}

void
Strinsert(String *p, String *q, Posn p0)
{
	Strinsure(p, p->n+q->n);
	memmove(p->s+p0+q->n, p->s+p0, (p->n-p0)*RUNESIZE);
	memmove(p->s+p0, q->s, q->n*RUNESIZE);
	p->n += q->n;
}

void
Strdelete(String *p, Posn p1, Posn p2)
{
	memmove(p->s+p1, p->s+p2, (p->n-p2)*RUNESIZE);
	p->n -= p2-p1;
}

int
Strcmp(String *a, String *b)
{
	int i, c;

	for(i=0; i<a->n && i<b->n; i++)
		if(c = (a->s[i] - b->s[i]))	/* assign = */
			return c;
	/* damn NULs confuse everything */
	i = a->n - b->n;
	if(i == 1){
		if(a->s[a->n-1] == 0)
			return 0;
	}else if(i == -1){
		if(b->s[b->n-1] == 0)
			return 0;
	}
	return i;
}

int
Strispre(String *a, String *b)
{
	int i;

	for(i=0; i<a->n && i<b->n; i++){
		if(a->s[i] - b->s[i]){	/* assign = */
			if(a->s[i] == 0)
				return 1;
			return 0;
		}
	}
	return i == a->n;
}

char*
Strtoc(String *s)
{
	int i;
	char *c, *d;
	Rune *r;
	c = emalloc(s->n*UTFmax + 1);  /* worst case UTFmax bytes per rune, plus NUL */
	d = c;
	r = s->s;
	for(i=0; i<s->n; i++)
		d += runetochar(d, r++);
	if(d==c || d[-1]!=0)
		*d = 0;
	return c;

}

/*
 * Build very temporary String from Rune*
 */
String*
tmprstr(Rune *r, int n)
{
	static String p;

	p.s = r;
	p.n = n;
	p.size = n;
	return &p;
}

/*
 * Convert null-terminated char* into String
 */
String*
tmpcstr(char *s)
{
	String *p;
	Rune *r;
	int i, n;

	n = utflen(s);	/* don't include NUL */
	p = emalloc(sizeof(String));
	r = emalloc(n*RUNESIZE);
	p->s = r;
	for(i=0; i<n; i++,r++)
		s += chartorune(r, s);
	p->n = n;
	p->size = n;
	return p;
}

void
freetmpstr(String *s)
{
	free(s->s);
	free(s);
}
