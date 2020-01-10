/*
 * 7.  Macros, strings, diversion, and position traps.
 *
 * 	macros can override builtins
 *	builtins can be renamed or removed!
 */

#include "a.h"

enum
{
	MAXARG = 10,
	MAXMSTACK = 40
};

/* macro invocation frame */
typedef struct Mac Mac;
struct Mac
{
	int argc;
	Rune *argv[MAXARG];
};

Mac		mstack[MAXMSTACK];
int		nmstack;
void		emitdi(void);
void		flushdi(void);

/*
 * Run a user-defined macro.
 */
void popmacro(void);
int
runmacro(int dot, int argc, Rune **argv)
{
	Rune *p;
	int i;
	Mac *m;

if(verbose && isupperrune(argv[0][0])) fprint(2, "run: %S\n", argv[0]);
	p = getds(argv[0]);
	if(p == nil){
		if(verbose)
			warn("ignoring unknown request %C%S", dot, argv[0]);
		if(verbose > 1){
			for(i=0; i<argc; i++)
				fprint(2, " %S", argv[i]);
			fprint(2, "\n");
		}
		return -1;
	}
	if(nmstack >= nelem(mstack)){
		fprint(2, "%L: macro stack overflow:");
		for(i=0; i<nmstack; i++)
			fprint(2, " %S", mstack[i].argv[0]);
		fprint(2, "\n");
		return -1;
	}
	m = &mstack[nmstack++];
	m->argc = argc;
	for(i=0; i<argc; i++)
		m->argv[i] = erunestrdup(argv[i]);
	pushinputstring(p);
	nr(L(".$"), argc-1);
	inputnotify(popmacro);
	return 0;
}

void
popmacro(void)
{
	int i;
	Mac *m;

	if(--nmstack < 0){
		fprint(2, "%L: macro stack underflow\n");
		return;
	}
	m = &mstack[nmstack];
	for(i=0; i<m->argc; i++)
		free(m->argv[i]);
	if(nmstack > 0)
		nr(L(".$"), mstack[nmstack-1].argc-1);
	else
		nr(L(".$"), 0);
}

void popmacro1(void);
jmp_buf runjb[10];
int nrunjb;

void
runmacro1(Rune *name)
{
	Rune *argv[2];
	int obol;

if(verbose) fprint(2, "outcb %p\n", outcb);
	obol = bol;
	argv[0] = name;
	argv[1] = nil;
	bol = 1;
	if(runmacro('.', 1, argv) >= 0){
		inputnotify(popmacro1);
		if(!setjmp(runjb[nrunjb++]))
			runinput();
		else
			if(verbose) fprint(2, "finished %S\n", name);
	}
	bol = obol;
}

void
popmacro1(void)
{
	popmacro();
	if(nrunjb >= 0)
		longjmp(runjb[--nrunjb], 1);
}

/*
 * macro arguments
 *
 *	"" means " inside " "
 *	"" empty string
 *	\newline can be done
 *	argument separator is space (not tab)
 *	number register .$ = number of arguments
 *	no arguments outside macros or in strings
 *
 *	arguments copied in copy mode
 */

/*
 * diversions
 *
 *	processed output diverted
 *	dn dl registers vertical and horizontal size of last diversion
 *	.z - current diversion name
 */

/*
 * traps
 *
 *	skip most
 *	.t register - distance to next trap
 */
static Rune *trap0;

void
outtrap(void)
{
	Rune *t;

	if(outcb)
		return;
	if(trap0){
if(verbose) fprint(2, "trap: %S\n", trap0);
		t = trap0;
		trap0 = nil;
		runmacro1(t);
		free(t);
	}
}

/* .wh - install trap */
void
r_wh(int argc, Rune **argv)
{
	int i;

	if(argc < 2)
		return;

	i = eval(argv[1]);
	if(argc == 2){
		if(i == 0){
			free(trap0);
			trap0 = nil;
		}else
			if(verbose)
				warn("not removing trap at %d", i);
	}
	if(argc > 2){
		if(i == 0){
			free(trap0);
			trap0 = erunestrdup(argv[2]);
		}else
			if(verbose)
				warn("not installing %S trap at %d", argv[2], i);
	}
}

