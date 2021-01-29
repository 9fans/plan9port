#include "a.h"

enum
{
	MAXREQ = 100,
	MAXRAW = 40,
	MAXESC = 60,
	MAXLINE = 1024,
	MAXIF = 20,
	MAXARG = 10
};

typedef struct Esc Esc;
typedef struct Req Req;
typedef struct Raw Raw;

/* escape sequence handler, like for \c */
struct Esc
{
	Rune r;
	int (*f)(void);
	int mode;
};

/* raw request handler, like for .ie */
struct Raw
{
	Rune *name;
	void (*f)(Rune*);
};

/* regular request handler, like for .ft */
struct Req
{
	int argc;
	Rune *name;
	void (*f)(int, Rune**);
};

int		dot = '.';
int		tick = '\'';
int		backslash = '\\';

int		inputmode;
Req		req[MAXREQ];
int		nreq;
Raw		raw[MAXRAW];
int		nraw;
Esc		esc[MAXESC];
int		nesc;
int		iftrue[MAXIF];
int		niftrue;

int isoutput;
int linepos;


void
addraw(Rune *name, void (*f)(Rune*))
{
	Raw *r;

	if(nraw >= nelem(raw)){
		fprint(2, "too many raw requets\n");
		return;
	}
	r = &raw[nraw++];
	r->name = erunestrdup(name);
	r->f = f;
}

void
delraw(Rune *name)
{
	int i;

	for(i=0; i<nraw; i++){
		if(runestrcmp(raw[i].name, name) == 0){
			if(i != --nraw){
				free(raw[i].name);
				raw[i] = raw[nraw];
			}
			return;
		}
	}
}

void
renraw(Rune *from, Rune *to)
{
	int i;

	delraw(to);
	for(i=0; i<nraw; i++)
		if(runestrcmp(raw[i].name, from) == 0){
			free(raw[i].name);
			raw[i].name = erunestrdup(to);
			return;
		}
}


void
addreq(Rune *s, void (*f)(int, Rune**), int argc)
{
	Req *r;

	if(nreq >= nelem(req)){
		fprint(2, "too many requests\n");
		return;
	}
	r = &req[nreq++];
	r->name = erunestrdup(s);
	r->f = f;
	r->argc = argc;
}

void
delreq(Rune *name)
{
	int i;

	for(i=0; i<nreq; i++){
		if(runestrcmp(req[i].name, name) == 0){
			if(i != --nreq){
				free(req[i].name);
				req[i] = req[nreq];
			}
			return;
		}
	}
}

void
renreq(Rune *from, Rune *to)
{
	int i;

	delreq(to);
	for(i=0; i<nreq; i++)
		if(runestrcmp(req[i].name, from) == 0){
			free(req[i].name);
			req[i].name = erunestrdup(to);
			return;
		}
}

void
addesc(Rune r, int (*f)(void), int mode)
{
	Esc *e;

	if(nesc >= nelem(esc)){
		fprint(2, "too many escapes\n");
		return;
	}
	e = &esc[nesc++];
	e->r = r;
	e->f = f;
	e->mode = mode;
}

/*
 * Get the next logical character in the input stream.
 */
int
getnext(void)
{
	int i, r;

next:
	r = getrune();
	if(r < 0)
		return -1;
	if(r == Uformatted){
		br();
		assert(!isoutput);
		while((r = getrune()) >= 0 && r != Uunformatted){
			if(r == Uformatted)
				continue;
			outrune(r);
		}
		goto next;
	}
	if(r == Uunformatted)
		goto next;
	if(r == backslash){
		r = getrune();
		if(r < 0)
			return -1;
		for(i=0; i<nesc; i++){
			if(r == esc[i].r && (inputmode&esc[i].mode)==inputmode){
				if(esc[i].f == e_warn)
					warn("ignoring %C%C", backslash, r);
				r = esc[i].f();
				if(r <= 0)
					goto next;
				return r;
			}
		}
		if(inputmode&(ArgMode|CopyMode)){
			ungetrune(r);
			r = backslash;
		}
	}
	return r;
}

void
ungetnext(Rune r)
{
	/*
	 * really we want to undo the getrunes that led us here,
	 * since the call after ungetnext might be getrune!
	 */
	ungetrune(r);
}

int
_readx(Rune *p, int n, int nmode, int line)
{
	int c, omode;
	Rune *e;

	while((c = getrune()) == ' ' || c == '\t')
		;
	ungetrune(c);
	omode = inputmode;
	inputmode = nmode;
	e = p+n-1;
	for(c=getnext(); p<e; c=getnext()){
		if(c < 0)
			break;
		if(!line && (c == ' ' || c == '\t'))
			break;
		if(c == '\n'){
			if(!line)
				ungetnext(c);
			break;
		}
		*p++ = c;
	}
	inputmode = omode;
	*p = 0;
	if(c < 0)
		return -1;
	return 0;
}

/*
 * Get the next argument from the current line.
 */
