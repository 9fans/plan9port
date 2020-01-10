#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>

typedef struct LocRegs LocRegs;
struct LocRegs
{
	Regs r;
	Regs *oldregs;
	Map *map;
	u64int *val;
};

static int
locregrw(Regs *regs, char *name, u64int *val, int isr)
{
	int i;
	LocRegs *lr;

	lr = (LocRegs*)regs;
	i = windindex(name);
	if(i == -1)
		return lr->oldregs->rw(lr->oldregs, name, val, isr);
	if(isr){
		*val = lr->val[i];
		return 0;
	}else{
		werrstr("saved registers are immutable");
		return -1;
	}
}

int
stacktrace(Map *map, Regs *regs, Tracer trace)
{
	char *rname;
	int i, ipc, ret;
	u64int nextpc, pc, v;
	u64int *cur, *next;
	LocRegs lr;
	Symbol s, *sp;

	/*
	 * Allocate location arrays.
	 */
	ret = -1;
	cur = malloc(mach->nwindreg*sizeof(cur[0]));
	next = malloc(mach->nwindreg*sizeof(cur[0]));
	if(cur==nil || next==nil)
		goto out;

	/*
	 * Initialize current registers using regs.
	 */
	if(rget(regs, mach->pc, &pc) < 0){
		werrstr("cannot fetch initial pc: %r");
		goto out;
	}

	for(i=0; i<mach->nwindreg; i++){
		rname = mach->windreg[i];
		if(rget(regs, rname, &v) < 0)
			v = ~(ulong)0;
		cur[i] = v;
	}

	ipc = windindex(mach->pc);
	ret = 0;

	/* set up cur[i]==next[i] for unwindframe */
	memmove(next, cur, mach->nwindreg*sizeof(next[0]));
	for(;;){
		sp = &s;
		if(findsym(locaddr(pc), CTEXT, &s) < 0)
			sp = nil;

		lr.r.rw = locregrw;
		lr.oldregs = regs;
		lr.val = cur;
		lr.map = map;
		if((i = unwindframe(map, &lr.r, next, sp)) >= 0)
			nextpc = next[ipc];
		else
			nextpc = ~(ulong)0;
		if((*trace)(map, &lr.r, pc, nextpc, sp, ++ret) <= 0)
			break;
		if(i < 0)
			break;
		if(sp){
			if(strcmp(sp->name, "main") == 0
			|| strcmp(sp->name, "procscheduler") == 0
			|| strcmp(sp->name, "threadstart") == 0)
				break;
		}
		pc = nextpc;
		memmove(cur, next, mach->nwindreg*sizeof(cur[0]));
	}

out:
	free(cur);
	free(next);
	return ret;
}

int
windindex(char *reg)
{
	char **p;
	int i;

	p = mach->windreg;
	for(i=0; i<mach->nwindreg; i++)
		if(strcmp(p[i], reg) == 0)
			return i;
	werrstr("%s is not a winding register", reg);
	return -1;
}

Loc*
windreglocs(void)
{
	int i;
	Loc *loc;

	loc = malloc(mach->nwindreg*sizeof(loc[0]));
	if(loc == nil)
		return nil;
	for(i=0; i<mach->nwindreg; i++){
		loc[i].type = LREG;
		loc[i].reg = mach->windreg[i];
	}
	return loc;
}
