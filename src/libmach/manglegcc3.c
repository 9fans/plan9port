/*
 * gcc3 name demangler.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>

typedef struct Chartab Chartab;
struct Chartab
{
	char c;
	char *s;
};

static char*
chartabsearch(Chartab *ct, int c)
{
	for(; ct->c; ct++)
		if(ct->c == c)
			return ct->s;
	return nil;
}

typedef struct Gccstate Gccstate;
struct Gccstate
{
	char *name[128];
	int nname;
};
static int gccname(char**, char**, Gccstate*);
char*
demanglegcc3(char *s, char *buf)
{
	char *p, *os;
	Gccstate state;

	state.nname = 0;
	os = s;
	/* mangled names always start with _Z */
	if(s[0] != '_' || s[1] != 'Z')
		return s;
	s += 2;

	p = buf;
	if(!gccname(&s, &p, &state)){
		if(strchr(os, '@') == nil)
			fprint(2, "demangle: %s\n");
		return os;
	}
	if(*s){
		/* the rest of the name is the argument types */
		*p++ = '(';
		while(*s != 0 && gccname(&s, &p, &state))
			*p++ = ',';
		if(*(p-1) == ',')
			p--;
		*p++ = ')';
	}
	*p = 0;
	return buf;
}

static Chartab stdnames[] =
{
	'a',	"std::allocator",
	'b',	"std::basic_string",
	'd',	"std::iostream",
	'i',	"std::istream",
	'o',	"std::ostream",
	's',	"std::string",
	0, 0
};

static Chartab typetab[] =
{
	'b',	"bool",
	'c',	"char",
	'd',	"double",
	'i',	"int",
	'j',	"uint",
	'v',	"void",
	0, 0
};

static struct {
	char *shrt;
	char *actual;
	char *lng;
} operators[] =
{
	"aN",	"&=",		"andeq",
	"aS",	"=",		"assign",
	"aa",	"&&",		"andand",
	"ad",	"&",		"and",
	"an",	"&",		"and",
	"cl",	"()",		"construct",
	"cm",	",",		"comma",
	"co",	"~",		"twiddle",
	"dV",	"/=",		"diveq",
	"da",	"delete[]",	"deletearray",
	"de",	"*",		"star",
	"dl",	"delete",	"delete",
	"dv",	"/",		"div",
	"eO",	"^=",		"xoreq",
	"eo",	"^",		"xor",
	"eq",	"==",		"eq",
	"ge",	">=",		"geq",
	"gt",	">",		"gt",
	"ix",	"[]",		"index",
	"IS",	"<<=",		"lsheq",
	"le",	"<=",		"leq",
	"ls",	"<<",		"lsh",
	"lt",	"<",		"lt",
	"ml",	"-=",		"subeq",
	"mL",	"*=",		"muleq",
	"mi",	"-",		"sub",
	"mI",	"*",		"mul",
	"mm",	"--",		"dec",
	"na",	"new[]",	"newarray",
	"ne",	"!=",		"neq",
	"ng",	"-",		"neg",
	"nt",	"!",		"not",
	"nw",	"new",		"new",
	"oR",	"|=",		"oreq",
	"oo",	"||",		"oror",
	"or",	"|",		"or",
	"pL",	"+=",		"addeq",
	"pl",	"+",		"add",
	"pm",	"->*",		"pointstoderef",
	"pp",	"++",		"inc",
	"ps",	"+",		"pos",
	"pt",	"->",		"pointsto",
	"qu",	"?",		"question",
	"rM",	"%=",		"modeq",
	"rS",	">>=",		"rsheq",
	"rm",	"%",		"mod",
	"rs",	">>",		"rsh",
	"st",	"sizeof",	"sizeoftype",
	"sz",	"sizeof",	"sizeofexpr",

	0,0,0
};

/*
 * Pick apart the next mangled name section.
 * Names and types are treated as the same.
 * Let's see how far we can go before that becomes a problem.
 */