Rune*
copyarg(void)
{
	static Rune buf[MaxLine];
	int c;
	Rune *r;

	if(_readx(buf, MaxLine, ArgMode, 0) < 0)
		return nil;
	r = runestrstr(buf, L("\\\""));
	if(r){
		*r = 0;
		while((c = getrune()) >= 0 && c != '\n')
			;
		ungetrune('\n');
	}
	r = erunestrdup(buf);
	return r;
}

/*
 * Read the current line in given mode.  Newline not kept.
 * Uses different buffer from copyarg!
 */
Rune*
readline(int m)
{
	static Rune buf[MaxLine];
	Rune *r;

	if(_readx(buf, MaxLine, m, 1) < 0)
		return nil;
	r = erunestrdup(buf);
	return r;
}

/*
 * Given the argument line (already read in copy+arg mode),
 * parse into arguments.  Note that \" has been left in place
 * during copy+arg mode parsing, so comments still need to be stripped.
 */
int
parseargs(Rune *p, Rune **argv)
{
	int argc;
	Rune *w;

	for(argc=0; argc<MAXARG; argc++){
		while(*p == ' ' || *p == '\t')
			p++;
		if(*p == 0)
			break;
		argv[argc] = p;
		if(*p == '"'){
			/* quoted argument */
			if(*(p+1) == '"'){
				/* empty argument */
				*p = 0;
				p += 2;
			}else{
				/* parse quoted string */
				w = p++;
				for(; *p; p++){
					if(*p == '"' && *(p+1) == '"')
						*w++ = '"';
					else if(*p == '"'){
						p++;
						break;
					}else
						*w++ = *p;
				}
				*w = 0;
			}
		}else{
			/* unquoted argument - need to watch out for \" comment */
			for(; *p; p++){
				if(*p == ' ' || *p == '\t'){
					*p++ = 0;
					break;
				}
				if(*p == '\\' && *(p+1) == '"'){
					*p = 0;
					if(p != argv[argc])
						argc++;
					return argc;
				}
			}
		}
	}
	return argc;
}

/*
 * Process a dot line.  The dot has been read.
 */
void
dotline(int dot)
{
	int argc, i;
	Rune *a, *argv[1+MAXARG];

	/*
	 * Read request/macro name
	 */
	a = copyarg();
	if(a == nil || a[0] == 0){
		free(a);
		getrune();	/* \n */
		return;
	}
	argv[0] = a;
	/*
	 * Check for .if, .ie, and others with special parsing.
	 */
	for(i=0; i<nraw; i++){
		if(runestrcmp(raw[i].name, a) == 0){
			raw[i].f(raw[i].name);
			free(a);
			return;
		}
	}

	/*
	 * Read rest of line in copy mode, invoke regular request.
	 */
	a = readline(ArgMode);
	if(a == nil){
		free(argv[0]);
		return;
	}
	argc = 1+parseargs(a, argv+1);
	for(i=0; i<nreq; i++){
		if(runestrcmp(req[i].name, argv[0]) == 0){
			if(req[i].argc != -1){
				if(argc < 1+req[i].argc){
					warn("not enough arguments for %C%S", dot, req[i].name);
					free(argv[0]);
					free(a);
					return;
				}
				if(argc > 1+req[i].argc)
					warn("too many arguments for %C%S", dot, req[i].name);
			}
			req[i].f(argc, argv);
			free(argv[0]);
			free(a);
			return;
		}
	}

	/*
	 * Invoke user-defined macros.
	 */
	runmacro(dot, argc, argv);
	free(argv[0]);
	free(a);
}

/*
 * newlines are magical in various ways.
 */
int bol;
void
newline(void)
{
	int n;

	if(bol)
		sp(eval(L("1v")));
	bol = 1;
	if((n=getnr(L(".ce"))) > 0){
		nr(L(".ce"), n-1);
		br();
	}
	if(getnr(L(".fi")) == 0)
		br();
	outrune('\n');
}

void
startoutput(void)
{
	char *align;
	double ps, vs, lm, rm, ti;
	Rune buf[200];

	if(isoutput)
		return;
	isoutput = 1;

	if(getnr(L(".paragraph")) == 0)
		return;

	nr(L(".ns"), 0);
	isoutput = 1;
	ps = getnr(L(".s"));
	if(ps <= 1)
		ps = 10;
	ps /= 72.0;
	USED(ps);

	vs = getnr(L(".v"))*getnr(L(".ls")) * 1.0/UPI;
	vs /= (10.0/72.0);	/* ps */
	if(vs == 0)
		vs = 1.2;

	lm = (getnr(L(".o"))+getnr(L(".i"))) * 1.0/UPI;
	ti = getnr(L(".ti")) * 1.0/UPI;
	nr(L(".ti"), 0);

	rm = 8.0 - getnr(L(".l"))*1.0/UPI - getnr(L(".o"))*1.0/UPI;
	if(rm < 0)
		rm = 0;
	switch(getnr(L(".j"))){
	default:
	case 0:
		align = "left";
		break;
	case 1:
		align = "justify";
		break;
	case 3:
		align = "center";
		break;
	case 5:
		align = "right";
		break;
	}
	if(getnr(L(".ce")))
		align = "center";
	if(!getnr(L(".margin")))
		runesnprint(buf, nelem(buf), "<p style=\"line-height: %.1fem; text-indent: %.2fin; margin-top: 0; margin-bottom: 0; text-align: %s;\">\n",
			vs, ti, align);
	else
		runesnprint(buf, nelem(buf), "<p style=\"line-height: %.1fem; margin-left: %.2fin; text-indent: %.2fin; margin-right: %.2fin; margin-top: 0; margin-bottom: 0; text-align: %s;\">\n",
			vs, lm, ti, rm, align);
	outhtml(buf);
}
void
br(void)
{
	if(!isoutput)
		return;
	isoutput = 0;

	nr(L(".dv"), 0);
	dv(0);
	hideihtml();
	if(getnr(L(".paragraph")))
		outhtml(L("</p>"));
}

