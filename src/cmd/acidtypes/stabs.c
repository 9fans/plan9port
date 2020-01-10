#include <u.h>
#include <errno.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>
#include <stabs.h>
#include <ctype.h>
#include "dat.h"

static jmp_buf kaboom;

static Type *parsename(char*, char**);
static Type *parseinfo(char*, char**);
static int parsenum(char*, int*, int*, char**);
static int parseattr(char*, char**, char**);
static Type *parsedefn(char *p, Type *t, char **pp);
static int parsebound(char**);
static vlong parsebigint(char**);

typedef struct Ftypes Ftypes;
struct Ftypes
{
	Ftypes *down;
	Ftypes *next;
	char *file;
	TypeList *list;
};

Ftypes *fstack;
Ftypes *allftypes;

static char*
estrndup(char *s, int n)
{
	char *t;

	t = emalloc(n+1);
	memmove(t, s, n);
	return t;
}

static char*
mkpath(char *dir, char *name)
{
	char *s;
	if(name[0] == '/' || dir == nil)
		return estrdup(name);
	else{
		s = emalloc(strlen(dir)+strlen(name)+1);
		strcpy(s, dir);
		strcat(s, name);
		return s;
	}
}

static Ftypes*
mkftypes(char *dir, char *name)
{
	Ftypes *f;

	f = emalloc(sizeof(*f));
	f->file = mkpath(dir, name);
	f->next = allftypes;
	allftypes = f;
	return f;
}

static Ftypes*
findftypes(char *dir, char *name)
{
	char *s;
	Ftypes *f, *found;

	found = nil;
	s = mkpath(dir, name);
	for(f=allftypes; f; f=f->next)
		if(strcmp(f->file, s) == 0)
			found = f;
	return found;
}

static void
oops(void)
{
	longjmp(kaboom, 1);
}

/* find a : but skip over :: */
static char*
findcolon(char *p)
{
	while((p = strchr(p, ':')) != nil && *(p+1) == ':')
		p += 2;
	if(p == nil)
		oops();
	return p;
}

static void
semi(char **p)
{
	if(**p != ';')
		oops();
	(*p)++;
}

static void
comma(char **p)
{
	if(**p != ',')
		oops();
	(*p)++;
}

static int
parseint(char **pp)
{
	if(!isdigit((uchar)**pp))
		oops();
	return strtol(*pp, pp, 10);
}

/*
	name ::= symbol_opt info
 */
static Type*
parsename(char *desc, char **pp)
{
	if(*desc == 'c')
		return nil;

	if(isdigit((uchar)*desc) || *desc=='-' || *desc=='(')
		return parseinfo(desc, pp);
	if(*desc == 0)
		oops();
	return parseinfo(desc+1, pp);
}

/*
	info ::= num | num '=' attr* defn
 */
static Type*
parseinfo(char *desc, char **pp)
{
	int n1, n2;
	Type *t;
	char *attr;

	n1 = n2 = 0;
	parsenum(desc, &n1, &n2, &desc);
	t = typebynum(n1, n2);
	if(*desc != '='){
		*pp = desc;
		return t;
	}
	desc++;
	if(fstack)
		fstack->list = mktl(t, fstack->list);
	while(parseattr(desc, &attr, &desc) >= 0){
		if(*attr == 's')
			t->xsizeof = atoi(attr+1)/8;
	}
	return parsedefn(desc, t, pp);
}

/*
	num ::= integer | '(' integer ',' integer ')'
*/
static int
parsenum(char *p, int *n1, int *n2, char **pp)
{
	if(isdigit((uchar)*p)){
		*n1 = strtol(p, &p, 10);
		*n2 = 0;
		*pp = p;
		return 0;
	}
	if(*p == '('){
		*n1 = strtol(p+1, &p, 10);
		if(*p != ',')
			oops();
		*n2 = strtol(p+1, &p, 10);
		if(*p != ')')
			oops();
		*pp = p+1;
		return 0;
	}
	oops();
	return -1;
}

/*
	attr ::= '@' text ';'

	text is
		'a' integer (alignment)
		'p' integer (pointer class)
		'P' (packed type)
		's' integer (size of type in bits)
		'S' (string instead of array of chars)
*/
static int
parseattr(char *p, char **text, char **pp)
{
	if(*p != '@')
		return -1;
	*text = p+1;
	if((p = strchr(p, ';')) == nil)
		oops();
	*pp = p+1;
	return 0;
}


