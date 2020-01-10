#include "a.h"

/*
 * 16. Conditional acceptance of input.
 *
 *	conditions are
 *		c - condition letter (o, e, t, n)
 *		!c - not c
 *		N - N>0
 *		!N - N <= 0
 *		'a'b' - if a==b
 *		!'a'b'	- if a!=b
 *
 *	\{xxx\} can be used for newline in bodies
 *
 *	.if .ie .el
 *
 */

int iftrue[20];
int niftrue;

void
startbody(void)
{
	int c;

	while((c = getrune()) == ' ' || c == '\t')
		;
	ungetrune(c);
}

void
skipbody(void)
{
	int c, cc, nbrace;

	nbrace = 0;
	for(cc=0; (c = getrune()) >= 0; cc=c){
		if(c == '\n' && nbrace <= 0)
			break;
		if(cc == '\\' && c == '{')
			nbrace++;
		if(cc == '\\' && c == '}')
			nbrace--;
	}
}

int
ifeval(void)
{
	int c, cc, neg, nc;
	Rune line[MaxLine], *p, *e, *q;
	Rune *a;

	while((c = getnext()) == ' ' || c == '\t')
		;
	neg = 0;
	while(c == '!'){
		neg = !neg;
		c = getnext();
	}

	if('0' <= c && c <= '9'){
		ungetnext(c);
		a = copyarg();
		c = (eval(a)>0) ^ neg;
		free(a);
		return c;
	}

	switch(c){
	case ' ':
	case '\n':
		ungetnext(c);
		return !neg;
	case 'o':	/* odd page */
	case 't':	/* troff */
	case 'h':	/* htmlroff */
		while((c = getrune()) != ' ' && c != '\t' && c != '\n' && c >= 0)
			;
		return 1 ^ neg;
	case 'n':	/* nroff */
	case 'e':	/* even page */
		while((c = getnext()) != ' ' && c != '\t' && c != '\n' && c >= 0)
			;
		return 0 ^ neg;
	}

	/* string comparison 'string1'string2' */
	p = line;
	e = p+nelem(line);
	nc = 0;
	q = nil;
	while((cc=getnext()) >= 0 && cc != '\n' && p<e){
		if(cc == c){
			if(++nc == 2)
				break;
			q = p;
		}
		*p++ = cc;
	}
	if(cc != c){
		ungetnext(cc);
		return 0;
	}
	if(nc < 2){
		return 0;
	}
	*p = 0;
	return (q-line == p-(q+1)
		&& memcmp(line, q+1, (q-line)*sizeof(Rune))==0) ^ neg;
}

void
r_if(Rune *name)
{
	int n;

	n = ifeval();
	if(runestrcmp(name, L("ie")) == 0){
		if(niftrue >= nelem(iftrue))
			sysfatal("%Cie overflow", dot);
		iftrue[niftrue++] = n;
	}
	if(n)
		startbody();
	else
		skipbody();
}

void
r_el(Rune *name)
{
	USED(name);

	if(niftrue <= 0){
		warn("%Cel underflow", dot);
		return;
	}
	if(iftrue[--niftrue])
		skipbody();
	else
		startbody();
}

void
t16init(void)
{
	addraw(L("if"), r_if);
	addraw(L("ie"), r_if);
	addraw(L("el"), r_el);

	addesc('{', e_nop, HtmlMode|ArgMode);
	addesc('}', e_nop, HtmlMode|ArgMode);
}
