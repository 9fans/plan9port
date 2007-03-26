#include "rc.h"
#include "exec.h"
#include "io.h"
#include "fns.h"

char*
emalloc(long n)
{
	char *p = (char *)Malloc(n);
	if(p==0)
		panic("Can't malloc %d bytes", n);
/*	if(err){ pfmt(err, "malloc %d->%p\n", n, p); flush(err); } /**/
	return p;
}

void
efree(char *p)
{
/*	pfmt(err, "free %p\n", p); flush(err); /**/
	if(p)
		free(p);
	else pfmt(err, "free 0\n");
}
extern int lastword, lastdol;

void
yyerror(char *m)
{
	pfmt(err, "rc: ");
	if(runq->cmdfile && !runq->iflag)
		pfmt(err, "%s:%d: ", runq->cmdfile, runq->lineno);
	else if(runq->cmdfile)
		pfmt(err, "%s: ", runq->cmdfile);
	else if(!runq->iflag)
		pfmt(err, "line %d: ", runq->lineno);
	if(tok[0] && tok[0]!='\n')
		pfmt(err, "token %q: ", tok);
	pfmt(err, "%s\n", m);
	flush(err);
	lastword = 0;
	lastdol = 0;
	while(lastc!='\n' && lastc!=EOF) advance();
	nerror++;
	setvar("status", newword(m, (word *)0));
}
char *bp;

static void
iacvt(int n)
{
	if(n<0){
		*bp++='-';
		n=-n;	/* doesn't work for n==-inf */
	}
	if(n/10)
		iacvt(n/10);
	*bp++=n%10+'0';
}

void
inttoascii(char *s, long n)
{
	bp = s;
	iacvt(n);
	*bp='\0';
}

void
panic(char *s, int n)
{
	pfmt(err, "rc: ");
	pfmt(err, s, n);
	pchr(err, '\n');
	flush(err);
	Abort();
}
