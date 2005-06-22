#include <u.h>
#include <libc.h>
#include <bio.h>
#include "elf.h"
#include "dwarf.h"

void printrules(Dwarf *d, ulong pc);
int exprfmt(Fmt*);

void
usage(void)
{
	fprint(2, "usage: dwarfdump file\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	int c;
	Elf *elf;
	Dwarf *d;
	DwarfSym s;
	char *cdir, *dir, *file;
	ulong line, mtime, length;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	fmtinstall('R', exprfmt);
	fmtinstall('H', encodefmt);

	if((elf = elfopen(argv[0])) == nil)
		sysfatal("elfopen %s: %r", argv[0]);
	if((d=dwarfopen(elf)) == nil)
		sysfatal("dwarfopen: %r");

	if(dwarfenum(d, &s) < 0)
		sysfatal("dwarfenumall: %r");

	while(dwarfnextsym(d, &s) == 1){
		switch(s.attrs.tag){
		case TagCompileUnit:
			print("compileunit %s\n", s.attrs.name);
			break;
		case TagSubprogram:
			c = 't';
			goto sym;
		case TagVariable:
			c = 'd';
			goto sym;
		case TagConstant:
			c = 'c';
			goto sym;
		case TagFormalParameter:
			if(!s.attrs.name)
				break;
			c = 'p';
		sym:
			if(s.attrs.isexternal)
				c += 'A' - 'a';
			print("%c %s", c, s.attrs.name);
			if(s.attrs.have.lowpc)
				print(" 0x%lux-0x%lux", s.attrs.lowpc, s.attrs.highpc);
			switch(s.attrs.have.location){
			case TBlock:
				print(" @ %.*H", s.attrs.location.b.len, s.attrs.location.b.data);
				break;
			case TConstant:
				print(" @ 0x%lux", s.attrs.location.c);
				break;
			}
			if(s.attrs.have.ranges)
				print(" ranges@0x%lux", s.attrs.ranges);
			print("\n");
			if(s.attrs.have.lowpc){
				if(dwarfpctoline(d, s.attrs.lowpc, &cdir, &dir, &file, &line, &mtime, &length) < 0)
					print("\tcould not find source: %r\n");
				else if(dir == nil)
					print("\t%s/%s:%lud mtime=%lud length=%lud\n",
						cdir, file, line, mtime, length);
				else
					print("\t%s/%s/%s:%lud mtime=%lud length=%lud\n",
						cdir, dir, file, line, mtime, length);

				if(0) printrules(d, s.attrs.lowpc);
				if(0) printrules(d, (s.attrs.lowpc+s.attrs.highpc)/2);
			}
			break;
		}
	}
	exits(0);
}

void
printrules(Dwarf *d, ulong pc)
{
	int i;
	DwarfExpr r[10];
	DwarfExpr cfa, ra;

	if(dwarfunwind(d, pc, &cfa, &ra, r, nelem(r)) < 0)
		print("\tcannot unwind from pc 0x%lux: %r\n", pc);

	print("\tpc=0x%lux cfa=%R ra=%R", pc, &cfa, &ra);
	for(i=0; i<nelem(r); i++)
		if(r[i].type != RuleSame)
			print(" r%d=%R", i, &r[i]);
	print("\n");
}

int
exprfmt(Fmt *fmt)
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
