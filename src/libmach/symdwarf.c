#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>
#include "elf.h"
#include "dwarf.h"

static void	dwarfsymclose(Fhdr*);
static int	dwarfpc2file(Fhdr*, u64int, char*, uint, ulong*);
static int	dwarfline2pc(Fhdr*, u64int, ulong, u64int*);
static int	dwarflookuplsym(Fhdr*, Symbol*, char*, Symbol*);
static int	dwarfindexlsym(Fhdr*, Symbol*, uint, Symbol*);
static int	dwarffindlsym(Fhdr*, Symbol*, Loc, Symbol*);
static void	dwarfsyminit(Fhdr*);
static int	dwarftosym(Fhdr*, Dwarf*, DwarfSym*, Symbol*, int);
static int	_dwarfunwind(Fhdr *fhdr, Map *map, Regs *regs, u64int *next, Symbol*);

int
symdwarf(Fhdr *hdr)
{
	if(hdr->dwarf == nil){
		werrstr("no dwarf debugging symbols");
		return -1;
	}

	hdr->symclose = dwarfsymclose;
	hdr->pc2file = dwarfpc2file;
	hdr->line2pc = dwarfline2pc;
	hdr->lookuplsym = dwarflookuplsym;
	hdr->indexlsym = dwarfindexlsym;
	hdr->findlsym = dwarffindlsym;
	hdr->unwind = _dwarfunwind;
	dwarfsyminit(hdr);

	return 0;
}

static void
dwarfsymclose(Fhdr *hdr)
{
	dwarfclose(hdr->dwarf);
	hdr->dwarf = nil;
}

static int
dwarfpc2file(Fhdr *fhdr, u64int pc, char *buf, uint nbuf, ulong *line)
{
	char *cdir, *dir, *file;

	if(dwarfpctoline(fhdr->dwarf, pc, &cdir, &dir, &file, line, nil, nil) < 0)
		return -1;

	if(file[0] == '/' || (dir==nil && cdir==nil))
		strecpy(buf, buf+nbuf, file);
	else if((dir && dir[0] == '/') || cdir==nil)
		snprint(buf, nbuf, "%s/%s", dir, file);
	else
		snprint(buf, nbuf, "%s/%s/%s", cdir, dir ? dir : "", file);
	cleanname(buf);
	return 0;;
}

static int
dwarfline2pc(Fhdr *fhdr, u64int basepc, ulong line, u64int *pc)
{
	werrstr("dwarf line2pc not implemented");
	return -1;
}

static uint
typesize(Dwarf *dwarf, ulong unit, ulong tref, char *name)
{
	DwarfSym ds;

top:
	if(dwarfseeksym(dwarf, unit, tref-unit, &ds) < 0){
	cannot:
		fprint(2, "warning: cannot compute size of parameter %s (%lud %lud: %r)\n",
			name, unit, tref);
		return 0;
	}

	if(ds.attrs.have.bytesize)
		return ds.attrs.bytesize;

	switch(ds.attrs.tag){
	case TagVolatileType:
	case TagRestrictType:
	case TagTypedef:
		if(ds.attrs.have.type != TReference)
			goto cannot;
		tref = ds.attrs.type;
		goto top;
	}

	goto cannot;
}

static int
roundup(int s, int n)
{
	return (s+n-1)&~(n-1);
}

static int
dwarflenum(Fhdr *fhdr, Symbol *p, char *name, uint j, Loc l, Symbol *s)
{
	int depth, bpoff;
	DwarfSym ds;
	Symbol s1;

	if(p == nil)
		return -1;

	if(p->u.dwarf.unit == 0 && p->u.dwarf.uoff == 0)
		return -1;

	if(dwarfseeksym(fhdr->dwarf, p->u.dwarf.unit, p->u.dwarf.uoff, &ds) < 0)
		return -1;

	ds.depth = 1;
	depth = 1;

	bpoff = 8;
	while(dwarfnextsym(fhdr->dwarf, &ds) == 1 && depth < ds.depth){
		if(ds.attrs.tag != TagVariable){
			if(ds.attrs.tag != TagFormalParameter
			&& ds.attrs.tag != TagUnspecifiedParameters)
				continue;
			if(ds.depth != depth+1)
				continue;
		}
		if(dwarftosym(fhdr, fhdr->dwarf, &ds, &s1, 1) < 0)
			continue;
		/* XXX move this out once there is another architecture */
		/*
		 * gcc tells us the registers where the parameters might be
		 * held for an instruction or two.  use the parameter list to
		 * recompute the actual stack locations.
		 */
		if(fhdr->mtype == M386)
		if(ds.attrs.tag==TagFormalParameter || ds.attrs.tag==TagUnspecifiedParameters){
			if(s1.loc.type==LOFFSET
			&& strcmp(s1.loc.reg, "BP")==0
			&& s1.loc.offset >= 8)
				bpoff = s1.loc.offset;
			else{
				s1.loc.type = LOFFSET;
				s1.loc.reg = "BP";
				s1.loc.offset = bpoff;
			}
			if(ds.attrs.tag == TagFormalParameter){
				if(ds.attrs.have.type)
					bpoff += roundup(typesize(fhdr->dwarf, p->u.dwarf.unit, ds.attrs.type, s1.name), 4);
				else
					fprint(2, "warning: cannot compute size of parameter %s\n", s1.name);
			}
		}
		if(name){
			if(strcmp(ds.attrs.name, name) != 0)
				continue;
		}else if(l.type){
			if(loccmp(&s1.loc, &l) != 0)
				continue;
		}else{
			if(j-- > 0)
				continue;
		}
		*s = s1;
		return 0;
	}
	return -1;
}

