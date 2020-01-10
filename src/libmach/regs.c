#include <u.h>
#include <libc.h>
#include <mach.h>

Regdesc*
regdesc(char *name)
{
	Regdesc *r;

	for(r=mach->reglist; r->name; r++)
		if(strcmp(r->name, name) == 0)
			return r;
	return nil;
}

int
rput(Regs *regs, char *name, u64int u)
{
	if(regs == nil){
		werrstr("registers not mapped");
		return -1;
	}
	return regs->rw(regs, name, &u, 0);
}

int
rget(Regs *regs, char *name, u64int *u)
{
	if(regs == nil){
		*u = ~(u64int)0;
		werrstr("registers not mapped");
		return -1;
	}
	return regs->rw(regs, name, u, 1);
}

int
_uregrw(Regs *regs, char *name, u64int *u, int isr)
{
	Regdesc *r;
	uchar *ureg;

	if(!isr){
		werrstr("cannot write registers");
		return -1;
	}

	if((r = regdesc(name)) == nil)
		return -1;
	ureg = ((UregRegs*)regs)->ureg + r->offset;

	switch(r->format){
	default:
	case 'X':
		*u = mach->swap4(*(u32int*)ureg);
		return 0;
	case 'Y':
		*u = mach->swap8(*(u64int*)ureg);
		return 0;
	}
}