void
r_ch(int argc, Rune **argv)
{
	int i;

	if(argc == 2){
		if(trap0 && runestrcmp(argv[1], trap0) == 0){
			free(trap0);
			trap0 = nil;
		}else
			if(verbose)
				warn("not removing %S trap", argv[1]);
		return;
	}
	if(argc >= 3){
		i = eval(argv[2]);
		if(i == 0){
			free(trap0);
			trap0 = erunestrdup(argv[1]);
		}else
			if(verbose)
				warn("not moving %S trap to %d", argv[1], i);
	}
}

void
r_dt(int argc, Rune **argv)
{
	USED(argc);
	USED(argv);
	warn("ignoring diversion trap");
}

/* define macro - .de, .am, .ig */
void
r_de(int argc, Rune **argv)
{
	Rune *end, *p;
	Fmt fmt;
	int ignore, len;

	delreq(argv[1]);
	delraw(argv[1]);
	ignore = runestrcmp(argv[0], L("ig")) == 0;
	if(!ignore)
		runefmtstrinit(&fmt);
	end = L("..");
	if(argc >= 3)
		end = argv[2];
	if(runestrcmp(argv[0], L("am")) == 0 && (p=getds(argv[1])) != nil)
		fmtrunestrcpy(&fmt, p);
	len = runestrlen(end);
	while((p = readline(CopyMode)) != nil){
		if(runestrncmp(p, end, len) == 0
		&& (p[len]==' ' || p[len]==0 || p[len]=='\t'
			|| (p[len]=='\\' && p[len+1]=='}'))){
			free(p);
			goto done;
		}
		if(!ignore)
			fmtprint(&fmt, "%S\n", p);
		free(p);
	}
	warn("eof in %C%S %S - looking for %#Q", dot, argv[0], argv[1], end);
done:
	if(ignore)
		return;
	p = runefmtstrflush(&fmt);
	if(p == nil)
		sysfatal("out of memory");
	ds(argv[1], p);
	free(p);
}

/* define string .ds .as */
void
r_ds(Rune *cmd)
{
	Rune *name, *line, *p;

	name = copyarg();
	line = readline(CopyMode);
	if(name == nil || line == nil){
		free(name);
		return;
	}
	p = line;
	if(*p == '"')
		p++;
	if(cmd[0] == 'd')
		ds(name, p);
	else
		as(name, p);
	free(name);
	free(line);
}

/* remove request, macro, or string */
void
r_rm(int argc, Rune **argv)
{
	int i;

	emitdi();
	for(i=1; i<argc; i++){
		delreq(argv[i]);
		delraw(argv[i]);
		ds(argv[i], nil);
	}
}

/* .rn - rename request, macro, or string */
void
r_rn(int argc, Rune **argv)
{
	USED(argc);
	renreq(argv[1], argv[2]);
	renraw(argv[1], argv[2]);
	ds(argv[2], getds(argv[1]));
	ds(argv[1], nil);
}

/* .di - divert output to macro xx */
/* .da - divert, appending to macro */
/* page offsetting is not done! */
Fmt difmt;
int difmtinit;
Rune di[20][100];
int ndi;

void
emitdi(void)
{
	flushdi();
	runefmtstrinit(&difmt);
	difmtinit = 1;
	fmtrune(&difmt, Uformatted);
}

void
flushdi(void)
{
	int n;
	Rune *p;

	if(ndi == 0 || difmtinit == 0)
		return;
	fmtrune(&difmt, Uunformatted);
	p = runefmtstrflush(&difmt);
	memset(&difmt, 0, sizeof difmt);
	difmtinit = 0;
	if(p == nil)
		warn("out of memory in diversion %C%S", dot, di[ndi-1]);
	else{
		n = runestrlen(p);
		if(n > 0 && p[n-1] != '\n'){
			p = runerealloc(p, n+2);
			p[n] = '\n';
			p[n+1] = 0;
		}
	}
	as(di[ndi-1], p);
	free(p);
}

void
outdi(Rune r)
{
if(!difmtinit) abort();
	if(r == Uempty)
		return;
	fmtrune(&difmt, r);
}

