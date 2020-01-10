/*
 * gcc2 name demangler.
 *
 * gcc2 follows the C++ Annotated Reference Manual section 7.2.1
 * name mangling description with a few changes.
 * See gpcompare.texi, gxxint_15.html in this directory for the changes.
 *
 * Not implemented:
 *	unicode mangling
 *	renaming of operator functions
 */
/*
RULES TO ADD:

_10CycleTimer.cycles_per_ms_ => CycleTimer::cycles_per_ms_


*/
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>

#define debug 0

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

static Chartab typetab[] =
{
	'b',	"bool",
	'c',	"char",
	'd',	"double",
	'e',	"...",
	'f',	"float",
	'i',	"int",
	'J',	"complex",
	'l',	"long",
	'r',	"long double",
	's',	"short",
	'v',	"void",
	'w',	"wchar_t",
	'x',	"long long",
	0, 0
};

static Chartab modifiertab[] =
{
	'C',	"const",
	'S',	"signed",		/* means static for member functions */
	'U',	"unsigned",
	'V',	"volatile",

	'G',	"garbage",	/* no idea what this is */
	0, 0
};

static char constructor[] = "constructor";
static char destructor[] = "destructor";
static char gconstructor[] = "$gconstructor";	/* global destructor */
static char gdestructor[] = "$gdestructor";	/* global destructor */

static char manglestarts[] = "123456789CFHQSUVt";

static int gccname(char**, char**);
static char *demanglegcc2a(char*, char*);
static char *demanglegcc2b(char*, char*);
static char *demanglegcc2c(char*, char*);
static int gccnumber(char**, int*, int);

char*
demanglegcc2(char *s, char *buf)
{
	char *name, *os, *p, *t;
	int isfn, namelen;


	/*
	 * Pick off some cases that seem not to fit the pattern.
	 */
	if((t = demanglegcc2a(s, buf)) != nil)
		return t;
	if((t = demanglegcc2b(s, buf)) != nil)
		return t;
	if((t = demanglegcc2c(s, buf)) != nil)
		return t;

	/*
	 * First, figure out whether this is a mangled name.
	 * The name begins with a short version of the name, then __.
	 * Of course, some C names begin with __ too, so the ultimate
	 * test is whether what follows __ looks reasonable.
	 * We use a test on the first letter instead.
	 *
	 * Constructors have no name - they begin __ (double underscore).
	 * Destructors break the rule - they begin _._ (underscore, dot, underscore).
	 */
	os = s;
	isfn = 0;
	if(memcmp(s, "_._", 3) == 0){
		isfn = 1;
		name = destructor;
		namelen = strlen(name);
		s += 3;
	}else if(memcmp(s, "_GLOBAL_.D.__", 13) == 0){
		isfn = 1;
		name = gdestructor;
		namelen = strlen(name);
		s += 13;
	}else if(memcmp(s, "_GLOBAL_.D._", 12) == 0){
		isfn = 0;
		name = gdestructor;
		namelen = strlen(name);
		s += 12;
	}else if(memcmp(s, "_GLOBAL_.I.__", 13) == 0){
		isfn = 1;
		name = gconstructor;
		namelen = strlen(name);
		s += 13;
	}else if(memcmp(s, "_GLOBAL_.I._", 12) == 0){
		isfn = 0;
		name = gconstructor;
		namelen = strlen(name);
		s += 12;
	}else{
		t = strstr(os, "__");
		if(t == nil)
			return os;
		do{
			s = t;
			if(strchr(manglestarts, *(s+2)))
				break;
		}while((t = strstr(s+1, "__")) != nil);

		name = os;
		namelen = s - os;
		if(namelen == 0){
			isfn = 1;
			name = constructor;
			namelen = strlen(name);
		}
		s += 2;
	}

	/*
	 * Now s points at the mangled crap (maybe).
	 * and name is the final element of the name.
	 */
	if(strchr(manglestarts, *s) == nil)
		return os;

	p = buf;
	if(*s == 'F'){
		/* global function, no extra name pieces, just types */
		isfn = 1;
	}else{
		/* parse extra name pieces */
		if(!gccname(&s, &p)){
			if(debug)
				fprint(2, "parsename %s: %r\n", s);
			return os;
		}

		/* if we have a constructor or destructor, try to use the C++ name */
		t = nil;
		if(name == constructor || name == destructor){
			*p = 0;
			t = strrchr(buf, ':');
			if(t)
				t++;
			else
				t = buf;
		}
		strcpy(p, "::");
		p += 2;
		if(t){
			namelen = strlen(t)-2;
			if(name == destructor)
				*p++ = '~';
			name = t;
		}
	}
	if(p >= buf+2 && memcmp(p-2, "::", 2) == 0 && *(p-3) == ')')
		p -= 2;
	memmove(p, name, namelen);
	p += namelen;

	if(*s == 'F'){
		/* might be from above, or might follow name pieces */
		s++;
		isfn = 1;
	}

	/* the rest of the name is argument types - could skip this */
	if(*s || isfn){
		*p++ = '(';
		while(*s != 0 && *s != '_'){
			if(!gccname(&s, &p))
				break;
			*p++ = ',';
		}
		if(*(p-1) == ',')
			p--;
		*p++ = ')';
	}

	if(*s == '_'){
		/* return type (left over from H) */
	}

	*p = 0;
	return buf;
}

