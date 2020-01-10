#include <u.h>
#include <libc.h>
#include <mach.h>
#include "elf.h"
#include "ureg386.h"

#undef errno
#define errno	uregerrno

typedef struct Lreg Lreg;
typedef struct Status Status;
typedef struct Psinfo Psinfo;

/*
 * UregLinux386 is 64-bit aligned within status, so we shouldn't
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
	UregLinux386	reg;
	u32int	fpvalid;
};
enum
{
	StatusSize = sizeof(Status)
};

struct Psinfo
{
	char state;
	char sname;
	char zomb;
	char nice;
	u32int flag;
	u16int uid;
	u16int gid;
	u32int pid;
	u32int ppid;
	u32int pgrp;
	u32int sid;
	char fname[16];
	char psargs[80];
};
enum
{
	PsinfoSize = sizeof(Psinfo)
};

int
coreregslinux386(Elf *elf, ElfNote *note, uchar **up)
{
	Status *s;
	UregLinux386 *l;
	Ureg *u;

	if(note->descsz < sizeof(Status)){
		werrstr("elf status note too small");
		return -1;
	}
	s = (Status*)note->desc;
	l = &s->reg;
	if((u = malloc(sizeof *u)) == nil)
		return -1;
	linux2ureg386(l, u);
	*up = (uchar*)u;
	return sizeof(Ureg);
}

int
corecmdlinux386(Elf *elf, ElfNote *note, char **pp)
{
	char *t;
	Psinfo *p;

	*pp = nil;
	if(note->descsz < sizeof(Psinfo)){
		werrstr("elf psinfo note too small");
		return -1;
	}
	p = (Psinfo*)note->desc;
	/* print("elf name %s\nelf args %s\n", p->fname, p->psargs); */
	t = malloc(80+1);
	if(t == nil)
		return -1;
	memmove(t, p->psargs, 80);
	t[80] = 0;
	*pp = t;
	return 0;
}

#define dprint if(0)print

void
elfcorelinux386(Fhdr *fp, Elf *elf, ElfNote *note)
{
	int i;
	Psinfo *ps;
	Status *st;
	Mach *m;
	Ureg *u;

	m = fp->mach;
	dprint("%s ", note->name);
	switch(note->type){
	case ElfNotePrPsinfo:
		ps = (Psinfo*)note->desc;
		dprint("note info\n");
		dprint("state=%d sname=%d zomb=%d nice=%d\n",
			ps->state, ps->sname, ps->zomb, ps->nice);
		dprint("flag=0x%ux uid=%ud gid=%ud pid=%ud ppid=%ud pgrp=%ud sid=%ud\n",
			(uint)m->swap4(ps->flag),
			(uint)m->swap2(ps->uid),
			(uint)m->swap2(ps->gid),
			(uint)m->swap4(ps->pid),
			(uint)m->swap4(ps->ppid),
			(uint)m->swap4(ps->pgrp),
			(uint)m->swap4(ps->sid));
		dprint("fname=%s psargs=%s\n", ps->fname, ps->psargs);
		fp->pid = m->swap4(ps->pid);
		if((fp->prog = strdup(ps->fname)) == nil)
			fprint(2, "warning: out of memory saving core program name\n");
		if((fp->cmdline = strdup(ps->psargs)) == nil)
			fprint(2, "warning: out of memory saving core command line\n");
		break;
	case ElfNotePrTaskstruct:
		dprint("note taskstruct\n");
		break;
	case ElfNotePrAuxv:
		dprint("note auxv\n");
		break;
	case ElfNotePrStatus:
		dprint("note status\n");
		if(note->descsz < StatusSize){
			dprint("too small\n");
			break;
		}
		st = (Status*)note->desc;
		dprint("sig=%ud code=%ud errno=%ud cursig=%ud sigpend=0x%ux sighold=0x%ux\n",
			(uint)m->swap4(st->signo),
			(uint)m->swap4(st->code),
			(uint)m->swap4(st->errno),
			(uint)m->swap4(st->cursig),
			(uint)m->swap4(st->sigpend),
			(uint)m->swap4(st->sighold));
		dprint("pid=%ud ppid=%ud pgrp=%ud sid=%ud\n",
			(uint)m->swap4(st->pid),
			(uint)m->swap4(st->ppid),
			(uint)m->swap4(st->pgrp),
			(uint)m->swap4(st->sid));
		dprint("utime=%ud.%06ud stime=%ud.%06ud cutime=%ud.%06ud cstime=%ud.%06ud\n",
			(uint)m->swap4(st->utime[0]),
			(uint)m->swap4(st->utime[1]),
			(uint)m->swap4(st->stime[0]),
			(uint)m->swap4(st->stime[1]),
			(uint)m->swap4(st->cutime[0]),
			(uint)m->swap4(st->cutime[1]),
			(uint)m->swap4(st->cstime[0]),
			(uint)m->swap4(st->cstime[1]));
		dprint("fpvalid=%ud\n",
			(uint)m->swap4(st->fpvalid));
		if((fp->thread = realloc(fp->thread, (1+fp->nthread)*sizeof(fp->thread[0]))) == nil){
			fprint(2, "warning: out of memory saving thread info\n");
			return;
		}
		i = fp->nthread;
		fp->thread[i].id = m->swap4(st->pid);
		u = malloc(sizeof *u);
		if(u == nil){
			fprint(2, "warning: out of memory saving thread info\n");
			return;
		}
		fp->thread[i].ureg = u;
		linux2ureg386(&st->reg, u);
		fp->nthread++;
		break;
	case ElfNotePrFpreg:
		dprint("note fpreg\n");
		/* XXX maybe record floating-point registers eventually */
		break;
	case ElfNotePrXfpreg:
		dprint("note xfpreg\n");
		/* XXX maybe record floating-point registers eventually */
		break;
	default:
		dprint("note %d\n", note->type);
	}
}
