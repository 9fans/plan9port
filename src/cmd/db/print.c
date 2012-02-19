/*
 *
 *	debugger
 *
 */
#include "defs.h"
#include "fns.h"

#define ptrace dbptrace

extern	int	infile;
extern	int	outfile;
extern	int	maxpos;

/* general printing routines ($) */

char	*Ipath = INCDIR;
static	int	tracetype;
static void	printfp(Map*, int);

/*
 *	callback on stack trace
 */
static int
ptrace(Map *map, Regs *regs, u64int pc, u64int nextpc, Symbol *sym, int depth)
{
	char buf[512];

	USED(map);
	if(sym){
		dprint("%s(", sym->name);
		printparams(sym, regs);
		dprint(") ");
	}else
		dprint("%#lux ", pc);
	printsource(pc);

	dprint(" called from ");
	symoff(buf, 512, nextpc, CTEXT);
	dprint("%s ", buf);
/*	printsource(nextpc); */
	dprint("\n");
	if(tracetype == 'C' && sym)
		printlocals(sym, regs);
	return depth<40;
}

static ulong *adrregvals;

static int
adrrw(Regs *regs, char *name, u64int *val, int isr)
{
	int i;

	if((i = windindex(name)) == -1)
		return correg->rw(correg, name, val, isr);
	if(isr){
		*val = adrregvals[i];
		return 0;
	}
	werrstr("saved registers are immutable");
	return -1;
}

Regs*
adrregs(void)
{
	int i;
	static Regs r;
	static u32int x;

	if(adrregvals== nil){
		adrregvals = malloc(mach->nwindreg*sizeof(adrregvals[0]));
		if(adrregvals == nil)
			error("%r");
	}
	for(i=0; i<mach->nwindreg; i++){
		if(get4(cormap, adrval+4*i, &x) < 0)
			error("%r");
		adrregvals[i] = x;
	}
	r.rw = adrrw;
	return &r;
}

void
printdollar(int modif)
{
	int	i;
	u32int u4;
	BKPT *bk;
	Symbol s;
	int	stack;
	char	*fname;
	char buf[512];
	Regs *r;

	if (cntflg==0)
		cntval = -1;
	switch (modif) {

	case '<':
		if (cntval == 0) {
			while (readchar() != EOR)
				;
			reread();
			break;
		}
		if (rdc() == '<')
			stack = 1;
		else {
			stack = 0;
			reread();
		}
		fname = getfname();
		redirin(stack, fname);
		break;

	case '>':
		fname = getfname();
		redirout(fname);
		break;

	case 'a':
		attachprocess();
		break;

/* maybe use this for lwpids?
	case 'A':
		attachpthread();
		break;
*/
	case 'k':
		kmsys();
		break;

	case 'q':
	case 'Q':
		done();

	case 'w':
		maxpos=(adrflg?adrval:MAXPOS);
		break;

	case 'S':
		printsym();
		break;

	case 's':
		maxoff=(adrflg?adrval:MAXOFF);
		break;

	case 'm':
		printmap("? map", symmap);
		printmap("/ map", cormap);
		break;

	case 0:
	case '?':
		if (pid)
			dprint("pid = %d\n",pid);
		else
			prints("no process\n");
		flushbuf();

	case 'r':
	case 'R':
		printregs(modif);
		return;

	case 'f':
	case 'F':
		printfp(cormap, modif);
		return;

	case 'c':
	case 'C':
		tracetype = modif;
		if (adrflg)
			r = adrregs();
		else
			r = correg;
		if(stacktrace(cormap, correg, ptrace) <= 0)
			error("no stack frame");
		break;

		/*print externals*/
	case 'e':
		for (i = 0; indexsym(i, &s)>=0; i++) {
			if (s.class==CDATA)
			if (s.loc.type==LADDR)
			if (get4(cormap, s.loc.addr, &u4) > 0)
				dprint("%s/%12t%#lux\n", s.name, (ulong)u4);
		}
		break;

		/*print breakpoints*/
	case 'b':
	case 'B':
		for (bk=bkpthead; bk; bk=bk->nxtbkpt)
			if (bk->flag) {
				symoff(buf, 512, (WORD)bk->loc, CTEXT);
				dprint(buf);
				if (bk->count != 1)
					dprint(",%d", bk->count);
				dprint(":%c %s", bk->flag == BKPTTMP ? 'B' : 'b', bk->comm);
			}
		break;

	case 'M':
		fname = getfname();
		if (machbyname(fname) == 0)
			dprint("unknown name\n");;
		break;
	default:
		error("bad `$' command");
	}
	USED(r);

}