typedef struct Basic Basic;
struct Basic
{
	int size;
	int fmt;
};

static Basic baseints[] =
{
	0, 0,
/*1*/	4, 'd',	/* int32 */
/*2*/	1, 'd',	/* char8 */
/*3*/	2, 'd',	/* int16 */
/*4*/	4, 'd',	/* long int32 */
/*5*/	1, 'x',	/* uchar8 */
/*6*/	1, 'd',	/* schar8 */
/*7*/	2, 'x',	/* uint16 */
/*8*/	4, 'x',	/* uint32 */
/*9*/	4, 'x',	/* uint32 */
/*10*/	4, 'x',	/* ulong32 */
/*11*/	0, 0,		/* void */
/*12*/	4, 'f',		/* float */
/*13*/	8, 'f',		/* double */
/*14*/	10, 'f',	/* long double */
/*15*/	4, 'd',	/* int32 */
/*16*/	4, 'd',	/* bool32 */
/*17*/	2, 'f',		/* short real */
/*18*/	4, 'f',		/* real */
/*19*/	4, 'x',	/* stringptr */
/*20*/	1, 'd',	/* character8 */
/*21*/	1, 'x',	/* logical*1 */
/*22*/	2, 'x',	/* logical*2 */
/*23*/	4, 'X',	/* logical*4 */
/*24*/	4, 'X',	/* logical32 */
/*25*/	8, 'F',		/* complex (two single) */
/*26*/	16, 'F',	/* complex (two double) */
/*27*/	1, 'd',	/* integer*1 */
/*28*/	2, 'd',	/* integer*2 */
/*29*/	4, 'd',	/* integer*4 */
/*30*/	2, 'x',	/* wide char */
/*31*/	8, 'd',	/* int64 */
/*32*/	8, 'x',	/* uint64 */
/*33*/	8, 'x',	/* logical*8 */
/*34*/	8, 'd',	/* integer*8 */
};

static Basic basefloats[] =
{
	0,0,
/*1*/	4, 'f',	/* 32-bit */
/*2*/	8, 'f',	/* 64-bit */
/*3*/	8, 'F',	/* complex */
/*4*/	4, 'F',	/* complex16 */
/*5*/	8, 'F', /* complex32 */
/*6*/	10, 'f',	/* long double */
};

/*
	defn ::= info
		| 'b' ('u' | 's') 'c'? width; offset; nbits; 		(builtin, signed/unsigned, char/not, width in bytes, offset & nbits of type)
		| 'w'		(aix wide char type, not used)
		| 'R' fptype; bytes;			(fptype 1=32-bit, 2=64-bit, 3=complex, 4=complex16, 5=complex32, 6=long double)
		| 'g' typeinfo ';' nbits 		(aix floating, not used)
		| 'c' typeinfo ';' nbits		(aix complex, not used)
		| 'b' typeinfo ';' bytes		(ibm, no idea)
		| 'B' typeinfo		(volatile typref)
		| 'd' typeinfo		(file of typeref)
		| 'k' typeinfo		(const typeref)
		| 'M' typeinfo ';' length		(multiple instance type, fortran)
		| 'S' typeinfo		(set, typeref must have small number of values)
		| '*' typeinfo		(pointer to typeref)
		| 'x' ('s'|'u'|'e') name ':'	(struct, union, enum reference.  name can have '::' in it)
		| 'r' typeinfo ';' low ';' high ';'		(subrange.  typeref can be type being defined for base types!)
				low and high are bounds
				if bound is octal power of two, it's a big negative number
		| ('a'|'P') indextypedef arraytypeinfo 	(array, index should be range type)
					indextype is type definition not typeinfo (need not say typenum=)
					P means packed array
		| 'A' arraytypeinfo		(open array (no index bounds))
		| 'D' dims ';' typeinfo		(dims-dimensional dynamic array)
		| 'E' dims ';' typeinfo		(subarray of N-dimensional array)
		| 'n' typeinfo ';' bytes		(max length string)
		| 'z' typeinfo ';' bytes		(no idea what difference is from 'n')
		| 'N'					(pascal stringptr)
		| 'e' (name ':' bigint ',')* ';'	(enum listing)
		| ('s'|'u') bytes (name ':' type ',' bitoffset ',' bitsize ';')* ';'		(struct/union defn)
							tag is given as name in stabs entry, with 'T' symbol
		| 'f' typeinfo ';' 			(function returning type)
		| 'f' rettypeinfo ',' paramcount ';' (typeinfo ',' (0|1) ';')* ';'
		| 'p' paramcount ';' (typeinfo ',' (0|1) ';')* ';'
		| 'F' rettypeinfo ',' paramcount ';' (name ':' typeinfo ',' (0|1) ';')* ';'
		| 'R' paramcount ';' (name ':' typeinfo ',' (0|1) ';')* ';'
							(the 0 or 1 is pass-by-reference vs pass-by-value)
							(the 0 or 1 is pass-by-reference vs pass-by-value)
*/

