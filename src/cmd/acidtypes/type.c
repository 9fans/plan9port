#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>
#include <ctype.h>
#include "dat.h"

char *prefix = "";

static TypeList *thash[1021];
static TypeList *namehash[1021];
static TypeList *alltypes;

static uint
hash(uint num, uint num1)
{
	return (num*1009 + num1*1013) % nelem(thash);
}

static uint
shash(char *s)
{
	uint h;

	h = 0;
	for(; *s; s++)
		h = 37*h + *s;
	return h%nelem(namehash);
}

void
addnamehash(Type *t)
{
	uint h;

	if(t->name){
		h = shash(t->name);
		namehash[h] = mktl(t, namehash[h]);
	}
}

static void
addhash(Type *t)
{
	uint h;

	if(t->n1 || t->n2){
		h = hash(t->n1, t->n2);
		thash[h] = mktl(t, thash[h]);
	}
	if(t->name)
		addnamehash(t);
}

Type*
typebysue(char sue, char *name)
{
	Type *t;
	TypeList *tl;

	for(tl=namehash[shash(name)]; tl; tl=tl->tl){
		t = tl->hd;
		if(t->sue==sue && t->suename && strcmp(name, t->suename)==0)
			return t;
	}
	t = newtype();
	if(sue=='e')
		t->ty = Enum;
	else
		t->ty = Aggr;
	if(sue=='u')
		t->isunion = 1;
	t->sue = sue;
	t->suename = name;
	addnamehash(t);
	return t;
}

Type*
typebynum(uint n1, uint n2)
{
	Type *t;
	TypeList *tl;

	if(n1 || n2){
		for(tl=thash[hash(n1, n2)]; tl; tl=tl->tl){
			t = tl->hd;
			if(t->n1==n1 && t->n2==n2)
				return t;
		}
	}

	t = newtype();
	t->n1 = n1;
	t->n2 = n2;
	addhash(t);
	return t;
}	

Type*
newtype(void)
{
	Type *t;
	static int gen;

	t = emalloc(sizeof *t);
	t->gen = ++gen;
	alltypes = mktl(t, alltypes);
	return t;
}

struct {
	char *old;
	char *new;
} fixes[] = { /* Font Tab 4 */
	"append",		"$append",
	"builtin",		"$builtin",
	"complex",		"$complex",
	"delete",		"$delete",
	"do",			"$do",
	"else",			"$else",
	"eval",			"$eval",
	"fmt",			"$fmt",
	"fn",			"$fn",
	"head",			"$head",
	"if",			"$if",
	"local",		"$local",
	"loop",			"$loop",
	"ret",			"$ret",
	"tail",			"$tail",
	"then",			"$then",
	"whatis",			"$whatis",
	"while",		"$while",
};

char*
fixname(char *name)
{
	int i;

	if(name == nil)
		return nil;
	for(i=0; i<nelem(fixes); i++)
		if(name[0]==fixes[i].old[0] && strcmp(name, fixes[i].old)==0)
			return fixes[i].new;
	return name;
}

void
denumber(void)
{
	memset(thash, 0, sizeof thash);
	memset(namehash, 0, sizeof namehash);
}

void
renumber(TypeList *tl, uint n1)
{
	Type *t;

	for(; tl; tl=tl->tl){
		t = tl->hd;
		t->n1 = n1;
		addhash(t);
	}
}

Type*
defer(Type *t)
{
	Type *u, *oldt;
	int n;

	u = t;
	n = 0;
	oldt = t;
	while(t && (t->ty == Defer || t->ty == Typedef)){
		if(n++%2)
			u = u->sub;
		t = t->sub;
		if(t == u)	/* cycle */
			goto cycle;
	}
	return t;

cycle:
	fprint(2, "cycle\n");
	t = oldt;
	n = 0;
	while(t && (t->ty==Defer || t->ty==Typedef)){
		fprint(2, "t %p/%d %s\n", t, t->ty, t->name);
		if(t == u && n++ == 2)
			break;
		t = t->sub;
	}
	return u;
}

static void
dotypedef(Type *t)
{
	if(t->ty != Typedef && t->ty != Defer)
		return;

	if(t->didtypedef)
		return;

	t->didtypedef = 1;
	if(t->sub == nil)
		return;

	/* push names downward to remove anonymity */
	if(t->name && t->sub->name == nil)
		t->sub->name = t->name;

	dotypedef(t->sub);
}

static int
countbytes(uvlong x)
{
	int n;

	for(n=0; x; n++)
		x>>=8;
	return n;
}

