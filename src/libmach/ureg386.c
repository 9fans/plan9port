#include <u.h>
#include <libc.h>
#include <mach.h>
#include "ureg386.h"

void
linux2ureg386(UregLinux386 *l, Ureg *u)
{
	u->di = l->edi;
	u->si = l->esi;
	u->bp = l->ebp;
	u->nsp = l->esp;
	u->bx = l->ebx;
	u->dx = l->edx;
	u->cx = l->ecx;
	u->ax = l->eax;
	u->gs = l->xgs;
	u->fs = l->xfs;
	u->es = l->xes;
	u->ds = l->xds;
	u->trap = ~0; /* l->trapno; */
	u->ecode = ~0; /* l->err; */
	u->pc = l->eip;
	u->cs = l->xcs;
	u->flags = l->eflags;
	u->sp = l->esp;
	u->ss = l->xss;
}

void
ureg2linux386(Ureg *u, UregLinux386 *l)
{
	l->edi = u->di;
	l->esi = u->si;
	l->ebp = u->bp;
	l->esp = u->nsp;
	l->ebx = u->bx;
	l->edx = u->dx;
	l->ecx = u->cx;
	l->eax = u->ax;
	l->xgs = u->gs;
	l->xfs = u->fs;
	l->xes = u->es;
	l->xds = u->ds;
	l->eip = u->pc;
	l->xcs = u->cs;
	l->eflags = u->flags;
	l->esp = u->sp;
	l->xss = u->ss;
}