/*
 * _10CycleTimer.cycles_per_ms_ => CycleTimer::cycles_per_ms_
 * _t12basic_string3ZcZt11char_traits1ZcZt9allocator1Zc.npos
 * (maybe the funny syntax means they are private)
 */
static char*
demanglegcc2a(char *s, char *buf)
{
	char *p;

	if(*s != '_' || strchr(manglestarts, *(s+1)) == nil)
		return nil;
	p = buf;
	s++;
	if(!gccname(&s, &p))
		return nil;
	if(*s != '.')
		return nil;
	s++;
	strcpy(p, "::");
	p += 2;
	strcpy(p, s);
	return buf;
}

/*
 * _tfb => type info for bool
 * __vt_7ostream => vtbl for ostream
 */
static char*
demanglegcc2b(char *s, char *buf)
{
	char *p;
	char *t;

	if(memcmp(s, "__ti", 4) == 0){
		t = "$typeinfo";
		s += 4;
	}else if(memcmp(s, "__tf", 4) == 0){
		t = "$typeinfofn";
		s += 4;
	}else if(memcmp(s, "__vt_", 5) == 0){
		t = "$vtbl";
		s += 5;
	}else
		return nil;

	p = buf;
	for(;;){
		if(*s == 0 || !gccname(&s, &p))
			return nil;
		if(*s == 0)
			break;
		if(*s != '.' && *s != '$')
			return nil;
		strcpy(p, "::");
		p += 2;
		s++;
	}
	strcpy(p, "::");
	p += 2;
	strcpy(p, t);
	return buf;
}

/*
 * __thunk_176__._Q210LogMessage9LogStream => thunk (offset -176) for LogMessage::LogStream
 */
static char*
demanglegcc2c(char *s, char *buf)
{
	int n;
	char *p;

	if(memcmp(s, "__thunk_", 8) != 0)
		return nil;
	s += 8;
	if(!gccnumber(&s, &n, 1))
		return nil;
	if(memcmp(s, "__._", 4) != 0)	/* might as well be morse code */
		return nil;
	s += 4;
	p = buf;
	if(!gccname(&s, &p))
		return nil;
	strcpy(p, "::$thunk");
	return buf;
}

/*
 * Parse a number, a non-empty run of digits.
 * If many==0, then only one digit is used, even
 * if it is followed by more.  When we need a big
 * number in a one-digit slot, it gets bracketed by underscores.
 */
static int
gccnumber(char **ps, int *pn, int many)
{
	char *s;
	int n, eatunderscore;

	s = *ps;
	eatunderscore = 0;
	if(!many && *s == '_'){
		many = 1;
		s++;
		eatunderscore = 1;
	}
	if(!isdigit((uchar)*s)){
	bad:
		werrstr("bad number %.20s", *ps);
		return 0;
	}
	if(many)
		n = strtol(s, &s, 10);
	else
		n = *s++ - '0';
	if(eatunderscore){
		if(*s != '_')
			goto bad;
		s++;
	}
	*ps = s;
	*pn = n;
	return 1;
}

/*
 * Pick apart the next mangled name section.
 * Names and types are treated as the same.
 * Let's see how far we can go before that becomes a problem.
 */