static void
dorange(Type *t)
{
	Type *tt;

	if(t->ty != Range)
		return;
	if(t->didrange)
		return;
	t->didrange = 1;
	tt = defer(t->sub);
	if(tt == nil)
		return;
	dorange(tt);
	if(t != tt && tt->ty != Base)
		return;
	t->ty = Base;
	t->xsizeof = tt->xsizeof;
	if(t->lo == 0)
		t->printfmt = 'x';
	else
		t->printfmt = 'd';
	if(t->xsizeof == 0)
		t->xsizeof = countbytes(t->hi);
}


char*
nameof(Type *t, int doanon)
{
	static char buf[1024];
	char *p;

	if(t->name)
		strcpy(buf, fixname(t->name));
	else if(t->suename)
		snprint(buf, sizeof buf, "%s_%s", t->isunion ? "union" : "struct", t->suename);
	else if(doanon)
		snprint(buf, sizeof buf, "%s_%lud_", prefix, t->gen);
	else
		return "";
	for(p=buf; *p; p++)
		if(isspace(*p))
			*p = '_';
	return buf;
}

static char
basecharof(Type *t)	//XXX
{
	switch(t->xsizeof){
	default:
		return 'X';
	case 1:
		return 'b';
	case 2:
		if(t->printfmt=='d')
			return 'd';
		else
			return 'x';
	case 4:
		if(t->printfmt=='d')
			return 'D';
		else if(t->printfmt=='f')
			return 'f';
		else
			return 'X';
	case 8:
		if(t->printfmt=='d')
			return 'V';
		else if(t->printfmt=='f')
			return 'F';
		else
			return 'Y';
	}
}

static int
nilstrcmp(char *a, char *b)
{
	if(a == b)
		return 0;
	if(a == nil)
		return -1;
	if(b == nil)
		return 1;
	return strcmp(a, b);
}

static int
typecmp(Type *t, Type *u)
{
	int i;

	if(t == u)
		return 0;
	if(t == nil)
		return -1;
	if(u == nil)
		return 1;

	if(t->ty < u->ty)
		return -1;
	if(t->ty > u->ty)
		return 1;

	if(t->isunion != u->isunion)
		return t->isunion - u->isunion;

	i = nilstrcmp(t->name, u->name);
	if(i)
		return i;

	i = nilstrcmp(t->suename, u->suename);
	if(i)
		return i;

	if(t->ty == Aggr){
		if(t->n > u->n)
			return -1;
		if(t->n < u->n)
			return 1;
	}

	if(t->name || t->suename)
		return 0;

	if(t->ty==Enum){
		if(t->n < u->n)
			return -1;
		if(t->n > u->n)
			return 1;
		if(t->n == 0)
			return 0;
		i = strcmp(t->tname[0], u->tname[0]);
		return i;
	}
	if(t < u)
		return -1;
	if(t > u)
		return 1;
	return 0;
}

static int
qtypecmp(const void *va, const void *vb)
{
	Type *t, *u;

	t = *(Type**)va;
	u = *(Type**)vb;
	return typecmp(t, u);
}

