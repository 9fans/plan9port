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
	if((u = _linux2ureg386(l)) == nil)
		return -1;
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
	// print("elf name %s\nelf args %s\n", p->fname, p->psargs);
	t = malloc(80+1);
	if(t == nil)
		return -1;
	memmove(t, p->psargs, 80);
	t[80] = 0;
	*pp = t;
	return 0;
}

