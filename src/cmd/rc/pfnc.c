#include "rc.h"
#include "exec.h"
#include "io.h"
#include "fns.h"
struct{
	void (*f)(void);
	char *name;
}fname[] = {
	Xappend, "Xappend",
	Xasync, "Xasync",
	Xbang, "Xbang",
	Xclose, "Xclose",
	Xdup, "Xdup",
	Xeflag, "Xeflag",
	Xexit, "Xexit",
	Xfalse, "Xfalse",
	Xifnot, "Xifnot",
	Xjump, "Xjump",
	Xmark, "Xmark",
	Xpopm, "Xpopm",
	Xpush, "Xpush",
	Xrdwr, "Xrdwr",
	Xread, "Xread",
	Xhere, "Xhere",
	Xhereq, "Xhereq",
	Xreturn, "Xreturn",
	Xtrue, "Xtrue",
	Xif, "Xif",
	Xwastrue, "Xwastrue",
	Xword, "Xword",
	Xwrite, "Xwrite",
	Xmatch, "Xmatch",
	Xcase, "Xcase",
	Xconc, "Xconc",
	Xassign, "Xassign",
	Xdol, "Xdol",
	Xcount, "Xcount",
	Xlocal, "Xlocal",
	Xunlocal, "Xunlocal",
	Xfn, "Xfn",
	Xdelfn, "Xdelfn",
	Xpipe, "Xpipe",
	Xpipewait, "Xpipewait",
	Xpopredir, "Xpopredir",
	Xrdcmds, "Xrdcmds",
	Xbackq, "Xbackq",
	Xpipefd, "Xpipefd",
	Xsubshell, "Xsubshell",
	Xfor, "Xfor",
	Xglob, "Xglob",
	Xsimple, "Xsimple",
	Xqw, "Xqw",
	Xsrcline, "Xsrcline",
0};

void
pfun(io *f, void (*fn)(void))
{
	int i;
	for(i = 0;fname[i].f;i++) if(fname[i].f==fn){
		pstr(f, fname[i].name);
		return;
	}
	pfmt(f, "%p", fn);
}

void
pfnc(io *f, thread *t)
{
	list *a;

	pfln(f, srcfile(t), t->line);
	pfmt(f, " pid %d cycle %p %d ", getpid(), t->code, t->pc);
	pfun(f, t->code[t->pc].f);
	for(a = t->argv;a;a = a->next) pfmt(f, " (%v)", a->words);
	pchr(f, '\n');
	flushio(f);
}