static int
gccname(char **ps, char **pp)
{
	int i, n, m, val;
	char *os, *s, *t, *p, *p0, *p1;

	s = *ps;
	os = s;
	p = *pp;

/*	print("\tgccname: %s\n", s); */

	/* basic types */
	if((t = chartabsearch(typetab, *s)) != nil){
		s++;
		strcpy(p, t);
		p += strlen(t);
		goto out;
	}

	/* modifiers */
	if((t = chartabsearch(modifiertab, *s)) != nil){
		s++;
		if(!gccname(&s, &p))
			return 0;
		/*
		 * These don't end up in the right place
		 * and i don't care anyway
		 * (AssertHeld__C17ReaderWriterMutex)
		 */
		/*
		*p++ = ' ';
		strcpy(p, t);
		p += strlen(p);
		*/
		goto out;
	}

	switch(*s){
	default:
	bad:
		if(debug)
			fprint(2, "gccname: %s (%s)\n", os, s);
		werrstr("bad name %.20s", s);
		return 0;

	case '1': case '2': case '3': case '4':	/* length-prefixed string */
	case '5': case '6': case '7': case '8': case '9':
		if(!gccnumber(&s, &n, 1))
			return 0;
		memmove(p, s, n);
		p += n;
		s += n;
		break;

	case 'A':	/* array */
		t = s;
		s++;
		if(!gccnumber(&s, &n, 1))
			return 0;
		if(*s != '_'){
			werrstr("bad array %.20s", t);
			return 0;
		}
		s++;
		sprint(p, "array[%d] ", n);
		p += strlen(p);
		break;

	case 'F':	/* function */
		t = s;
		s++;
		strcpy(p, "fn(");
		p += 3;
		/* arguments */
		while(*s && *s != '_')
			if(!gccname(&s, &p))
				return 0;
		if(*s != '_'){
			werrstr("unexpected end in function: %s", t);
			return 0;
		}
		s++;
		strcpy(p, " => ");
		p += 4;
		/* return type */
		if(!gccname(&s, &p))
			return 0;
		*p++ = ')';
		break;

	case 'H':	/* template specialization */
		if(memcmp(s-2, "__", 2) != 0)
			fprint(2, "wow: %s\n", s-2);
		t = s;
		s++;
		if(!gccnumber(&s, &n, 0))
			return 0;
		p0 = p;
		/* template arguments */
		*p++ = '<';
		for(i=0; i<n; i++){
			val = 1;
			if(*s == 'Z'){	/* argument is a type, not value */
				val = 0;
				s++;
			}
			if(!gccname(&s, &p))
				return 0;
			if(val){
				if(!gccnumber(&s, &m, 1))	/* gccnumber: 1 or 0? */
					return 0;
				sprint(p, "=%d", m);
				p += strlen(p);
			}
			if(i+1<n)
				*p++ = ',';
		}
		*p++ = '>';
		if(*s != '_'){
			werrstr("bad template %s", t);
			return 0;
		}
		s++;

		/*
		 * Can't seem to tell difference between a qualifying name
		 * and arguments.  Not sure which is which.  It appears that if
		 * you get a name, use it, otherwise look for types.
		 * The G type qualifier appears to have no effect other than
		 * turning an ambiguous name into a definite type.
		 *
		 *	SetFlag__H1Zb_P15FlagSettingMode_v
		 *	=>	void SetFlag<bool>(FlagSettingMode *)
		 *	SetFlag__H1Zb_15FlagSettingMode_v
		 *	=>	void FlagSettingMode::SetFlag<bool>()
		 *	SetFlag__H1Zb_G15FlagSettingMode_v
		 *	=>	void SetFlag<bool>(FlagSettingMode)
		 */
		if(strchr("ACFGPRSUVX", *s)){
			/* args */
			t = s;
			p1 = p;
			*p++ = '(';
			while(*s != '_'){
				if(*s == 0 || !gccname(&s, &p)){
					werrstr("bad H args: %s", t);
					return 0;
				}
			}
			*p++ = ')';
			s++;
		}else{
			p1 = p;
			/* name */
			if(!gccname(&s, &p))
				return 0;
		}
		/*
		 * Need to do some rearrangement of <> () and names here.
		 * Doesn't matter since we strip out the <> and () anyway.
		 */
		break;

	case 'M':	/* M1S: pointer to member */
		if(*(s+1) != '1' || *(s+2) != 'S')
			goto bad;
		s += 3;
		strcpy(p, "mptr ");
		p += 5;
		if(!gccname(&s, &p))
			return 0;
		break;

	case 'N':	/* multiply-repeated type */
		s++;
		if(!gccnumber(&s, &n, 0) || !gccnumber(&s, &m, 0))
			return 0;
		sprint(p, "T%dx%d", m, n);
		p += strlen(p);
		break;

	case 'P':	/* pointer */
		s++;
		strcpy(p, "ptr ");
		p += 4;
		if(!gccname(&s, &p))
			return 0;
		break;

	case 'Q':	/* qualified name */
		s++;
		if(!gccnumber(&s, &n, 0))
			return 0;
		for(i=0; i<n; i++){
			if(!gccname(&s, &p)){
				werrstr("in hierarchy: %r");
				return 0;
			}
			if(i+1 < n){
				strcpy(p, "::");
				p += 2;
			}
		}
		break;

	case 'R':	/* reference */
		s++;
		strcpy(p, "ref ");
		p += 4;
		if(!gccname(&s, &p))
			return 0;
		break;

	case 't':	/* class template instantiation */
		/* should share code with case 'H' */
		t = s;
		s++;
		if(!gccname(&s, &p))
			return 0;
		if(!gccnumber(&s, &n, 0))
			return 0;
		p0 = p;
		/* template arguments */
		*p++ = '<';
		for(i=0; i<n; i++){
			val = 1;
			if(*s == 'Z'){	/* argument is a type, not value */
				val = 0;
				s++;
			}
			if(!gccname(&s, &p))
				return 0;
			if(val){
				if(!gccnumber(&s, &m, 1))	/* gccnumber: 1 or 0? */
					return 0;
				sprint(p, "=%d", m);
				p += strlen(p);
			}
			if(i+1<n)
				*p++ = ',';
		}
		*p++ = '>';
		break;

	case 'T':	/* once-repeated type */
		s++;
		if(!gccnumber(&s, &n, 0))
			return 0;
		sprint(p, "T%d", n);
		p += strlen(p);
		break;

	case 'X':	/* type parameter in 'H' */
		if(!isdigit((uchar)*(s+1)) || !isdigit((uchar)*(s+2)))
			goto bad;
		memmove(p, s, 3);
		p += 3;
		s += 3;
		break;
	}

	USED(p1);
	USED(p0);

out:
	*ps = s;
	*pp = p;
	return 1;
}
