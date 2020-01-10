#include <u.h>
#include <libc.h>
#include <mach.h>
#include "elf.h"
#include "uregamd64.h"

typedef struct Ureg Ureg;

// See FreeBSD's sys/procfs.h.

typedef struct Lreg Lreg;
typedef struct Status Status;
typedef struct Psinfo Psinfo;

struct Lreg
{
	u64int	r15;
	u64int	r14;
	u64int	r13;
	u64int	r12;
	u64int	r11;
	u64int	r10;
	u64int	r9;
	u64int	r8;
	u64int	rdi;
	u64int	rsi;
	u64int	rbp;
	u64int	rbx;
	u64int	rdx;
	u64int	rcx;
	u64int	rax;
	u32int	trapno;
	u16int	fs;
	u16int	gs;
	u32int	err;
	u16int	es;
	u16int	ds;
	u64int	rip;
	u64int	cs;
	u64int	rflags;
	u64int	rsp;
	u64int	ss;
};

struct Status
{
	u32int		version;	/* Version number of struct (1) */
	u64int		statussz;	/* sizeof(prstatus_t) (1) */
	u64int		gregsetsz;	/* sizeof(gregset_t) (1) */
	u64int		fpregsetsz;	/* sizeof(fpregset_t) (1) */
	u32int		osreldate;	/* Kernel version (1) */
	u32int		cursig;	/* Current signal (1) */
	u32int		pid;		/* Process ID (1) */
	Lreg		reg;		/* General purpose registers (1) */
};

struct Psinfo
{
	u32int	version;
	u64int	size;
	char	name[17];
	char	psargs[81];
};

void
elfcorefreebsdamd64(Fhdr *fp, Elf *elf, ElfNote *note)
{
	Status *s;
	Lreg *l;
	Ureg *u;
	int i;

	switch(note->type) {
	case ElfNotePrStatus:
		if(note->descsz < sizeof(Status)){
			fprint(2, "warning: elf status note too small\n");
			break;
		}
		s = (Status*)note->desc;
		if(s->version != 1){
			fprint(2, "warning: unknown elf note status version %ud\n", (uint)s->version);
			break;
		}
		l = &s->reg;
		u = malloc(sizeof(Ureg));

		/* no byte order problems - just copying and rearranging */
		u->ax = l->rax;
		u->bx = l->rbx;
		u->cx = l->rcx;
		u->dx = l->rdx;
		u->si = l->rsi;
		u->di = l->rdi;
		u->bp = l->rbp;
		u->r8 = l->r8;
		u->r9 = l->r9;
		u->r10 = l->r10;
		u->r11 = l->r11;
		u->r12 = l->r12;
		u->r13 = l->r13;
		u->r14 = l->r14;
		u->r15 = l->r15;

		u->ds = l->ds;
		u->es = l->es;
		u->fs = l->fs;
		u->gs = l->gs;

		u->type = l->trapno;
		u->error = l->err;
		u->ip = l->rip;
		u->cs = l->cs;
		u->flags = l->rflags;
		u->sp = l->rsp;
		u->ss = l->ss;

		if((fp->thread = realloc(fp->thread, (1+fp->nthread)*sizeof(fp->thread[0]))) == nil){
			fprint(2, "warning: out of memory saving thread info\n");
			return;
		}
		i = fp->nthread;
		fp->thread[i].id = s->pid;
		fp->thread[i].ureg = u;
		fp->nthread++;
		break;
	}
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
