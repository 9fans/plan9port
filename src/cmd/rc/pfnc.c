#include "rc.h"
#include "exec.h"
#include "io.h"
#include "fns.h"
struct{
	void (*f)(void);
	char *name;
}fname[]={
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
	Xread, "Xread",
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
	Xrdcmds, "Xrdcmds",
	(void (*)(void))Xerror, "Xerror",
	Xbackq, "Xbackq",
	Xpipefd, "Xpipefd",
	Xsubshell, "Xsubshell",
	Xdelhere, "Xdelhere",
	Xfor, "Xfor",
	Xglob, "Xglob",
	Xrdfn, "Xrdfn",
	Xsimple, "Xsimple",
	Xrdfn, "Xrdfn",
	Xqdol, "Xqdol",
0};
void pfnc(io *fd, thread *t)
{
	int i;
	void (*fn)(void)=t->code[t->pc].f;
	list *a;
	pfmt(fd, "pid %d cycle %p %d ", getpid(), t->code, t->pc);
	for(i=0;fname[i].f;i++) if(fname[i].f==fn){
		pstr(fd, fname[i].name);
		break;
	}
	if(!fname[i].f) pfmt(fd, "%p", fn);
	for(a=t->argv;a;a=a->next) pfmt(fd, " (%v)", a->words);
	pchr(fd, '\n');
	flush(fd);
}