static Type*
parsedefn(char *p, Type *t, char **pp)
{
	char c, *name;
	int ischar, namelen, n, wid, offset, bits, sign;
	long val;
	Type *tt;

	if(*p == '(' || isdigit((uchar)*p)){
		t->ty = Defer;
		t->sub = parseinfo(p, pp);
		return t;
	}

	switch(c = *p){
	case '-':	/* builtin */
		n = strtol(p+1, &p, 10);
		if(n >= nelem(baseints) || n < 0)
			n = 0;
		t->ty = Base;
		t->xsizeof = baseints[n].size;
		t->printfmt = baseints[n].fmt;
		break;
	case 'b':	/* builtin */
		p++;
		if(*p != 'u' && *p != 's')
			oops();
		sign = (*p == 's');
		p++;
		ischar = 0;
		if(*p == 'c'){
			ischar = 1;
			p++;
		}
		wid = parseint(&p);
		semi(&p);
		offset = parseint(&p);
		semi(&p);
		bits = parseint(&p);
		semi(&p);
		t->ty = Base;
		t->xsizeof = wid;
		if(sign == 1)
			t->printfmt = 'd';
		else
			t->printfmt = 'x';
		USED(bits);
		USED(ischar);
		break;
	case 'R':	/* fp type */
		n = parseint(&p);
		semi(&p);
		wid = parseint(&p);
		semi(&p);
		t->ty = Base;
		t->xsizeof = wid;
		if(n < 0 || n >= nelem(basefloats))
			n = 0;
		t->xsizeof = basefloats[n].size;
		t->printfmt = basefloats[n].fmt;
		break;
	case 'r':	/* subrange */
		t->ty = Range;
		t->sub = parseinfo(p+1, &p);
		if(*(p-1) == ';' && *p != ';')
			p--;
		semi(&p);
		t->lo = parsebound(&p);
		semi(&p);
		t->hi = parsebound(&p);
		semi(&p);
		break;
	case 'B':	/* volatile */
	case 'k':	/* const */
		t->ty = Defer;
		t->sub = parseinfo(p+1, &p);
		break;
	case '*':	/* pointer */
	case 'A':	/* open array */
	case '&':	/* reference */	/* guess - C++? (rob) */
		t->ty = Pointer;
		t->sub = parseinfo(p+1, &p);
		break;
	case 'a':	/* array */
	case 'P':	/* packed array */
		t->ty = Pointer;
		tt = newtype();
		parsedefn(p+1, tt, &p);	/* index type */
		if(*p == ';')
			p++;
		tt = newtype();
		t->sub = tt;
		parsedefn(p, tt, &p);	/* element type */
		break;
	case 'e':	/* enum listing */
		p++;
		t->sue = 'e';
		t->ty = Enum;
		while(*p != ';'){
			name = p;
			p = findcolon(p)+1;
			namelen = (p-name)-1;
			val = parsebigint(&p);
			comma(&p);
			if(t->n%32 == 0){
				t->tname = erealloc(t->tname, (t->n+32)*sizeof(t->tname[0]));
				t->val = erealloc(t->val, (t->n+32)*sizeof(t->val[0]));
			}
			t->tname[t->n] = estrndup(name, namelen);
			t->val[t->n] = val;
			t->n++;
		}
		semi(&p);
		break;

	case 's':	/* struct */
	case 'u':	/* union */
		p++;
		t->sue = c;
		t->ty = Aggr;
		n = parseint(&p);
		while(*p != ';'){
			name = p;
			p = findcolon(p)+1;
			namelen = (p-name)-1;
			tt = parseinfo(p, &p);
			comma(&p);
			offset = parseint(&p);
			comma(&p);
			bits = parseint(&p);
			semi(&p);
			if(t->n%32 == 0){
				t->tname = erealloc(t->tname, (t->n+32)*sizeof(t->tname[0]));
				t->val = erealloc(t->val, (t->n+32)*sizeof(t->val[0]));
				t->t = erealloc(t->t, (t->n+32)*sizeof(t->t[0]));
			}
			t->tname[t->n] = estrndup(name, namelen);
			t->val[t->n] = offset;
			t->t[t->n] = tt;
			t->n++;
		}
		semi(&p);
		break;

	case 'x':	/* struct, union, enum reference */
		p++;
		t->ty = Defer;
		if(*p != 's' && *p != 'u' && *p != 'e')
			oops();
		c = *p;
		name = p+1;
		p = findcolon(p+1);
		name = estrndup(name, p-name);
		t->sub = typebysue(c, name);
		p++;
		break;

#if 0	/* AIX */
	case 'f':	/* function */
	case 'p':	/* procedure */
	case 'F':	/* Pascal function */
	/* case 'R':	/* Pascal procedure */
		/*
		 * Even though we don't use the info, we have
		 * to parse it in case it is embedded in other descriptions.
		 */
		t->ty = Function;
		p++;
		if(c == 'f' || c == 'F'){
			t->sub = parseinfo(p, &p);
			if(*p != ','){
				if(*p == ';')
					p++;
				break;
			}
			comma(&p);
		}
		n = parseint(&p);	/* number of params */
		semi(&p);
		while(*p != ';'){
			if(c == 'F' || c == 'R'){
				name = p;		/* parameter name */
				p = findcolon(p)+1;
			}
			parseinfo(p, &p);	/* param type */
			comma(&p);
			parseint(&p);	/* bool: passed by value? */
			semi(&p);
		}
		semi(&p);
		break;
#endif

	case 'f':	/* static function */
	case 'F':	/* global function */
		t->ty = Function;
		p++;
		t->sub = parseinfo(p, &p);
		break;

	/*
	 * We'll never see any of this stuff.
	 * When we do, we can worry about it.
	 */
	case 'D':	/* n-dimensional array */
	case 'E':	/* subarray of n-dimensional array */
	case 'M':	/* fortran multiple instance type */
	case 'N':	/* pascal string ptr */
	case 'S':	/* set */
	case 'c':	/* aix complex */
	case 'd':	/* file of */
	case 'g':	/* aix float */
	case 'n':	/* max length string */
	case 'w':	/* aix wide char */
	case 'z':	/* another max length string */
	default:
		fprint(2, "unsupported type char %c (%d)\n", *p, *p);
		oops();
	}
	*pp = p;
	return t;
}