static Loc zl;

static int
dwarflookuplsym(Fhdr *fhdr, Symbol *p, char *name, Symbol *s)
{
	return dwarflenum(fhdr, p, name, 0, zl, s);
}

static int
dwarfindexlsym(Fhdr *fhdr, Symbol *p, uint i, Symbol *s)
{
	return dwarflenum(fhdr, p, nil, i, zl, s);
}

static int
dwarffindlsym(Fhdr *fhdr, Symbol *p, Loc l, Symbol *s)
{
	return dwarflenum(fhdr, p, nil, 0, l, s);
}

static void
dwarfsyminit(Fhdr *fp)
{
	Dwarf *d;
	DwarfSym s;
	Symbol sym;

	d = fp->dwarf;
	if(dwarfenum(d, &s) < 0)
		return;

	while(dwarfnextsymat(d, &s, 0) == 1)
	while(dwarfnextsymat(d, &s, 1) == 1){
		if(s.attrs.name == nil)
			continue;
		switch(s.attrs.tag){
		case TagSubprogram:
		case TagVariable:
			if(dwarftosym(fp, d, &s, &sym, 0) < 0)
				continue;
			_addsym(fp, &sym);
		}
	}
}

static char*
regname(Dwarf *d, int i)
{
	if(i < 0 || i >= d->nreg)
		return nil;
	return d->reg[i];
}

static int
dwarftosym(Fhdr *fp, Dwarf *d, DwarfSym *ds, Symbol *s, int infn)
{
	DwarfBuf buf;
	DwarfBlock b;

	memset(s, 0, sizeof *s);
	s->u.dwarf.uoff = ds->uoff;
	s->u.dwarf.unit = ds->unit;
	switch(ds->attrs.tag){
	default:
		return -1;
	case TagUnspecifiedParameters:
		ds->attrs.name = "...";
		s->type = 'p';
		goto sym;
	case TagFormalParameter:
		s->type = 'p';
		s->class = CPARAM;
		goto sym;
	case TagSubprogram:
		s->type = 't';
		s->class = CTEXT;
		goto sym;
	case TagVariable:
		if(infn){
			s->type = 'a';
			s->class = CAUTO;
		}else{
			s->type = 'd';
			s->class = CDATA;
		}
	sym:
		s->name = ds->attrs.name;
		if(ds->attrs.have.lowpc){
			s->loc.type = LADDR;
			s->loc.addr = ds->attrs.lowpc;
			if(ds->attrs.have.highpc){
				s->hiloc.type = LADDR;
				s->hiloc.addr = ds->attrs.highpc;
			}
		}else if(ds->attrs.have.location == TConstant){
			s->loc.type = LADDR;
			s->loc.addr = ds->attrs.location.c;
		}else if(ds->attrs.have.location == TBlock){
			b = ds->attrs.location.b;
			if(b.len == 0)
				return -1;
			buf.p = b.data+1;
			buf.ep = b.data+b.len;
			buf.d = d;
			buf.addrsize = 0;
			if(b.data[0]==OpAddr){
				if(b.len != 5)
					return -1;
				s->loc.type = LADDR;
				s->loc.addr = dwarfgetaddr(&buf);
			}else if(OpReg0 <= b.data[0] && b.data[0] < OpReg0+0x20){
				if(b.len != 1 || (s->loc.reg = regname(d, b.data[0]-OpReg0)) == nil)
					return -1;
				s->loc.type = LREG;
			}else if(OpBreg0 <= b.data[0] && b.data[0] < OpBreg0+0x20){
				s->loc.type = LOFFSET;
				s->loc.reg = regname(d, b.data[0]-0x70);
				s->loc.offset = dwarfget128s(&buf);
				if(s->loc.reg == nil)
					return -1;
			}else if(b.data[0] == OpRegx){
				s->loc.type = LREG;
				s->loc.reg = regname(d, dwarfget128(&buf));
				if(s->loc.reg == nil)
					return -1;
			}else if(b.data[0] == OpFbreg){
				s->loc.type = LOFFSET;
				s->loc.reg = mach->fp;
				s->loc.offset = dwarfget128s(&buf);
			}else if(b.data[0] == OpBregx){
				s->loc.type = LOFFSET;
				s->loc.reg = regname(d, dwarfget128(&buf));
				s->loc.offset = dwarfget128s(&buf);
				if(s->loc.reg == nil)
					return -1;
			}else
				s->loc.type = LNONE;
			if(buf.p != buf.ep)
				s->loc.type = LNONE;
		}else
			return -1;
		if(ds->attrs.isexternal)
			s->type += 'A' - 'a';
		if(ds->attrs.tag==TagVariable && s->loc.type==LADDR && s->loc.addr>=fp->dataddr+fp->datsz)
			s->type += 'b' - 'd';
		s->fhdr = fp;
		return 0;
	}
}

