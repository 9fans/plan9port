#include "rc.h"
#include "io.h"
#include "exec.h"
#include "fns.h"
#include "getflags.h"
#define	c0	t->child[0]
#define	c1	t->child[1]
#define	c2	t->child[2]
code *codebuf;
static int codep, ncode, codeline;
#define	emitf(x) ((codep!=ncode || morecode()), codebuf[codep].f = (x), codep++)
#define	emiti(x) ((codep!=ncode || morecode()), codebuf[codep].i = (x), codep++)
#define	emits(x) ((codep!=ncode || morecode()), codebuf[codep].s = (x), codep++)

void stuffdot(int);
void outcode(tree*, int);
void codeswitch(tree*, int);
int iscase(tree*);
code *codecopy(code*);
void codefree(code*);

int
morecode(void)
{
	ncode+=ncode;
	codebuf = (code *)erealloc((char *)codebuf, ncode*sizeof codebuf[0]);
	return 0;
}

void
stuffdot(int a)
{
	if(a<0 || codep<=a)
		panic("Bad address %d in stuffdot", a);
	codebuf[a].i = codep;
}

int
compile(tree *t)
{
	ncode = 100;
	codebuf = emalloc(ncode*sizeof codebuf[0]);
	codep = 0;
	codeline = 0;			/* force source */
	emiti(0);			/* reference count */
	emits(estrdup(lex->file));	/* source file name */
	outcode(t, !lex->qflag && flag['e']!=0);
	if(nerror){
		free(codebuf);
		return 0;
	}
	emitf(Xreturn);
	emitf(0);
	return 1;
}

/*
 * called on a tree where we expect eigther
 * a pattern or a string instead of a glob to
 * remove the GLOB chars from the strings
 * or set glob to 2 for pattern so Xglob
 * is not inserted when compiling the tree.
 */
void
noglobs(tree *t, int pattern)
{
Again:
	if(t==0)
		return;
	if(t->type==WORD && t->glob){
		if(pattern)
			t->glob=2;
		else{
			deglob(t->str);
			t->glob=0;
		}
	}
	if(t->type==PAREN || t->type==WORDS || t->type=='^'){
		t->glob=0;
		noglobs(c1, pattern);
		t = c0;
		goto Again;
	}
}