/*
	bound ::=
		'A' offset			(bound is on stack by ref at offset offset from arg list)
		| 'T' offset			(bound is on stack by val at offset offset from arg list)
		| 'a' regnum		(bound passed by reference in register)
		| 't' regnum		(bound passed by value in register)
		| 'J'				(no bound)
		| bigint
*/
static int
parsebound(char **pp)
{
	char *p;
	int n;

	n = 0;
	p = *pp;
	switch(*p){
	case 'A':	/* bound is on stack by reference at offset n from arg list */
	case 'T':	/* bound is on stack by value at offset n from arg list */
	case 'a':	/* bound is passed by reference in register n */
	case 't':	/* bound is passed by value in register n */
		p++;
		parseint(&p);
		break;
	case 'J':	/* no bound */
		p++;
		break;
	default:
		n = parsebigint(&p);
		break;
	}
	*pp = p;
	return n;
}

/*
	bigint ::= '-'? decimal
		| 0 octal
		| -1
 */
static vlong
parsebigint(char **pp)
{
	char *p;
	int n, neg;

	p = *pp;
	if(*p == '0'){
		errno = 0;
		n = strtoll(p, &p, 8);
		if(errno)
			n = 0;
		goto out;
	}
	neg = 0;
	if(*p == '-'){
		neg = 1;
		p++;
	}
	if(!isdigit((uchar)*p))
		oops();
	n = strtol(p, &p, 10);
	if(neg)
		n = -n;

out:
	*pp = p;
	return n;
}

