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
	"var",		"$var",
	"whatis",		"$whatis",
	"while",		"$while",

/* register names! */
	/* generic */
	"PC",		"$PC",
	"SP",		"$SP",
	"LR",		"$LR",
	"R0",		"$R0",
	"R1",		"$R1",
	"R2",		"$R2",
	"R3",		"$R3",
	"R4",		"$R4",
	"R5",		"$R5",
	"R6",		"$R6",
	"R7",		"$R7",
	"R8",		"$R8",
	"R9",		"$R9",
	"R10",		"$R10",
	"R11",		"$R11",
	"R12",		"$R12",
	"R13",		"$R13",
	"R14",		"$R14",
	"R15",		"$R15",
	"R16",		"$R16",
	"R17",		"$R17",
	"R18",		"$R18",
	"R19",		"$R19",
	"R20",		"$R20",
	"R21",		"$R21",
	"R22",		"$R22",
	"R23",		"$R23",
	"R24",		"$R24",
	"R25",		"$R25",
	"R26",		"$R26",
	"R27",		"$R27",
	"R28",		"$R28",
	"R29",		"$R29",
	"R30",		"$R30",
	"R31",		"$R31",
	"E0",		"$E0",
	"E1",		"$E1",
	"E2",		"$E2",
	"E3",		"$E3",
	"E4",		"$E4",
	"E5",		"$E5",
	"E6",		"$E6",
	"F0",		"$F0",
	"F1",		"$F1",
	"F2",		"$F2",
	"F3",		"$F3",
	"F4",		"$F4",
	"F5",		"$F5",
	"F6",		"$F6",
	"F7",		"$F7",
	"F8",		"$F8",
	"F9",		"$F9",
	"F10",		"$F10",
	"F11",		"$F11",
	"F12",		"$F12",
	"F13",		"$F13",
	"F14",		"$F14",
	"F15",		"$F15",
	"F16",		"$F16",
	"F17",		"$F17",
	"F18",		"$F18",
	"F19",		"$F19",
	"F20",		"$F20",
	"F21",		"$F21",
	"F22",		"$F22",
	"F23",		"$F23",
	"F24",		"$F24",
	"F25",		"$F25",
	"F26",		"$F26",
	"F27",		"$F27",
	"F28",		"$F28",
	"F29",		"$F29",
	"F30",		"$F30",
	"F31",		"$F31",

	/* 386 */
	"DI",		"$DI",
	"SI",		"$SI",
	"BP",		"$BP",
	"BX",		"$BX",
	"DX",		"$DX",
	"CX",		"$CX",
	"AX",		"$AX",
	"GS",		"$GS",
	"FS",		"$FS",
	"ES",		"$ES",
	"DS",		"$DS",
	"TRAP",		"$TRAP",
	"ECODE",		"$ECODE",
	"CS",		"$CS",
	"EFLAGS",		"$EFLAGS",
	"SS",		"$SS",

	/* power */
	"CAUSE",		"$CAUSE",
	"SRR1",		"$SRR1",
	"CR",		"$CR",
	"XER",		"$XER",
	"CTR",		"$CTR",
	"VRSAVE",		"$VRSAVE",
	"FPSCR",		"$FPSCR"
};

char*
nonempty(char *name)
{
	if(name[0] == '\0')
		return "__empty__name__";
	return name;
}

char*
cleanstl(char *name)
{
	char *b, *p;
	static char buf[65536];	/* These can be huge. */

	if(strchr(name, '<') == nil)
		return nonempty(name);

	b = buf;
	for(p = name; *p != 0; p++){
		switch(*p){
		case '<':
			strcpy(b, "_L_");
			b += 3;
			break;
		case '>':
			strcpy(b, "_R_");
			b += 3;
			break;
		case '*':
			strcpy(b, "_A_");
			b += 3;
			break;
		case ',':
			strcpy(b, "_C_");
			b += 3;
			break;
		case '.':
			strcpy(b, "_D_");
			b += 3;
			break;
		default:
			*b++ = *p;
			break;
		}
	}
	*b = 0;
	return buf;
}

char*
fixname(char *name)
{
	int i;
	char *s;
	static int nbuf;
	static char buf[8][65536];

	if(name == nil)
		return nil;
	s = demangle(name, buf[nbuf], 1);
	if(s != name){
		if(++nbuf == nelem(buf))
			nbuf = 0;
		name = s;
	}
	for(i=0; i<nelem(fixes); i++)
		if(name[0]==fixes[i].old[0] && strcmp(name, fixes[i].old)==0)
			return nonempty(fixes[i].new);
	return nonempty(name);
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
	int n;
	Type *t, *tt;

	for(; tl; tl=tl->tl){
		t = tl->hd;
		tt = typebynum(n1, t->n2);
		*tt = *t;
		tt->n1 = n1;
		if(tt->n){
			n = (tt->n+31)&~31;
			if(tt->tname){
				tt->tname = emalloc(n*sizeof tt->tname[0]);
				memmove(tt->tname, t->tname, n*sizeof tt->tname[0]);
			}
			if(tt->val){
				tt->val = emalloc(n*sizeof tt->val[0]);
				memmove(tt->val, t->val, n*sizeof tt->val[0]);
			}
			if(tt->t){
				tt->t = emalloc(n*sizeof tt->t[0]);
				memmove(tt->t, t->t, n*sizeof tt->t[0]);
			}
		}
		addhash(tt);
	}
}