void
outcode(tree *t, int eflag)
{
	void (*f)(void);
	int p, q;
	tree *tt;
	if(t==0)
		return;
	if(t->type!=NOT && t->type!=';')
		lex->iflast = 0;
	if(t->line != codeline){
		codeline = t->line;
		if(codebuf && codep >= 2 && codebuf[codep-2].f == Xsrcline)
			codebuf[codep-1].i = codeline;
		else {
			emitf(Xsrcline);
			emiti(codeline);
		}
	}
	switch(t->type){
	default:
		pfmt(err, "bad type %d in outcode\n", t->type);
		break;
	case '$':
		emitf(Xmark);
		noglobs(c0, 0);
		outcode(c0, eflag);
		emitf(Xdol);
		break;
	case '"':
		emitf(Xmark);
		emitf(Xmark);
		noglobs(c0, 0);
		outcode(c0, eflag);
		emitf(Xdol);
		emitf(Xqw);
		emitf(Xpush);
		break;
	case SUB:
		emitf(Xmark);
		noglobs(c0, 0);
		outcode(c0, eflag);
		emitf(Xmark);
		noglobs(c1, 0);
		outcode(c1, eflag);
		emitf(Xsub);
		break;
	case '&':
		emitf(Xasync);
		p = emiti(0);

		/* undocumented? */
		emitf(Xmark);
		emitf(Xword);
		emits(estrdup("/dev/null"));
		emitf(Xread);
		emiti(0);

		/* insert rfork s for plan9 */
		f = builtinfunc("rfork");
		if(f){
			emitf(Xmark);
			emitf(Xword);
			emits(estrdup("s"));
			emitf(Xword);
			emits(estrdup("rfork"));
			emitf(f);
		}

		codeline = 0;	/* force source */
		outcode(c0, eflag);
		emitf(Xexit);
		stuffdot(p);
		break;
	case ';':
		outcode(c0, eflag);
		outcode(c1, eflag);
		break;
	case '^':
		emitf(Xmark);
		outcode(c1, eflag);
		emitf(Xmark);
		outcode(c0, eflag);
		emitf(Xconc);
		break;
	case '`':
		emitf(Xmark);
		if(c0){
			noglobs(c0, 0);
			outcode(c0, 0);
		} else {
			emitf(Xmark);
			emitf(Xword);
			emits(estrdup("ifs"));
			emitf(Xdol);
		}
		emitf(Xqw);
		emitf(Xbackq);
		p = emiti(0);
		codeline = 0;	/* force source */
		outcode(c1, 0);
		emitf(Xexit);
		stuffdot(p);
		break;
	case ANDAND:
		outcode(c0, 0);
		emitf(Xtrue);
		p = emiti(0);
		outcode(c1, eflag);
		stuffdot(p);
		break;
	case ARGLIST:
		outcode(c1, eflag);
		outcode(c0, eflag);
		break;
	case BANG:
		outcode(c0, eflag);
		emitf(Xbang);
		break;
	case PCMD:
	case BRACE:
		outcode(c0, eflag);
		break;
	case COUNT:
		emitf(Xmark);
		noglobs(c0, 0);
		outcode(c0, eflag);
		emitf(Xcount);
		break;
	case FN:
		emitf(Xmark);
		noglobs(c0, 0);
		outcode(c0, eflag);
		if(c1){
			emitf(Xfn);
			p = emiti(0);
			emits(fnstr(c1));
			codeline = 0;	/* force source */
			outcode(c1, eflag);
			emitf(Xreturn);
			stuffdot(p);
		}
		else
			emitf(Xdelfn);
		break;
	case IF:
		outcode(c0, 0);
		emitf(Xif);
		p = emiti(0);
		outcode(c1, eflag);
		emitf(Xwastrue);
		stuffdot(p);
		break;
	case NOT:
		if(!lex->iflast)
			yyerror("`if not' does not follow `if(...)'");
		emitf(Xifnot);
		p = emiti(0);
		outcode(c0, eflag);
		stuffdot(p);
		break;
	case OROR:
		outcode(c0, 0);
		emitf(Xfalse);
		p = emiti(0);
		outcode(c1, eflag);
		stuffdot(p);
		break;
	case PAREN:
		outcode(c0, eflag);
		break;
	case SIMPLE:
		emitf(Xmark);
		outcode(c0, eflag);
		emitf(Xsimple);
		if(eflag)
			emitf(Xeflag);
		break;
	case SUBSHELL:
		emitf(Xsubshell);
		p = emiti(0);
		codeline = 0;	/* force source */
		outcode(c0, eflag);
		emitf(Xexit);
		stuffdot(p);
		if(eflag)
			emitf(Xeflag);
		break;
	case SWITCH:
		codeswitch(t, eflag);
		break;
	case TWIDDLE:
		emitf(Xmark);
		noglobs(c1, 1);
		outcode(c1, eflag);
		emitf(Xmark);
		outcode(c0, eflag);
		emitf(Xqw);
		emitf(Xmatch);
		if(eflag)
			emitf(Xeflag);
		break;
	case WHILE:
		q = codep;
		outcode(c0, 0);
		if(q==codep)
			emitf(Xsettrue);	/* empty condition == while(true) */
		emitf(Xtrue);
		p = emiti(0);
		outcode(c1, eflag);
		emitf(Xjump);
		emiti(q);
		stuffdot(p);
		break;
	case WORDS:
		outcode(c1, eflag);
		outcode(c0, eflag);
		break;
	case FOR:
		emitf(Xmark);
		if(c1){
			outcode(c1, eflag);
		}
		else{
			emitf(Xmark);
			emitf(Xword);
			emits(estrdup("*"));
			emitf(Xdol);
		}
		emitf(Xmark);		/* dummy value for Xlocal */
		emitf(Xmark);
		noglobs(c0, 0);
		outcode(c0, eflag);
		emitf(Xlocal);
		p = emitf(Xfor);
		q = emiti(0);
		outcode(c2, eflag);
		emitf(Xjump);
		emiti(p);
		stuffdot(q);
		emitf(Xunlocal);
		break;
	case WORD:
		emitf(Xword);
		emits(t->str);
		t->str=0;	/* passed ownership */
		break;
	case DUP:
		if(t->rtype==DUPFD){
			emitf(Xdup);
			emiti(t->fd0);
			emiti(t->fd1);
		}
		else{
			emitf(Xclose);
			emiti(t->fd0);
		}
		outcode(c1, eflag);
		emitf(Xpopredir);
		break;
	case PIPEFD:
		emitf(Xpipefd);
		emiti(t->rtype);
		p = emiti(0);
		codeline = 0;	/* force source */
		outcode(c0, eflag);
		emitf(Xexit);
		stuffdot(p);
		break;
	case REDIR:
		if(t->rtype!=HERE){
			emitf(Xmark);
			outcode(c0, eflag);
		}
		switch(t->rtype){
		case APPEND:
			emitf(Xappend);
			break;
		case WRITE:
			emitf(Xwrite);
			break;
		case READ:
			emitf(Xread);
			break;
		case RDWR:
			emitf(Xrdwr);
			break;
		case HERE:
			emitf(c0->quoted?Xhereq:Xhere);
			emits(t->str);
			t->str=0;	/* passed ownership */
			break;
		}
		emiti(t->fd0);
		outcode(c1, eflag);
		emitf(Xpopredir);
		break;
	case '=':
		tt = t;
		for(;t && t->type=='=';t = c2);
		if(t){					/* var=value cmd */
			for(t = tt;t->type=='=';t = c2){
				emitf(Xmark);
				outcode(c1, eflag);
				emitf(Xmark);
				noglobs(c0, 0);
				outcode(c0, eflag);
				emitf(Xlocal);		/* push var for cmd */
			}
			outcode(t, eflag);		/* gen. code for cmd */
			for(t = tt; t->type == '='; t = c2)
				emitf(Xunlocal);	/* pop var */
		}
		else{					/* var=value */
			for(t = tt;t;t = c2){
				emitf(Xmark);
				outcode(c1, eflag);
				emitf(Xmark);
				noglobs(c0, 0);
				outcode(c0, eflag);
				emitf(Xassign);	/* set var permanently */
			}
		}
		t = tt;	/* so tests below will work */
		break;
	case PIPE:
		emitf(Xpipe);
		emiti(t->fd0);
		emiti(t->fd1);
		p = emiti(0);
		q = emiti(0);
		codeline = 0;	/* force source */
		outcode(c0, eflag);
		emitf(Xexit);
		stuffdot(p);
		codeline = 0;	/* force source */
		outcode(c1, eflag);
		emitf(Xreturn);
		stuffdot(q);
		emitf(Xpipewait);
		break;
	}
	if(t->glob==1)
		emitf(Xglob);
	if(t->type!=NOT && t->type!=';')
		lex->iflast = t->type==IF;
	else if(c0)
		lex->iflast = c0->type==IF;
}
/*
 * switch code looks like this:
 *	Xmark
 *	(get switch value)
 *	Xjump	1f
 * out:	Xjump	leave
 * 1:	Xmark
 *	(get case values)
 *	Xcase	1f
 *	(commands)
 *	Xjump	out
 * 1:	Xmark
 *	(get case values)
 *	Xcase	1f
 *	(commands)
 *	Xjump	out
 * 1:
 * leave:
 *	Xpopm
 */

