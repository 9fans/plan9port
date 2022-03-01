#include "e.h"
#include "y.tab.h"
#include <ctype.h>

Infile	infile[10];
Infile	*curfile = infile;

#define	MAXSRC	50
Src	src[MAXSRC];	/* input source stack */
Src	*srcp	= src;

extern int getarg(char *);
extern	void eprint(void);

void pushsrc(int type, char *ptr)	/* new input source */
{
	if (++srcp >= src + MAXSRC)
		ERROR "inputs nested too deep" FATAL;
	srcp->type = type;
	srcp->sp = ptr;
	if (dbg > 1) {
		printf("\n%3ld ", (long)(srcp - src));
		switch (srcp->type) {
		case File:
			printf("push file %s\n", ((Infile *)ptr)->fname);
			break;
		case Macro:
			printf("push macro <%s>\n", ptr);
			break;
		case Char:
			printf("push char <%c>\n", *ptr);
			break;
		case String:
			printf("push string <%s>\n", ptr);
			break;
		case Free:
			printf("push free <%s>\n", ptr);
			break;
		default:
			ERROR "pushed bad type %d\n", srcp->type FATAL;
		}
	}
}

void popsrc(void)	/* restore an old one */
{
	if (srcp <= src)
		ERROR "too many inputs popped" FATAL;
	if (dbg > 1) {
		printf("%3ld ", (long)(srcp - src));
		switch (srcp->type) {
		case File:
			printf("pop file\n");
			break;
		case Macro:
			printf("pop macro\n");
			break;
		case Char:
			printf("pop char <%c>\n", *srcp->sp);
			break;
		case String:
			printf("pop string\n");
			break;
		case Free:
			printf("pop free\n");
			break;
		default:
			ERROR "pop weird input %d\n", srcp->type FATAL;
		}
	}
	srcp--;
}

Arg	args[10];	/* argument frames */
Arg	*argfp = args;	/* frame pointer */
int	argcnt;		/* number of arguments seen so far */

void dodef(tbl *stp)	/* collect args and switch input to defn */
{
	int i, len;
	char *p;
	Arg *ap;

	ap = argfp+1;
	if (ap >= args+10)
		ERROR "more than arguments\n" FATAL;
	argcnt = 0;
	if (input() != '(')
		ERROR "disaster in dodef\n"FATAL;
	if (ap->argval == 0)
		ap->argval = malloc(1000);
	for (p = ap->argval; (len = getarg(p)) != -1; p += len) {
		ap->argstk[argcnt++] = p;
		if (input() == ')')
			break;
	}
	for (i = argcnt; i < MAXARGS; i++)
		ap->argstk[i] = "";
	if (dbg)
		for (i = 0; i < argcnt; i++)
			printf("arg %ld.%d = <%s>\n", (long)(ap-args), i+1, ap->argstk[i]);
	argfp = ap;
	pushsrc(Macro, stp->cval);
}

int
getarg(char *p)	/* pick up single argument, store in p, return length */
{
	int n, c, npar;

	n = npar = 0;
	for ( ;; ) {
		c = input();
		if (c == EOF)
			ERROR "end of file in getarg!\n" FATAL;
		if (npar == 0 && (c == ',' || c == ')'))
			break;
		if (c == '"')	/* copy quoted stuff intact */
			do {
				*p++ = c;
				n++;
			} while ((c = input()) != '"' && c != EOF);
		else if (c == '(')
			npar++;
		else if (c == ')')
			npar--;
		n++;
		*p++ = c;
	}
	*p = 0;
	unput(c);
	return(n + 1);
}

#define	PBSIZE	2000
char	pbuf[PBSIZE];		/* pushback buffer */
char	*pb	= pbuf-1;	/* next pushed back character */

char	ebuf[200];		/* collect input here for error reporting */
char	*ep	= ebuf;

int
input(void)
{
	register int c = 0;

  loop:
	switch (srcp->type) {
	case File:
		c = getc(curfile->fin);
		if (c == EOF) {
			if (curfile == infile)
				break;
			if (curfile->fin != stdin) {
				fclose(curfile->fin);
				free(curfile->fname);	/* assumes allocated */
			}
			curfile--;
			printf(".lf %d %s\n", curfile->lineno, curfile->fname);
			popsrc();
			goto loop;
		}
		if (c == '\n')
			curfile->lineno++;
		break;
	case Char:
		if (pb >= pbuf) {
			c = *pb--;
			popsrc();
			break;
		} else {	/* can't happen? */
			popsrc();
			goto loop;
		}
	case String:
		c = *srcp->sp++;
		if (c == '\0') {
			popsrc();
			goto loop;
		} else {
			if (*srcp->sp == '\0')	/* empty, so pop */
				popsrc();
			break;
		}
	case Macro:
		c = *srcp->sp++;
		if (c == '\0') {
			if (--argfp < args)
				ERROR "argfp underflow" FATAL;
			popsrc();
			goto loop;
		} else if (c == '$' && isdigit((unsigned char)*srcp->sp)) {
			int n = 0;
			while (isdigit((unsigned char)*srcp->sp))
				n = 10 * n + *srcp->sp++ - '0';
			if (n > 0 && n <= MAXARGS)
				pushsrc(String, argfp->argstk[n-1]);
			goto loop;
		}
		break;
	case Free:	/* free string */
		free(srcp->sp);
		popsrc();
		goto loop;
	}
	if (ep >= ebuf + sizeof ebuf)
		ep = ebuf;
	*ep++ = c;
	return c;
}

int
unput(int c)
{
	if (++pb >= pbuf + sizeof pbuf)
		ERROR "pushback overflow\n"FATAL;
	if (--ep < ebuf)
		ep = ebuf + sizeof(ebuf) - 1;
	*pb = c;
	pushsrc(Char, pb);
	return c;
}

void pbstr(char *s)
{
	pushsrc(String, s);
}

void error(int die, char *s)
{
	extern char *cmdname;

	if (synerr)
		return;
	fprintf(stderr, "%s: %s", cmdname, s);
	if (errno > 0)
		perror("???");
	if (curfile->fin)
		fprintf(stderr, " near %s:%d",
			curfile->fname, curfile->lineno+1);
	fprintf(stderr, "\n");
	eprint();
	synerr = 1;
	errno = 0;
	if (die) {
		if (dbg)
			abort();
		else
			exit(1);
	}
}

void yyerror(char *s)
{
	error(0, s);	/* temporary */
}

char errbuf[2000];

void eprint(void)	/* try to print context around error */
{
	char *p, *q;

	if (ep == ebuf)
		return;				/* no context */
	p = ep - 1;
	if (p > ebuf && *p == '\n')
		p--;
	for ( ; p >= ebuf && *p != '\n'; p--)
		;
	while (*p == '\n')
		p++;
	fprintf(stderr, " context is\n\t");
	for (q=ep-1; q>=p && *q!=' ' && *q!='\t' && *q!='\n'; q--)
		;
	while (p < q)
		putc(*p++, stderr);
	fprintf(stderr, " >>> ");
	while (p < ep)
		putc(*p++, stderr);
	fprintf(stderr, " <<< ");
	while (pb >= pbuf)
		putc(*pb--, stderr);
	if (curfile->fin)
		fgets(ebuf, sizeof ebuf, curfile->fin);
	fprintf(stderr, "%s", ebuf);
	pbstr("\n.EN\n");	/* safety first */
	ep = ebuf;
}
