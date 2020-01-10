#include <u.h>
#include <libc.h>
#include <mach.h>
#include "elf.h"
#include "ureg386.h"

typedef struct Lreg Lreg;
typedef struct Status Status;
typedef struct Psinfo Psinfo;

struct Lreg
{
	u32int	fs;
	u32int	es;
	u32int	ds;
	u32int	edi;
	u32int	esi;
	u32int	ebp;
	u32int	isp;
	u32int	ebx;
	u32int	edx;
	u32int	ecx;
	u32int	eax;
	u32int	trapno;
	u32int	err;
	u32int	eip;
	u32int	cs;
	u32int	eflags;
	u32int	esp;
	u32int	ss;
	u32int	gs;
};

struct Status
{
	u32int		version;	/* Version number of struct (1) */
	u32int		statussz;	/* sizeof(prstatus_t) (1) */
	u32int		gregsetsz;	/* sizeof(gregset_t) (1) */
	u32int		fpregsetsz;	/* sizeof(fpregset_t) (1) */
	u32int		osreldate;	/* Kernel version (1) */
	u32int		cursig;	/* Current signal (1) */
	u32int		pid;		/* Process ID (1) */
	Lreg		reg;		/* General purpose registers (1) */
};

struct Psinfo
{
	u32int	version;
	u32int	size;
	char	name[17];
	char	psargs[81];
};

int
coreregsfreebsd386(Elf *elf, ElfNote *note, uchar **up)
{
	Status *s;
	Lreg *l;
	Ureg *u;

	if(note->descsz < sizeof(Status)){
		werrstr("elf status note too small");
		return -1;
	}
	s = (Status*)note->desc;
	if(s->version != 1){
		werrstr("unknown status version %ud", (uint)s->version);
		return -1;
	}
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
	u->trap = l->trapno;
	u->ecode = l->err;
	u->pc = l->eip;
	u->cs = l->cs;
	u->flags = l->eflags;
	u->sp = l->esp;
	u->ss = l->ss;
	*up = (uchar*)u;
	return sizeof(Ureg);
}

int
corecmdfreebsd386(Elf *elf, ElfNote *note, char **pp)
{
	char *t;
	Psinfo *p;

	*pp = nil;
	if(note->descsz < sizeof(Psinfo)){
		werrstr("elf psinfo note too small");
		return -1;
	}
	p = (Psinfo*)note->desc;
	/* print("elf name %s\nelf args %s\n", p->name, p->psargs); */
	t = malloc(80+1);
	if(t == nil)
		return -1;
	memmove(t, p->psargs, 80);
	t[80] = 0;
	*pp = t;
	return 0;
}
