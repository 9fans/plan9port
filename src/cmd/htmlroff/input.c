/*
 * Read input files.
 */
#include "a.h"

typedef struct Istack Istack;
struct Istack
{
	Rune unget[3];
	int nunget;
	Biobuf *b;
	Rune *p;
	Rune *ep;
	Rune *s;
	int lineno;
	Rune *name;
	Istack *next;
	void (*fn)(void);
};

Istack *istack;
Istack *ibottom;

static void
setname(void)
{
	Rune *r, *p;

	if(istack == nil || istack->name == nil)
		return;
	_nr(L(".F"), istack->name);
	r = erunestrdup(istack->name);
	p = runestrchr(r, '.');
	if(p)
		*p = 0;
	_nr(L(".B"), r);
	free(r);
}

static void
ipush(Istack *is)
{
	if(istack == nil)
		ibottom = is;
	else
		is->next = istack;
	istack = is;
	setname();
}

static void
iqueue(Istack *is)
{
	if(ibottom == nil){
		istack = is;
		setname();
	}else
		ibottom->next = is;
	ibottom = is;
}

int
_inputfile(Rune *s, void (*push)(Istack*))
{
	Istack *is;
	Biobuf *b;
	char *t;

	t = esmprint("%S", s);
	if((b = Bopen(t, OREAD)) == nil){
		free(t);
		fprint(2, "%s: open %S: %r\n", argv0, s);
		return -1;
	}
	free(t);
	is = emalloc(sizeof *is);
	is->b = b;
	is->name = erunestrdup(s);
	is->lineno = 1;
	push(is);
	return 0;
}

int
pushinputfile(Rune *s)
{
	return _inputfile(s, ipush);
}

int
queueinputfile(Rune *s)
{
	return _inputfile(s, iqueue);
}

int
_inputstdin(void (*push)(Istack*))
{
	Biobuf *b;
	Istack *is;

	if((b = Bopen("/dev/null", OREAD)) == nil){
		fprint(2, "%s: open /dev/null: %r\n", argv0);
		return -1;
	}
	dup(0, b->fid);
	is = emalloc(sizeof *is);
	is->b = b;
	is->name = erunestrdup(L("stdin"));
	is->lineno = 1;
	push(is);
	return 0;
}

int
pushstdin(void)
{
	return _inputstdin(ipush);
}

int
queuestdin(void)
{
	return _inputstdin(iqueue);
}

void
_inputstring(Rune *s, void (*push)(Istack*))
{
	Istack *is;

	is = emalloc(sizeof *is);
	is->s = erunestrdup(s);
	is->p = is->s;
	is->ep = is->p+runestrlen(is->p);
	push(is);
}

void
pushinputstring(Rune *s)
{
	_inputstring(s, ipush);
}


void
inputnotify(void (*fn)(void))
{
	if(istack)
		istack->fn = fn;
}

int
popinput(void)
{
	Istack *is;

	is = istack;
	if(is == nil)
		return 0;

	istack = istack->next;
	if(is->b)
		Bterm(is->b);
	free(is->s);
	free(is->name);
	if(is->fn)
		is->fn();
	free(is);
	setname();
	return 1;
}

int
getrune(void)
{
	Rune r;
	int c;

top:
	if(istack == nil)
		return -1;
	if(istack->nunget)
		return istack->unget[--istack->nunget];
	else if(istack->p){
		if(istack->p >= istack->ep){
			popinput();
			goto top;
		}
		r = *istack->p++;
	}else if(istack->b){
		if((c = Bgetrune(istack->b)) < 0){
			popinput();
			goto top;
		}
		r = c;
	}else{
		r = 0;
		sysfatal("getrune - can't happen");
	}
	if(r == '\n')
		istack->lineno++;
	return r;
}

void
ungetrune(Rune r)
{
	if(istack == nil || istack->nunget >= nelem(istack->unget))
		pushinputstring(L(""));
	istack->unget[istack->nunget++] = r;
}

int
linefmt(Fmt *f)
{
	Istack *is;

	for(is=istack; is && !is->b; is=is->next)
		;
	if(is)
		return fmtprint(f, "%S:%d", is->name, is->lineno);
	else
		return fmtprint(f, "<no input>");
}

void
setlinenumber(Rune *s, int n)
{
	Istack *is;

	for(is=istack; is && !is->name; is=is->next)
		;
	if(is){
		if(s){
			free(is->name);
			is->name = erunestrdup(s);
		}
		is->lineno = n;
	}
}
