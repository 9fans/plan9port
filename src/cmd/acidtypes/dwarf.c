#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>
#include <elf.h>
#include <dwarf.h>
#include "dat.h"

static ulong
valof(uint ty, DwarfVal *v)
{
	switch(ty){
	default:
fmtinstall('H', encodefmt);
fprint(2, "valof %d %.*H\n", ty, v->b.len, v->b.data);
		return 0;
	case TConstant:
		return v->c;
	}
}

static Type*
xnewtype(uint ty, DwarfSym *s)
{
	Type *t;

	t = typebynum(s->unit+s->uoff, 0);
	t->ty = ty;
	return t;
}

int
dwarf2acid(Dwarf *d, Biobuf *b)
{
	char *fn;
	DwarfSym s;
	Type *t;

	/* pass over dwarf section pulling out type info */

	if(dwarfenum(d, &s) < 0)
		return -1;

	while(dwarfnextsym(d, &s, s.depth!=1) == 1){
	top:
		switch(s.attrs.tag){
		case TagSubprogram:
		case TagLexDwarfBlock:
			dwarfnextsym(d, &s, 1);
			goto top;

		case TagTypedef:
			t = xnewtype(Typedef, &s);
			t->name = s.attrs.name;
			t->sub = typebynum(s.attrs.type, 0);
			break;
		case TagBaseType:
			t = xnewtype(Base, &s);
			t->xsizeof = s.attrs.bytesize;
			switch(s.attrs.encoding){
			default:
			case TypeAddress:
				t->printfmt = 'x';
				break;
			case TypeBoolean:
			case TypeUnsigned:
			case TypeSigned:
			case TypeSignedChar:
			case TypeUnsignedChar:
				t->printfmt = 'd';
				break;
			case TypeFloat:
				t->printfmt = 'f';
				break;
			case TypeComplexFloat:
				t->printfmt = 'F';
				break;
			case TypeImaginaryFloat:
				t->printfmt = 'i';
				break;
			}
			break;
		case TagPointerType:
			t = xnewtype(Pointer, &s);
			t->sub = typebynum(s.attrs.type, 0);
			break;
		case TagStructType:
		case TagUnionType:
			t = xnewtype(Aggr, &s);
			t->sue = s.attrs.tag==TagStructType ? 's' : 'u';
			t->xsizeof = s.attrs.bytesize;
			t->suename = s.attrs.name;
			t->isunion = s.attrs.tag==TagUnionType;
			dwarfnextsym(d, &s, 1);
			if(s.depth != 2)
				goto top;
			do{
				if(!s.attrs.have.name || !s.attrs.have.type || s.attrs.tag != TagMember)
					continue;
				if(t->n%32 == 0){
					t->tname = erealloc(t->tname, (t->n+32)*sizeof(t->tname[0]));
					t->val = erealloc(t->val, (t->n+32)*sizeof(t->val[0]));
					t->t = erealloc(t->t, (t->n+32)*sizeof(t->t[0]));
				}
				t->tname[t->n] = s.attrs.name;
				if(t->isunion)
					t->val[t->n] = 0;
				else
					t->val[t->n] = valof(s.attrs.have.datamemberloc, &s.attrs.datamemberloc);
				t->t[t->n] = typebynum(s.attrs.type, 0);
				t->n++;
			}while(dwarfnextsym(d, &s, 1) == 1 && s.depth==2);
			goto top;
			break;
		case TagSubroutineType:
			t = xnewtype(Function, &s);
			break;
		case TagConstType:
		case TagVolatileType:
			t = xnewtype(Defer, &s);
			t->sub = typebynum(s.attrs.type, 0);
			break;
		case TagArrayType:
			t = xnewtype(Array, &s);
			t->sub = typebynum(s.attrs.type, 0);
			break;
		case TagEnumerationType:
			t = xnewtype(Enum, &s);
			t->sue = 'e';
			t->suename = s.attrs.name;
			t->xsizeof = s.attrs.bytesize;
			dwarfnextsym(d, &s, 1);
			if(s.depth != 2)
				goto top;
			do{
				if(!s.attrs.have.name || !s.attrs.have.constvalue || s.attrs.tag != TagEnumerator)
					continue;
				if(t->n%32 == 0){
					t->tname = erealloc(t->tname, (t->n+32)*sizeof(t->tname[0]));
					t->val = erealloc(t->val, (t->n+32)*sizeof(t->val[0]));
				}
				t->tname[t->n] = s.attrs.name;
				t->val[t->n] = valof(s.attrs.have.constvalue, &s.attrs.constvalue);
				t->n++;
			}while(dwarfnextsym(d, &s, 1) == 1 && s.depth==2);
			goto top;
			break;
		}
	}

	printtypes(b);

	/* pass over dwarf section pulling out type definitions */

	if(dwarfenum(d, &s) < 0)
		goto out;

	fn = nil;
	while(dwarfnextsym(d, &s, 1) == 1){
		if(s.depth == 1)
			fn = nil;
		switch(s.attrs.tag){
		case TagSubprogram:
			fn = s.attrs.name;
			break;
		case TagFormalParameter:
			if(s.depth != 2)
				break;
			/* fall through */
		case TagVariable:
			if(s.attrs.name == nil || s.attrs.type == 0)
				continue;
			t = typebynum(s.attrs.type, 0);
			if(t->ty == Pointer){
				t = t->sub;
				if(t && t->equiv)
					t = t->equiv;
			}
			if(t == nil)
				break;
			if(t->ty != Aggr)
				break;
			Bprint(b, "complex %s %s%s%s;\n", nameof(t, 1),
				fn ? fixname(fn) : "", fn ? ":" : "", fixname(s.attrs.name));
			break;
		}
	}

out:
	freetypes();
	return 0;
}

