#include <u.h>
#include <libc.h>
#include <mach.h>
#include <elf.h>
#include "ureg386.h"


Ureg*
_linux2ureg386(UregLinux386 *l)
{
	Ureg *u;

	u = malloc(sizeof(Ureg));
	if(u == nil)
		return nil;
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
	u->trap = ~0; // l->trapno;
	u->ecode = ~0; // l->err;
	u->pc = l->eip;
	u->cs = l->xcs;
	u->flags = l->eflags;
	u->sp = l->esp;
	u->ss = l->xss;
	return u;
}