char *
getfname(void)
{
	static char fname[ARB];
	char *p;

	if (rdc() == EOR) {
		reread();
		return (0);
	}
	p = fname;
	do {
		*p++ = lastc;
		if (p >= &fname[ARB-1])
			error("filename too long");
	} while (rdc() != EOR);
	*p = 0;
	reread();
	return (fname);
}

static void
printfp(Map *map, int modif)
{
	Regdesc *rp;
	int i;
	int ret;
	char buf[512];

	for (i = 0, rp = mach->reglist; rp->name; rp += ret) {
		ret = 1;
		if (!(rp->flags&RFLT))
			continue;
		ret = fpformat(map, rp, buf, sizeof(buf), modif);
		if (ret < 0) {
			werrstr("Register %s: %r", rp->name);
			error("%r");
		}
			/* double column print */
		if (i&0x01)
			dprint("%40t%-8s%-12s\n", rp->name, buf);
		else
			dprint("\t%-8s%-12s", rp->name, buf);
		i++;
	}
}

void
redirin(int stack, char *file)
{
	char pfile[ARB];

	if (file == 0) {
		iclose(-1, 0);
		return;
	}
	iclose(stack, 0);
	if ((infile = open(file, 0)) < 0) {
		strcpy(pfile, Ipath);
		strcat(pfile, "/");
		strcat(pfile, file);
		if ((infile = open(pfile, 0)) < 0) {
			infile = STDIN;
			error("cannot open");
		}
	}
}

void
printmap(char *s, Map *map)
{
	int i;

	if (!map)
		return;
	if (map == symmap)
		dprint("%s%12t`%s'\n", s, symfil==nil ? "-" : symfil);
	else if (map == cormap)
		dprint("%s%12t`%s'\n", s, corfil==nil ? "-" : corfil);
	else
		dprint("%s\n", s);
	for (i = 0; i < map->nseg; i++) {
		dprint("%s%8t%-16#lux %-16#lux %-16#lux %s\n", map->seg[i].name,
			map->seg[i].base, map->seg[i].base+map->seg[i].size, map->seg[i].offset,
			map->seg[i].file ? map->seg[i].file : "");
	}
}

/*
 *	dump the raw symbol table
 */
void
printsym(void)
{
	int i;
	Symbol *sp, s;

	for (i=0; indexsym(i, &s)>=0; i++){
		sp = &s;
		switch(sp->type) {
		case 't':
		case 'l':
			dprint("%8#lux t %s\n", sp->loc.addr, sp->name);
			break;
		case 'T':
		case 'L':
			dprint("%8#lux T %s\n", sp->loc.addr, sp->name);
			break;
		case 'D':
		case 'd':
		case 'B':
		case 'b':
		case 'a':
		case 'p':
		case 'm':
			dprint("%8#lux %c %s\n", sp->loc.addr, sp->type, sp->name);
			break;
		default:
			break;
		}
	}
}

#define	STRINGSZ	128

/*
 *	print the value of dot as file:line
 */
void
printsource(long dot)
{
	char str[STRINGSZ];

	if (fileline(dot, str, STRINGSZ) >= 0)
		dprint("%s", str);
}

void
printpc(void)
{
	char buf[512];
	u64int u;

	if(rget(correg, mach->pc, &u) < 0)
		error("%r");
	dot = u;
	if(dot){
		printsource((long)dot);
		printc(' ');
		symoff(buf, sizeof(buf), (long)dot, CTEXT);
		dprint("%s/", buf);
		if (mach->das(cormap, dot, 'i', buf, sizeof(buf)) < 0)
			error("%r");
		dprint("%16t%s\n", buf);
	}
}

void
printlocals(Symbol *fn, Regs *regs)
{
	int i;
	u32int v;
	Symbol s;

	for (i = 0; indexlsym(fn, i, &s)>=0; i++) {
		if (s.class != CAUTO)
			continue;
		if(lget4(cormap, regs, s.loc, &v) >= 0)
			dprint("%8t%s.%s/%10t%#lux\n", fn->name, s.name, v);
		else
			dprint("%8t%s.%s/%10t?\n", fn->name, s.name);
	}
}

void
printparams(Symbol *fn, Regs *regs)
{
	int i;
	Symbol s;
	u32int v;
	int first = 0;

	for (i = 0; indexlsym(fn, i, &s)>=0; i++) {
		if (s.class != CPARAM)
			continue;
		if (first++)
			dprint(", ");
		if(lget4(cormap, regs, s.loc, &v) >= 0)
			dprint("%s=%#lux", s.name, v);
		else
			dprint("%s=?", s.name);
	}
}