Type*
defer(Type *t)
{
	Type *u, *oldt;
	int n;

	if(t == nil)
		return nil;

/* XXX rob has return t; here */
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
	if(oldt != t)
		oldt->sub = t;
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
mkname(char *prefix, char *name)
{
	static char buf[65536];

	snprint(buf, sizeof buf, "%s%s", prefix, name);
	return buf;
}

char*
nameof(Type *t, int doanon)
{
	static char buf[65536];
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
		if(isspace((uchar)*p))
			*p = '_';
	return buf;
}

static char
basecharof(Type *t)	/*XXX */
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

int careaboutaggrcount;

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

	if(careaboutaggrcount && t->ty == Aggr){
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
		Bprint(b, "%B = %lud;\n", mkname("sizeof", name), t->xsizeof);
		Bprint(b, "aggr %B {\n", name);
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
if(0){
Bprint(b, "// t->t[j] = %p\n", ttt=t->t[j]);
while(ttt){
Bprint(b, "// %s %d (%d,%d) sub %p\n", ttt->name, ttt->ty, ttt->n1, ttt->n2, ttt->sub);
ttt=ttt->sub;
}
}
			case Base:
			case Pointer:
			case Enum:
			case Array:
			case Function:
				nprint++;
				Bprint(b, "\t'%c' %lud %B;\n", basecharof(tt), t->val[j], fixname(t->tname[j]));
				break;
			case Aggr:
				nprint++;
				Bprint(b, "\t%B %lud %B;\n", nameof(tt, 1), t->val[j], fixname(t->tname[j]));
				break;
			}
		}
		if(nprint == 0)
			Bprint(b, "\t'X' 0 __dummy;\n");
		Bprint(b, "};\n\n");

		name = nameof(t, 1);	/* might have smashed it */
		Bprint(b, "defn %B(addr) { %B(addr, \"\"); }\n", name, mkname("indent_", name));
		Bprint(b, "defn %B(addr, indent) {\n", mkname("indent_", name));
		Bprint(b, "\tcomplex %B addr;\n", name);
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
				Bprint(b, "\tprint(indent, \"%s\t\", addr.%B, \"\\n\");\n",
					name, name);
				break;
			case Pointer:
				ttt = defer(tt->sub);
				if(ttt && ttt->ty == Aggr)
					Bprint(b, "\tprint(indent, \"%s\t(%s)\", addr.%B, \"\\n\");\n",
						name, nameof(ttt, 1), name);
				else
					goto base;
				break;
			case Array:
				Bprint(b, "\tprint(indent, \"%s\t\", addr.%B\\X, \"\\n\");\n",
					name, name);
				break;
			case Enum:
				Bprint(b, "\tprint(indent, \"%s\t\", addr.%B, \" \", %B(addr.%B), \"\\n\");\n",
					name, name, nameof(tt, 1), name);
				break;
			case Aggr:
				Bprint(b, "\tprint(indent, \"%s\t%s{\\n\");\n",
					name, nameof(tt, 0));
				Bprint(b, "\t%B(addr+%lud, indent+\"  \");\n",
					mkname("indent_", nameof(tt, 1)), t->val[j]);
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
			Bprint(b, "%B = %ld;\n", fixname(t->tname[j]), t->val[j]);

		Bprint(b, "%B = {\n", mkname("vals_", name));
		for(j=0; j<t->n; j++)
			Bprint(b, "\t%lud,\n", t->val[j]);
		Bprint(b, "};\n");
		Bprint(b, "%B = {\n", mkname("names_", name));
		for(j=0; j<t->n; j++)
			Bprint(b, "\t\"%s\",\n", fixname(t->tname[j]));
		Bprint(b, "};\n");
		Bprint(b, "defn %B(val) {\n", name);
		Bprint(b, "\tlocal i;\n");
		Bprint(b, "\ti = match(val, %B);\n", mkname("vals_", name));
		Bprint(b, "\tif i >= 0 then return %B[i];\n", mkname("names_", name));
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
				warn("type %d,%d referenced but not defined - %p", t->n1, t->n2, t);
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

	careaboutaggrcount = 1;
	qsort(all, n, sizeof(all[0]), qtypecmp);
	careaboutaggrcount = 0;

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