void
printtype(Biobuf *b, Type *t)
{
	char *name;
	int j, nprint;
	Type *tt, *ttt;

	if(t->printed)
		return;
	t->printed = 1;
	switch(t->ty){
	case Aggr:
		name = nameof(t, 1);
		Bprint(b, "sizeof%s = %lud;\n", name, t->xsizeof);
		Bprint(b, "aggr %s {\n", name);
		nprint = 0;
		for(j=0; j<t->n; j++){
			tt = defer(t->t[j]);
			if(tt && tt->equiv)
				tt = tt->equiv;
			if(tt == nil){
				Bprint(b, "// oops: nil type\n");
				continue;
			}
			switch(tt->ty){
			default:
				Bprint(b, "// oops: unknown type %d for %p/%s (%d,%d; %c,%s; %p)\n",
					tt->ty, tt, fixname(t->tname[j]),
					tt->n1, tt->n2, tt->sue ? tt->sue : '.', tt->suename, tt->sub);
Bprint(b, "// t->t[j] = %p\n", ttt=t->t[j]);
while(ttt){
Bprint(b, "// %s %d (%d,%d) sub %p\n", ttt->name, ttt->ty, ttt->n1, ttt->n2, ttt->sub);
ttt=ttt->sub;
}
			case Base:
			case Pointer:
			case Enum:
			case Array:
			case Function:
				nprint++;
				Bprint(b, "\t'%c' %lud %s;\n", basecharof(tt), t->val[j], fixname(t->tname[j]));
				break;
			case Aggr:
				nprint++;
				Bprint(b, "\t%s %lud %s;\n", nameof(tt, 1), t->val[j], fixname(t->tname[j]));
				break;
			}
		}
		if(nprint == 0)
			Bprint(b, "\t'X' 0 __dummy;\n");
		Bprint(b, "};\n\n");
	
		name = nameof(t, 1);	/* might have smashed it */
		Bprint(b, "defn %s(addr) { indent_%s(addr, \"\"); }\n", name, name);
		Bprint(b, "defn\nindent_%s(addr, indent) {\n", name);
		Bprint(b, "\tcomplex %s addr;\n", name);
		for(j=0; j<t->n; j++){
			name = fixname(t->tname[j]);
			tt = defer(t->t[j]);
			if(tt == nil){
				Bprint(b, "// oops nil %s\n", name);
				continue;
			}
			switch(tt->ty){
			case Base:
			base:
				Bprint(b, "\tprint(indent, \"%s\t\", addr.%s, \"\\n\");\n",
					name, name);
				break;
			case Pointer:
				ttt = defer(tt->sub);
				if(ttt && ttt->ty == Aggr)
					Bprint(b, "\tprint(indent, \"%s\t(%s)\", addr.%s, \"\\n\");\n",
						name, nameof(ttt, 1), name);
				else
					goto base;
				break;
			case Array:
				Bprint(b, "\tprint(indent, \"%s\t\", addr.%s\\X, \"\\n\");\n",
					name, name);
				break;
			case Enum:
				Bprint(b, "\tprint(indent, \"%s\t\", addr.%s, \" \", %s(addr.%s), \"\\n\");\n",
					name, name, nameof(tt, 1), name);
				break;
			case Aggr:
				Bprint(b, "\tprint(indent, \"%s\t%s{\\n\");\n",
					name, nameof(tt, 0));
				Bprint(b, "\tindent_%s(addr+%lud, indent+\"  \");\n",
					nameof(tt, 1), t->val[j]);
				Bprint(b, "\tprint(indent, \"}\\n\");\n");
				break;
			}
		}
		Bprint(b, "};\n\n");
		break;
	
	case Enum:
		name = nameof(t, 1);
		Bprint(b, "// enum %s\n", name);
		for(j=0; j<t->n; j++)
			Bprint(b, "%s = %ld;\n", fixname(t->tname[j]), t->val[j]);
	
		Bprint(b, "vals_%s = {\n", name);
		for(j=0; j<t->n; j++)
			Bprint(b, "\t%lud,\n", t->val[j]);
		Bprint(b, "};\n");
		Bprint(b, "names_%s = {\n", name);
		for(j=0; j<t->n; j++)
			Bprint(b, "\t\"%s\",\n", fixname(t->tname[j]));
		Bprint(b, "};\n");
		Bprint(b, "defn\n%s(val) {\n", name);
		Bprint(b, "\tlocal i;\n");
		Bprint(b, "\ti = match(val, vals_%s);\n", name);
		Bprint(b, "\tif i >= 0 then return names_%s[i];\n", name);
		Bprint(b, "\treturn \"???\";\n");
		Bprint(b, "};\n");
		break;
	}
}

void
printtypes(Biobuf *b)
{
	int i, n, nn;
	Type *t, *tt, **all;
	TypeList *tl;

	/* check that pointer resolved */
	for(tl=alltypes; tl; tl=tl->tl){
		t = tl->hd;
		if(t->ty==None){
			if(t->n1 || t->n2)
				warn("type %d,%d referenced but not defined", t->n1, t->n2);
			else if(t->sue && t->suename)
				warn("%s %s referenced but not defined",
					t->sue=='s' ? "struct" :
					t->sue=='u' ? "union" :
					t->sue=='e' ? "enum" : "???", t->suename);
		}
	}

	/* push typedefs down, base types up */
	n = 0;
	for(tl=alltypes; tl; tl=tl->tl){
		n++;
		t = tl->hd;
		if(t->ty == Typedef || t->ty == Defer)
			dotypedef(t);
	}

	/* push ranges around */
	for(tl=alltypes; tl; tl=tl->tl)
		dorange(tl->hd);

	/*
	 * only take one type of a given name; acid is going to do this anyway,
	 * and this will reduce the amount of code we output considerably.
	 * we could run a DFA equivalence relaxation sort of algorithm
	 * to find the actual equivalence classes, and then rename types 
	 * appropriately, but this will do for now.
	 */
	all = emalloc(n*sizeof(all[0]));
	n = 0;
	for(tl=alltypes; tl; tl=tl->tl)
		all[n++] = tl->hd;

	qsort(all, n, sizeof(all[0]), qtypecmp);

	nn = 0;
	for(i=0; i<n; i++){
		if(i==0 || typecmp(all[i-1], all[i]) != 0)
			all[nn++] = all[i];
		else
			all[i]->equiv = all[nn-1];
	}

	for(tl=alltypes; tl; tl=tl->tl){
		t = tl->hd;
		tt = defer(t);
		if(tt && tt->equiv)
			t->equiv = tt->equiv;
	}

	for(i=0; i<nn; i++)
		printtype(b, all[i]);

	free(all);
}

void
freetypes(void)
{
	memset(thash, 0, sizeof(thash));
	memset(namehash, 0, sizeof namehash);
}
