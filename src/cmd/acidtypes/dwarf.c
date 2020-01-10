#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>
#include <elf.h>
#include <dwarf.h>
#include "dat.h"

static void ds2acid(Dwarf*, DwarfSym*, Biobuf*, char*);

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
	DwarfSym s;

	/* pass over dwarf section pulling out type info */

	if(dwarfenum(d, &s) < 0)
		return -1;

	while(dwarfnextsymat(d, &s, 0) == 1)
		ds2acid(d, &s, b, nil);

	printtypes(b);
	dumpsyms(b);
	freetypes();
	return 0;
}

static void
ds2acid(Dwarf *d, DwarfSym *s, Biobuf *b, char *fn)
{
	int depth;
	Type *t;

	depth = s->depth;

	switch(s->attrs.tag){
	case TagSubroutineType:
		t = xnewtype(Function, s);
		goto Recurse;

	case TagSubprogram:
		fn = s->attrs.name;
		goto Recurse;

	case TagCompileUnit:
	case TagLexDwarfBlock:
	Recurse:
		/* recurse into substructure */
		while(dwarfnextsymat(d, s, depth+1) == 1)
			ds2acid(d, s, b, fn);
		break;

	case TagTypedef:
		t = xnewtype(Typedef, s);
		t->name = s->attrs.name;
		t->sub = typebynum(s->attrs.type, 0);
		break;

	case TagBaseType:
		t = xnewtype(Base, s);
		t->xsizeof = s->attrs.bytesize;
		switch(s->attrs.encoding){
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
		t = xnewtype(Pointer, s);
		t->sub = typebynum(s->attrs.type, 0);
		break;

	case TagConstType:
	case TagVolatileType:
		t = xnewtype(Defer, s);
		t->sub = typebynum(s->attrs.type, 0);
		break;

	case TagArrayType:
		t = xnewtype(Array, s);
		t->sub = typebynum(s->attrs.type, 0);
		break;

	case TagStructType:
	case TagUnionType:
		t = xnewtype(Aggr, s);
		t->sue = s->attrs.tag==TagStructType ? 's' : 'u';
		t->xsizeof = s->attrs.bytesize;
		t->suename = s->attrs.name;
		t->isunion = s->attrs.tag==TagUnionType;
		while(dwarfnextsymat(d, s, depth+1) == 1){
			if(s->attrs.tag != TagMember){
				ds2acid(d, s, b, fn);
				continue;
			}
			if(!s->attrs.have.name || !s->attrs.have.type)
				continue;
			if(t->n%32 == 0){
				t->tname = erealloc(t->tname, (t->n+32)*sizeof(t->tname[0]));
				t->val = erealloc(t->val, (t->n+32)*sizeof(t->val[0]));
				t->t = erealloc(t->t, (t->n+32)*sizeof(t->t[0]));
			}
			t->tname[t->n] = s->attrs.name;
			if(t->isunion)
				t->val[t->n] = 0;
			else
				t->val[t->n] = valof(s->attrs.have.datamemberloc, &s->attrs.datamemberloc);
			t->t[t->n] = typebynum(s->attrs.type, 0);
			t->n++;
		}
		break;

	case TagEnumerationType:
		t = xnewtype(Enum, s);
		t->sue = 'e';
		t->suename = s->attrs.name;
		t->xsizeof = s->attrs.bytesize;
		while(dwarfnextsymat(d, s, depth+1) == 1){
			if(s->attrs.tag != TagEnumerator){
				ds2acid(d, s, b, fn);
				continue;
			}
			if(!s->attrs.have.name || !s->attrs.have.constvalue)
				continue;
			if(t->n%32 == 0){
				t->tname = erealloc(t->tname, (t->n+32)*sizeof(t->tname[0]));
				t->val = erealloc(t->val, (t->n+32)*sizeof(t->val[0]));
			}
			t->tname[t->n] = s->attrs.name;
			t->val[t->n] = valof(s->attrs.have.constvalue, &s->attrs.constvalue);
			t->n++;
		}
		break;

	case TagFormalParameter:
	case TagVariable:
		if(s->attrs.name==nil || s->attrs.type==0)
			break;
		addsymx(fn, s->attrs.name, typebynum(s->attrs.type, 0));
		break;
	}
}