static int
dwarfeval(Dwarf *d, Map *map, Regs *regs, ulong cfa, int rno, DwarfExpr e, u64int *u)
{
	int i;
	u32int u4;
	u64int uu;

	switch(e.type){
	case RuleUndef:
		*u = 0;
		return 0;
	case RuleSame:
		if(rno == -1){
			werrstr("pc cannot be `same'");
			return -1;
		}
		return rget(regs, regname(d, rno), u);
	case RuleRegister:
		if((i = windindex(regname(d, e.reg))) < 0)
			return -1;
		return rget(regs, regname(d, i), u);
	case RuleCfaOffset:
		if(cfa == 0){
			werrstr("unknown cfa");
			return -1;
		}
		if(get4(map, cfa + e.offset, &u4) < 0)
			return -1;
		*u = u4;
		return 0;
	case RuleRegOff:
		if(rget(regs, regname(d, e.reg), &uu) < 0)
			return -1;
		if(get4(map, uu+e.offset, &u4) < 0)
			return -1;
		*u = u4;
		return 0;
	case RuleLocation:
		werrstr("not evaluating dwarf loc expressions");
		return -1;
	}
	werrstr("not reached in dwarfeval");
	return -1;
}

#if 0
static int
dwarfexprfmt(Fmt *fmt)
{
	DwarfExpr *e;

	if((e = va_arg(fmt->args, DwarfExpr*)) == nil)
		return fmtstrcpy(fmt, "<nil>");

	switch(e->type){
	case RuleUndef:
		return fmtstrcpy(fmt, "undef");
	case RuleSame:
		return fmtstrcpy(fmt, "same");
	case RuleCfaOffset:
		return fmtprint(fmt, "%ld(cfa)", e->offset);
	case RuleRegister:
		return fmtprint(fmt, "r%ld", e->reg);
	case RuleRegOff:
		return fmtprint(fmt, "%ld(r%ld)", e->offset, e->reg);
	case RuleLocation:
		return fmtprint(fmt, "l.%.*H", e->loc.len, e->loc.data);
	default:
		return fmtprint(fmt, "?%d", e->type);
	}
}
#endif

static int
_dwarfunwind(Fhdr *fhdr, Map *map, Regs *regs, u64int *next, Symbol *sym)
{
	char *name;
	int i, j;
	u64int cfa, pc, u;
	Dwarf *d;
	DwarfExpr *e, epc, ecfa;


	/*
	 * Use dwarfunwind to tell us what to do.
	 */
	d = fhdr->dwarf;
	e = malloc(d->nreg*sizeof(e[0]));
	if(e == nil)
		return -1;
	if(rget(regs, mach->pc, &pc) < 0)
		goto err;
	if(dwarfunwind(d, pc, &ecfa, &epc, e, d->nreg) < 0)
		goto err;

	/*
	 * Compute CFA.
	 */
	switch(ecfa.type){
	default:
		werrstr("invalid call-frame-address in _dwarfunwind");
		goto err;
	case RuleRegister:
		ecfa.offset = 0;
	case RuleRegOff:
		if((name = regname(d, ecfa.reg)) == nil){
			werrstr("invalid call-frame-address register %d", (int)ecfa.reg);
			goto err;
		}
		if(rget(regs, name, &cfa) < 0){
			werrstr("fetching %s for call-frame-address: %r", name);
			goto err;
		}
		cfa += ecfa.offset;
	}

	/*
	 * Compute registers.
	 */
	for(i=0; i<d->nreg; i++){
		j = windindex(d->reg[i]);
		if(j == -1)
			continue;
		if(dwarfeval(d, map, regs, cfa, i, e[i], &u) < 0)
			u = ~(ulong)0;
		next[j] = u;
	}

	/*
	 * Compute caller pc
	 */
	if(dwarfeval(d, map, regs, cfa, -1, epc, &u) < 0){
		werrstr("computing caller %s: %r", mach->pc);
		goto err;
	}
	next[windindex(mach->pc)] = u;
	free(e);
	return 0;

err:
	free(e);
	return -1;
}
