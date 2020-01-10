#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include <mach.h>
#define Extern extern
#include "acid.h"
#include "y.tab.h"

static void install(int);

void
sproc(int xpid)
{
	Lsym *s;
	int i;
	Regs *regs;

	if(symmap == 0)
		error("no map");

	if(pid == xpid)
		return;

	if(corhdr){
		regs = coreregs(corhdr, xpid);
		if(regs == nil)
			error("no such pid in core dump");
		free(correg);
		correg = regs;
	}else{
		/* XXX should only change register set here if cormap already mapped */
		if(xpid <= 0)
			error("bad pid");
		unmapproc(cormap);
		unmapfile(corhdr, cormap);
		free(correg);
		correg = nil;
		pid = -1;
		corpid = -1;

		if(mapproc(xpid, cormap, &correg) < 0)
			error("setproc %d: %r", xpid);

		/* XXX check text file here? */

		for(i=0; i<cormap->nseg; i++){
			if(cormap->seg[i].file == nil){
				if(strcmp(cormap->seg[i].name, "data") == 0)
					cormap->seg[i].name = "*data";
				if(strcmp(cormap->seg[i].name, "text") == 0)
					cormap->seg[i].name = "*text";
			}
		}
	}
	pid = xpid;
	corpid = pid;
	s = look("pid");
	s->v->store.u.ival = pid;

	install(pid);
}

int
nproc(char **argv)
{
	int pid, i;

	pid = fork();
	switch(pid) {
	case -1:
		error("new: fork %r");
	case 0:
		rfork(RFNAMEG|RFNOTEG);
		if(ctlproc(getpid(), "hang") < 0)
			fatal("new: hang %d: %r", getpid());

		close(0);
		close(1);
		close(2);
		for(i = 3; i < NFD; i++)
			close(i);

		open("/dev/tty", OREAD);
		open("/dev/tty", OWRITE);
		open("/dev/tty", OWRITE);
		execv(argv[0], argv);
		fatal("new: exec %s: %r", argv[0]);
	default:
		install(pid);
		msg(pid, "attached");
		msg(pid, "waitstop");
		notes(pid);
		sproc(pid);
		dostop(pid);
		break;
	}

	return pid;
}

void
notes(int pid)
{
	Lsym *s;
	Value *v;
	int i, n;
	char **notes;
	List *l, **tail;

	s = look("notes");
	if(s == 0)
		return;

	v = s->v;
	n = procnotes(pid, &notes);
	if(n < 0)
		error("procnotes pid=%d: %r", pid);

	v->set = 1;
	v->type = TLIST;
	v->store.u.l = 0;
	tail = &v->store.u.l;
	for(i=0; i<n; i++) {
		l = al(TSTRING);
		l->store.u.string = strnode(notes[i]);
		l->store.fmt = 's';
		*tail = l;
		tail = &l->next;
	}
	free(notes);
}

void
dostop(int pid)
{
	Lsym *s;
	Node *np, *p;

	s = look("stopped");
	if(s && s->proc) {
		np = an(ONAME, ZN, ZN);
		np->sym = s;
		np->store.fmt = 'D';
		np->type = TINT;
		p = con(pid);
		p->store.fmt = 'D';
		np = an(OCALL, np, p);
		execute(np);
	}
}

static void
install(int pid)
{
	Lsym *s;
	List *l;
	int i, new, p;

	new = -1;
	for(i = 0; i < Maxproc; i++) {
		p = ptab[i].pid;
		if(p == pid)
			return;
		if(p == 0 && new == -1)
			new = i;
	}
	if(new == -1)
		error("no free process slots");

	ptab[new].pid = pid;

	s = look("proclist");
	l = al(TINT);
	l->store.fmt = 'D';
	l->store.u.ival = pid;
	l->next = s->v->store.u.l;
	s->v->store.u.l = l;
	s->v->set = 1;
}

/*
static int
installed(int pid)
{
	int i;

	for(i=0; i<Maxproc; i++)
		if(ptab[i].pid == pid)
			return 1;
	return 0;
}
*/

void
deinstall(int pid)
{
	int i;
	Lsym *s;
	List *f, **d;

	for(i = 0; i < Maxproc; i++) {
		if(ptab[i].pid == pid) {
			detachproc(pid);
			/* close(ptab[i].ctl); */
			ptab[i].pid = 0;
			s = look("proclist");
			d = &s->v->store.u.l;
			for(f = *d; f; f = f->next) {
				if(f->store.u.ival == pid) {
					*d = f->next;
					break;
				}
			}
			s = look("pid");
			if(s->v->store.u.ival == pid)
				s->v->store.u.ival = 0;
			return;
		}
	}
}

void
msg(int pid, char *msg)
{
	int i;
	char err[ERRMAX];

	for(i = 0; i < Maxproc; i++) {
		if(ptab[i].pid == pid) {
			if(ctlproc(pid, msg) < 0){
				errstr(err, sizeof err);
				if(strcmp(err, "process exited") == 0)
					deinstall(pid);
				error("msg: pid=%d %s: %s", pid, msg, err);
			}
			return;
		}
	}
	error("msg: pid=%d: not found for %s", pid, msg);
}

char *
getstatus(int pid)
{
	return "unknown";
}
