#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>

int
locfmt(Fmt *fmt)
{
	Loc l;

	l = va_arg(fmt->args, Loc);
	switch(l.type){
	default:
		return fmtprint(fmt, "<loc%d>", l.type);
	case LCONST:
		return fmtprint(fmt, "0x%lux", l.addr);
	case LADDR:
		return fmtprint(fmt, "*0x%lux", l.addr);
	case LOFFSET:
		return fmtprint(fmt, "%ld(%s)", l.offset, l.reg);
	case LREG:
		return fmtprint(fmt, "%s", l.reg);
	}
}

int
loccmp(Loc *a, Loc *b)
{
	int i;

	if(a->type < b->type)
		return -1;
	if(a->type > b->type)
		return 1;
	switch(a->type){
	default:
		return 0;
	case LADDR:
		if(a->addr < b->addr)
			return -1;
		if(a->addr > b->addr)
			return 1;
		return 0;
	case LOFFSET:
		i = strcmp(a->reg, b->reg);
		if(i != 0)
			return i;
		if(a->offset < b->offset)
			return -1;
		if(a->offset > b->offset)
			return 1;
		return 0;
	case LREG:
		return strcmp(a->reg, b->reg);
	}
}

int
lget1(Map *map, Regs *regs, Loc loc, uchar *a, uint n)
{
	if(locsimplify(map, regs, loc, &loc) < 0)
		return -1;
	if(loc.type == LADDR)
		return get1(map, loc.addr, a, n);
	/* could do more here - i'm lazy */
	werrstr("bad location for lget1");
	return -1;
}

int
lget2(Map *map, Regs *regs, Loc loc, u16int *u)
{
	u64int ul;

	if(locsimplify(map, regs, loc, &loc) < 0)
		return -1;
	if(loc.type == LADDR)
		return get2(map, loc.addr, u);
	if(loc.type == LCONST){
		*u = loc.addr;
		return 0;
	}
	if(loc.type == LREG){
		if(rget(regs, loc.reg, &ul) < 0)
			return -1;
		*u = ul;
		return 0;
	}
	werrstr("bad location for lget2");
	return -1;
}

int
lget4(Map *map, Regs *regs, Loc loc, u32int *u)
{
	u64int ul;

	if(locsimplify(map, regs, loc, &loc) < 0)
		return -1;
	if(loc.type == LADDR)
		return get4(map, loc.addr, u);
	if(loc.type == LCONST){
		*u = loc.addr;
		return 0;
	}
	if(loc.type == LREG){
		if(rget(regs, loc.reg, &ul) < 0)
			return -1;
		*u = ul;
		return 0;
	}
	werrstr("bad location for lget4");
	return -1;
}

int
lgeta(Map *map, Regs *regs, Loc loc, u64int *u)
{
	u32int v;

	if(machcpu == &machamd64)
		return lget8(map, regs, loc, u);
	if(lget4(map, regs, loc, &v) < 0)
		return -1;
	*u = v;
	return 4;
}

int
lget8(Map *map, Regs *regs, Loc loc, u64int *u)
{
	u64int ul;

	if(locsimplify(map, regs, loc, &loc) < 0)
		return -1;
	if(loc.type == LADDR)
		return get8(map, loc.addr, u);
	if(loc.type == LCONST){
		*u = loc.addr;
		return 0;
	}
	if(loc.type == LREG){
		if(rget(regs, loc.reg, &ul) < 0)
			return -1;
		*u = ul;
		return 0;
	}
	werrstr("bad location for lget8");
	return -1;
}

int
lput1(Map *map, Regs *regs, Loc loc, uchar *a, uint n)
{
	if(locsimplify(map, regs, loc, &loc) < 0)
		return -1;
	if(loc.type == LADDR)
		return put1(map, loc.addr, a, n);
	/* could do more here - i'm lazy */
	werrstr("bad location for lput1");
	return -1;
}

int
lput2(Map *map, Regs *regs, Loc loc, u16int u)
{
	if(locsimplify(map, regs, loc, &loc) < 0)
		return -1;
	if(loc.type == LADDR)
		return put2(map, loc.addr, u);
	if(loc.type == LREG)
		return rput(regs, loc.reg, u);
	werrstr("bad location for lput2");
	return -1;
}

int
lput4(Map *map, Regs *regs, Loc loc, u32int u)
{
	if(locsimplify(map, regs, loc, &loc) < 0)
		return -1;
	if(loc.type == LADDR)
		return put4(map, loc.addr, u);
	if(loc.type == LREG)
		return rput(regs, loc.reg, u);
	werrstr("bad location for lput4");
	return -1;
}

int
lput8(Map *map, Regs *regs, Loc loc, u64int u)
{
	if(locsimplify(map, regs, loc, &loc) < 0)
		return -1;
	if(loc.type == LADDR)
		return put8(map, loc.addr, u);
	if(loc.type == LREG)
		return rput(regs, loc.reg, u);
	werrstr("bad location for lput8");
	return -1;
}

static Loc zl;

Loc
locaddr(u64int addr)
{
	Loc l;

	l = zl;
	l.type = LADDR;
	l.addr = addr;
	return l;
}

Loc
locindir(char *reg, long offset)
{
	Loc l;

	l = zl;
	l.type = LOFFSET;
	l.reg = reg;
	l.offset = offset;
	l.addr = 0;	/* SHUT UP GCC 4.0 */
	return l;
}

Loc
locconst(u64int con)
{
	Loc l;

	l = zl;
	l.type = LCONST;
	l.addr = con;
	return l;
}

Loc
locnone(void)
{
	Loc l;

	l = zl;
	l.type = LNONE;
	return l;
}

Loc
locreg(char *reg)
{
	Loc l;

	l = zl;
	l.type = LREG;
	l.reg = reg;
	return l;
}

int
locsimplify(Map *map, Regs *regs, Loc loc, Loc *newloc)
{
	u64int u;

	if(loc.type == LOFFSET){
		if(rget(regs, loc.reg, &u) < 0)
			return -1;
		*newloc = locaddr(u + loc.offset);
	}else
		*newloc = loc;
	return 0;
}
