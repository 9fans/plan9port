#include <u.h>
#include <libc.h>
#include <mach.h>
#include "elf.h"
#include "ureg386.h"

typedef struct Lreg Lreg;
typedef struct Status Status;

struct Lreg
{
	u32int	ebx;
	u32int	ecx;
	u32int	edx;
	u32int	esi;
	u32int	edi;
	u32int	ebp;
	u32int	eax;
	u32int	ds;
	u32int	es;
	u32int	fs;
	u32int	gs;
	u32int	origeax;
	u32int	eip;
	u32int	cs;
	u32int	eflags;
	u32int	esp;
	u32int	ss;
};

/*
 * Lreg is 64-bit aligned within status, so we shouldn't 
 * have any packing problems. 
 */
struct Status
{
	u32int	signo;
	u32int	code;
	u32int	errno;
	u32int	cursig;
	u32int	sigpend;
	u32int	sighold;
	u32int	pid;
	u32int	ppid;
	u32int	pgrp;
	u32int	sid;
	u32int	utime[2];
	u32int	stime[2];
	u32int	cutime[2];
	u32int	cstime[2];
	Lreg	reg;
	u32int	fpvalid;
};

int
coreregslinux386(Elf *elf, ElfNote *note, uchar **up)
{
	Status *s;
	Lreg *l;
	Ureg *u;

	if(note->descsz < sizeof(Status)){
		werrstr("elf status note too small");
		return -1;
	}
	s = (Status*)note->desc;
	l = &s->reg;
	u = malloc(sizeof(Ureg));
	if(u == nil)
		return -1;

	/* no byte order problems - just copying and rearranging */
	u->di = l->edi;
	u->si = l->esi;
	u->bp = l->ebp;
	u->nsp = l->esp;
	u->bx = l->ebx;
	u->dx = l->edx;
	u->cx = l->ecx;
	u->ax = l->eax;
	u->gs = l->gs;
	u->fs = l->fs;
	u->es = l->es;
	u->ds = l->ds;
	u->trap = ~0; // l->trapno;
	u->ecode = ~0; // l->err;
	u->pc = l->eip;
	u->cs = l->cs;
	u->flags = l->eflags;
	u->sp = l->esp;
	u->ss = l->ss;
	*up = (uchar*)u;
	return sizeof(Ureg);
}