static int
gccname(char **ps, char **pp, Gccstate *state)
{
	int i, n;
	char *os, *s, *t, *p;
	Gccstate nstate;

	s = *ps;
	os = s;
	p = *pp;

/*	print("\tgccname: %s\n", s); */

	/* overloaded operators */
	for(i=0; operators[i].shrt; i++){
		if(memcmp(operators[i].shrt, s, 2) == 0){
			strcpy(p, "operator$");
			strcat(p, operators[i].lng);
			p += strlen(p);
			s += 2;
			goto suffix;
		}
	}

	/* basic types */
	if((t = chartabsearch(typetab, *s)) != nil){
		s++;
		strcpy(p, t);
		p += strlen(t);
		goto suffix;
	}

	switch(*s){
	default:
	bad:
		fprint(2, "bad name: %s\n", s);
		return 0;

	case '1': case '2': case '3': case '4':	/* name length */
	case '5': case '6': case '7': case '8': case '9':
		n = strtol(s, &s, 10);
		memmove(p, s, n);
		p += n;
		s += n;
		break;

	case 'C':	/* C1: constructor? */
		strtol(s+1, &s, 10);
		strcpy(p, "constructor");
		p += strlen(p);
		break;

	case 'D':	/* D1: destructor? */
		strtol(s+1, &s, 10);
		strcpy(p, "destructor");
		p += strlen(p);
		break;

	case 'K':	/* const */
		s++;
		strcpy(p, "const ");
		p += strlen(p);
		if(!gccname(&s, &p, state))
			return 0;
		break;

	case 'L':	/* default value */
		t = s;
		s++;
		if(!gccname(&s, &p, state))
			return 0;
		if(!isdigit((uchar)*s)){
			fprint(2, "bad value: %s\n", t);
			return 0;
		}
		n = strtol(s, &s, 10);
		if(*s != 'E'){
			fprint(2, "bad value2: %s\n", t);
			return 0;
		}
		sprint(p, "=%d", n);
		p += strlen(p);
		s++;
		break;

	case 'N':	/* hierarchical name */
		s++;
		while(*s != 'E'){
			if(!gccname(&s, &p, state)){
				fprint(2, "bad name in hierarchy: %s in %s\n", s, os);
				return 0;
			}
			strcpy(p, "::");
			p += 2;
		}
		p -= 2;
		s++;
		break;

	case 'P':	/* pointer to */
		s++;
		if(!gccname(&s, &p, state))
			return 0;
		*p++ = '*';
		break;

	case 'R':	/* reference to */
		s++;
		if(!gccname(&s, &p, state))
			return 0;
		*p++ = '&';
		break;

	case 'S':	/* standard or previously-seen name */
		s++;
		if('0' <= *s && *s <= '9'){
			/* previously seen */
			t = s-1;
			n = strtol(s, &s, 10);
			if(*s != '_'){
				fprint(2, "bad S: %s\n", t);
				return 0;
			}
			s++;
			sprint(p, "S%d_", n);
			p += strlen(p);
			break;
		}
		/* SA_ ??? */
		if(*s == 'A' && *(s+1) == '_'){
			strcpy(p, "SA_");
			p += 3;
			s += 2;
			break;
		}

		/* standard name */
		if(*s == 't'){
			strcpy(p, "std::");
			p += 5;
			s++;
			if(!gccname(&s, &p, state))
				return 0;
		}else if((t = chartabsearch(stdnames, *s)) != nil){
			strcpy(p, t);
			p += strlen(p);
			s++;
		}else{
			strcpy(p, "std::");
			p += 5;
			*p++ = *s++;
		}
		break;

	case 'T':	/* previously-seen type??? T0_ also T_*/
		t = s;
		for(; *s != '_'; s++){
			if(*s == 0){
				s = t;
				goto bad;
			}
		}
		s++;
		memmove(p, t, s-t);
		p += s-t;
		break;
	}

suffix:
	if(*s == 'I'){
		/* template suffix */
		nstate.nname = 0;
		*p++ = '<';
		s++;
		while(*s != 'E'){
			if(!gccname(&s, &p, &nstate)){
				fprint(2, "bad name in template: %s\n", s);
				return 0;
			}
			*p++ = ',';
		}
		*(p-1) = '>';
		s++;
	}

	*ps = s;
	*pp = p;
	return 1;
}