void
codeswitch(tree *t, int eflag)
{
	int leave;		/* patch jump address to leave switch */
	int out;		/* jump here to leave switch */
	int nextcase;	/* patch jump address to next case */
	tree *tt;
	if(c1->child[0]==0
	|| c1->child[0]->type!=';'
	|| !iscase(c1->child[0]->child[0])){
		yyerror("case missing in switch");
		return;
	}
	emitf(Xmark);
	outcode(c0, eflag);
	emitf(Xqw);
	emitf(Xjump);
	nextcase = emiti(0);
	out = emitf(Xjump);
	leave = emiti(0);
	stuffdot(nextcase);
	t = c1->child[0];
	while(t->type==';'){
		tt = c1;
		emitf(Xmark);
		for(t = c0->child[0];t->type==ARGLIST;t = c0) {
			noglobs(c1, 1);
			outcode(c1, eflag);
		}
		emitf(Xcase);
		nextcase = emiti(0);
		t = tt;
		for(;;){
			if(t->type==';'){
				if(iscase(c0)) break;
				outcode(c0, eflag);
				t = c1;
			}
			else{
				if(!iscase(t)) outcode(t, eflag);
				break;
			}
		}
		emitf(Xjump);
		emiti(out);
		stuffdot(nextcase);
	}
	stuffdot(leave);
	emitf(Xpopm);
}

int
iscase(tree *t)
{
	if(t->type!=SIMPLE)
		return 0;
	do t = c0; while(t->type==ARGLIST);
	return t->type==WORD && !t->quoted && strcmp(t->str, "case")==0;
}

code*
codecopy(code *cp)
{
	cp[0].i++;
	return cp;
}

void
codefree(code *cp)
{
	code *p;
	if(--cp[0].i!=0)
		return;
	for(p = cp+2;p->f;p++){
		if(p->f==Xappend || p->f==Xclose || p->f==Xread || p->f==Xwrite
		|| p->f==Xrdwr
		|| p->f==Xasync || p->f==Xbackq || p->f==Xcase || p->f==Xfalse
		|| p->f==Xfor || p->f==Xjump
		|| p->f==Xsrcline
		|| p->f==Xsubshell || p->f==Xtrue) p++;
		else if(p->f==Xdup || p->f==Xpipefd) p+=2;
		else if(p->f==Xpipe) p+=4;
		else if(p->f==Xhere || p->f==Xhereq) free(p[1].s), p+=2;
		else if(p->f==Xword) free((++p)->s);
		else if(p->f==Xfn){
			free(p[2].s);
			p+=2;
		}
	}
	free(cp[1].s);
	free(cp);
}