void
r_margin(int argc, Rune **argv)
{
	USED(argc);

	nr(L(".margin"), eval(argv[1]));
}

int inrequest;
void
runinput(void)
{
	int c;

	bol = 1;
	for(;;){
		c = getnext();
		if(c < 0)
			break;
		if((c == dot || c == tick) && bol){
			inrequest = 1;
			dotline(c);
			bol = 1;
			inrequest = 0;
		}else if(c == '\n'){
			newline();
			itrap();
			linepos = 0;
		}else{
			outtrap();
			startoutput();
			showihtml();
			if(c == '\t'){
				/* XXX do better */
				outrune(' ');
				while(++linepos%4)
					outrune(' ');
			}else{
				outrune(c);
				linepos++;
			}
			bol = 0;
		}
	}
}

void
run(void)
{
	t1init();
	t2init();
	t3init();
	t4init();
	t5init();
	t6init();
	t7init();
	t8init();
	/* t9init(); t9.c */
	t10init();
	t11init();
	/* t12init(); t12.c */
	t13init();
	t14init();
	t15init();
	t16init();
	t17init();
	t18init();
	t19init();
	t20init();
	htmlinit();
	hideihtml();

	addreq(L("margin"), r_margin, 1);
	nr(L(".margin"), 1);
	nr(L(".paragraph"), 1);

	runinput();
	while(popinput())
		;
	dot = '.';
	if(verbose)
		fprint(2, "eof\n");
	runmacro1(L("eof"));
	closehtml();
}

void
out(Rune *s)
{
	if(s == nil)
		return;
	for(; *s; s++)
		outrune(*s);
}

void (*outcb)(Rune);

void
inroman(Rune r)
{
	int f;

	f = getnr(L(".f"));
	nr(L(".f"), 1);
	runmacro1(L("font"));
	outrune(r);
	nr(L(".f"), f);
	runmacro1(L("font"));
}

void
Brune(Rune r)
{
	if(r == '&')
		Bprint(&bout, "&amp;");
	else if(r == '<')
		Bprint(&bout, "&lt;");
	else if(r == '>')
		Bprint(&bout, "&gt;");
	else if(r < Runeself || utf8)
		Bprint(&bout, "%C", r);
	else
		Bprint(&bout, "%S", rune2html(r));
}

void
outhtml(Rune *s)
{
	Rune r;

	for(; *s; s++){
		switch(r = *s){
		case '<':
			r = Ult;
			break;
		case '>':
			r = Ugt;
			break;
		case '&':
			r = Uamp;
			break;
		case ' ':
			r = Uspace;
			break;
		}
		outrune(r);
	}
}

void
outrune(Rune r)
{
	switch(r){
	case ' ':
		if(getnr(L(".fi")) == 0)
			r = Unbsp;
		break;
	case Uformatted:
	case Uunformatted:
		abort();
	}
	if(outcb){
		if(r == ' ')
			r = Uspace;
		outcb(r);
		return;
	}
	/* writing to bout */
	switch(r){
	case Uempty:
		return;
	case Upl:
		inroman('+');
		return;
	case Ueq:
		inroman('=');
		return;
	case Umi:
		inroman(0x2212);
		return;
	case Utick:
		r = '\'';
		break;
	case Ubtick:
		r = '`';
		break;
	case Uminus:
		r = '-';
		break;
	case '\'':
		Bprint(&bout, "&rsquo;");
		return;
	case '`':
		Bprint(&bout, "&lsquo;");
		return;
	case Uamp:
		Bputrune(&bout, '&');
		return;
	case Ult:
		Bputrune(&bout, '<');
		return;
	case Ugt:
		Bputrune(&bout, '>');
		return;
	case Uspace:
		Bputrune(&bout, ' ');
		return;
	case 0x2032:
		/*
		 * In Firefox, at least, the prime is not
		 * a superscript by default.
		 */
		Bprint(&bout, "<sup>");
		Brune(r);
		Bprint(&bout, "</sup>");
		return;
	}
	Brune(r);
}

void
r_nop(int argc, Rune **argv)
{
	USED(argc);
	USED(argv);
}

void
r_warn(int argc, Rune **argv)
{
	USED(argc);
	warn("ignoring %C%S", dot, argv[0]);
}

int
e_warn(void)
{
	/* dispatch loop prints a warning for us */
	return 0;
}

int
e_nop(void)
{
	return 0;
}