/* .di, .da */
void
r_di(int argc, Rune **argv)
{
	br();
	if(argc > 2)
		warn("extra arguments to %C%S", dot, argv[0]);
	if(argc == 1){
		/* end diversion */
		if(ndi <= 0){
			/* warn("unmatched %C%S", dot, argv[0]); */
			return;
		}
		flushdi();
		if(--ndi == 0){
			_nr(L(".z"), nil);
			outcb = nil;
		}else{
			_nr(L(".z"), di[ndi-1]);
			runefmtstrinit(&difmt);
			fmtrune(&difmt, Uformatted);
			difmtinit = 1;
		}
		return;
	}
	/* start diversion */
	/* various register state should be saved, but it's all useless to us */
	flushdi();
	if(ndi >= nelem(di))
		sysfatal("%Cdi overflow", dot);
	if(argv[0][1] == 'i')
		ds(argv[1], nil);
	_nr(L(".z"), argv[1]);
	runestrcpy(di[ndi++], argv[1]);
	runefmtstrinit(&difmt);
	fmtrune(&difmt, Uformatted);
	difmtinit = 1;
	outcb = outdi;
}

/* .wh - install trap */
/* .ch - change trap */
/* .dt - install diversion trap */

/* set input-line count trap */
int itrapcount;
int itrapwaiting;
Rune *itrapname;

void
r_it(int argc, Rune **argv)
{
	if(argc < 3){
		itrapcount = 0;
		return;
	}
	itrapcount = eval(argv[1]);
	free(itrapname);
	itrapname = erunestrdup(argv[2]);
}

void
itrap(void)
{
	itrapset();
	if(itrapwaiting){
		itrapwaiting = 0;
		runmacro1(itrapname);
	}
}

void
itrapset(void)
{
	if(itrapcount > 0 && --itrapcount == 0)
		itrapwaiting = 1;
}

/* .em - invoke macro when all input is over */
void
r_em(int argc, Rune **argv)
{
	Rune buf[20];

	USED(argc);
	runesnprint(buf, nelem(buf), ".%S\n", argv[1]);
	as(L("eof"), buf);
}

int
e_star(void)
{
	Rune *p;

	p = getds(getname());
	if(p)
		pushinputstring(p);
	return 0;
}

int
e_t(void)
{
	if(inputmode&CopyMode)
		return '\t';
	return 0;
}

int
e_a(void)
{
	if(inputmode&CopyMode)
		return '\a';
	return 0;
}

int
e_backslash(void)
{
	if(inputmode&ArgMode)
		ungetrune('\\');
	return backslash;
}

int
e_dot(void)
{
	return '.';
}

int
e_dollar(void)
{
	int c;

	c = getnext();
	if(c < '1' || c > '9'){
		ungetnext(c);
		return 0;
	}
	c -= '0';
	if(nmstack <= 0 || mstack[nmstack-1].argc <= c)
		return 0;
	pushinputstring(mstack[nmstack-1].argv[c]);
	return 0;
}

void
t7init(void)
{
	addreq(L("de"), r_de, -1);
	addreq(L("am"), r_de, -1);
	addreq(L("ig"), r_de, -1);
	addraw(L("ds"), r_ds);
	addraw(L("as"), r_ds);
	addreq(L("rm"), r_rm, -1);
	addreq(L("rn"), r_rn, -1);
	addreq(L("di"), r_di, -1);
	addreq(L("da"), r_di, -1);
	addreq(L("it"), r_it, -1);
	addreq(L("em"), r_em, 1);
	addreq(L("wh"), r_wh, -1);
	addreq(L("ch"), r_ch, -1);
	addreq(L("dt"), r_dt, -1);

	addesc('$', e_dollar, CopyMode|ArgMode|HtmlMode);
	addesc('*', e_star, CopyMode|ArgMode|HtmlMode);
	addesc('t', e_t, CopyMode|ArgMode);
	addesc('a', e_a, CopyMode|ArgMode);
	addesc('\\', e_backslash, ArgMode|CopyMode);
	addesc('.', e_dot, CopyMode|ArgMode);

	ds(L("eof"), L(".sp 0.5i\n"));
	ds(L(".."), L(""));
}
