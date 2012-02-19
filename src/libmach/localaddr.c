#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>

/*
 * XXX could remove the rock by hiding it in a special regs.
 * That would still be sleazy but would be thread-safe.
 */

static struct {
	int found;
	int nframe;
	Loc l;
	char *fn;
	char *var;
	char *reg;
} rock;

static int
ltrace(Map *map, Regs *regs, u64int pc, u64int nextpc, Symbol *sym, int depth)
{
	u64int v;
	Symbol s1;

	USED(pc);
	USED(nextpc);
	USED(depth);

	if(sym==nil || strcmp(sym->name, rock.fn) != 0)
		return ++rock.nframe < 40;
	if(rock.reg){
		if(rget(regs, rock.reg, &v) < 0)
			return 0;
		rock.l = locconst(v);
		rock.found = 1;
		return 0;
	}
	if(lookuplsym(sym, rock.var, &s1) < 0)
		return 0;
	if(locsimplify(map, regs, s1.loc, &rock.l) < 0)
		return 0;
	if(rock.l.type == LREG && rget(regs, rock.l.reg, &v) >= 0)
		rock.l = locconst(v);
	if(rock.l.type != LADDR && rock.l.type != LCONST)
		return 0;
	rock.found = 1;
	return 0;
}

int
localaddr(Map *map, Regs *regs, char *fn, char *var, u64int *val)
{
	Regdesc *rp;

	rock.found = 0;
	rock.nframe = 0;
	rock.fn = fn;
	rock.reg = nil;
	rock.var = nil;
	for(rp=mach->reglist; rp->name; rp++)
		if(strcmp(rp->name, var) == 0)
			rock.reg = rp->name;
	if(!rock.reg)
		rock.var = var;
	stacktrace(map, regs, ltrace);
	if(rock.found){
		*val = rock.l.addr;
		return 0;
	}
	return -1;
}