int
stabs2acid(Stab *stabs, Biobuf *b)
{
	volatile int fno, i;
	char c, *desc, *p;
	char *volatile dir, *volatile fn, *volatile name;
	Ftypes *f;
	Type *t, *tt;
	StabSym sym;

	dir = nil;
	fno = 0;
	fn = nil;
	for(i=0; stabsym(stabs, i, &sym)>=0; i++){
		if(verbose)
			print("%d %s\n", sym.type, sym.name);
		switch(sym.type){
		case N_SO:
			if(sym.name){
				if(sym.name[0] && sym.name[strlen(sym.name)-1] == '/')
					dir = sym.name;
			}
			denumber();
			fstack = nil;
			fno = 0;
			break;
		case N_BINCL:
			fno++;
			f = mkftypes(dir, sym.name);
			f->down = fstack;
			fstack = f;
			break;
		case N_EINCL:
			if(fstack)
				fstack = fstack->down;
			break;
		case N_EXCL:
			fno++;
			if((f = findftypes(dir, sym.name)) == nil){
				static int cannotprint;

				if(cannotprint++ == 0)
					fprint(2, "cannot find remembered %s\n", sym.name);
				continue;
			}
			renumber(f->list, fno);
			break;
		case N_GSYM:
		case N_FUN:
		case N_PSYM:
		case N_LSYM:
		case N_LCSYM:
		case N_STSYM:
		case N_RSYM:
			name = sym.name;
			if(name == nil){
				if(sym.type==N_FUN)
					fn = nil;
				continue;
			}
			if((p = findcolon(name)) == nil)
				continue;
			name = estrndup(name, p-name);
			desc = ++p;
			c = *desc;
			if(c == 'c'){
				fprint(2, "skip constant %s\n", name);
				continue;
			}
			if(setjmp(kaboom)){
				static int cannotparse;

				if(cannotparse++ == 0)
					fprint(2, "cannot parse %s\n", name);
				continue;
			}
			t = parsename(desc, &p);
			if(t == nil)
				continue;
			if(*p != 0){
				static int extradesc;

				if(extradesc++ == 0)
					fprint(2, "extra desc '%s' in '%s'\n", p, desc);
			}
			/* void is defined as itself */
			if(t->ty==Defer && t->sub==t && strcmp(name, "void")==0){
				t->ty = Base;
				t->xsizeof = 0;
				t->printfmt = '0';
			}
			if(*name==' ' && *(name+1) == 0)
				*name = 0;
			/* attach names to structs, unions, enums */
			if(c=='T' && *name && t->sue){
				t->suename = name;
				if(t->name == nil)
					t->name = name;
				tt = typebysue(t->sue, name);
				tt->ty = Defer;
				tt->sub = t;
			}
			if(c=='t'){
				tt = newtype();
				tt->ty = Typedef;
				tt->name = name;
				tt->sub = t;
			}
			/* define base c types */
			if(t->ty==None || t->ty==Range){
				if(strcmp(name, "char") == 0){
					t->ty = Base;
					t->xsizeof = 1;
					t->printfmt = 'x';
				}
				if(strcmp(name, "int") == 0){
					t->ty = Base;
					t->xsizeof = 4;
					t->printfmt = 'd';
				}
			}
			/* record declaration in list for later. */
			if(c != 't' && c != 'T')
			switch(sym.type){
			case N_GSYM:
				addsymx(nil, name, t);
				break;
			case N_FUN:
				fn = name;
				break;
			case N_PSYM:
			case N_LSYM:
			case N_LCSYM:
			case N_STSYM:
			case N_RSYM:
				addsymx(fn, name, t);
				break;
			}
			break;
		}
if(1) print("");
	}

	printtypes(b);
	dumpsyms(b);
	freetypes();

	return 0;
}
