/*
 * gcc2 name demangler.
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

typedef struct Gccstate Gccstate;
struct Gccstate
{
	char *name[128];
	int nname;
};
static int gccname(char**, char**, Gccstate*);
char*
demanglegcc2(char *s, char *buf)
{
	char *p, *os, *name, *t;
	int namelen;
	Gccstate state;
	
	state.nname = 0;
	os = s;
	p = buf;

	if(memcmp(os, "_._", 3) == 0){
		name = "destructor";
		namelen = strlen(name);
		s = os+3;
	}else{
		/* the mangled part begins with the final __ */
		if((s = strstr(os, "__")) == nil)
			return os;
		do{
			t = s;
			if(strchr("123456789FHQt", s[2]))
				break;
		}while((s = strstr(t+1, "__")) != nil);
		
		s = t;
		name = os;
		namelen = t - os;
		if(namelen == 0){
			name = "constructor";
			namelen = strlen(name);
		}
		s += 2;
	}

	switch(*s){
	default:
		return os;

	case 'F':	/* plain function */
		s++;
		break;
	
	case 'Q':
	case 'H':
	case 't':
	case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		if(!gccname(&s, &p, &state)){
			if(debug) fprint(2, "bad name: %s\n", os);
			return os;
		}
		strcpy(p, "::");
		p += 2;
		break;
	}

	memmove(p, name, namelen);
	p += namelen;

	if(*s && *s != '_'){
		/* the rest of the name is the argument types */
		*p++ = '(';
		while(*s != 0 && *s != '_' && gccname(&s, &p, &state))
			*p++ = ',';
		if(*(p-1) == ',')
			p--;
		*p++ = ')';
	}
	
	if(*s == '_'){
		/* the remainder is the type of the return value */
	}

	*p = 0;
	return buf;
}

static Chartab typetab[] =
{
	'b',	"bool",
	'c',	"char",
	'd',	"double",
	'i',	"int",
	'l',	"long",
	'v',	"void",
	0, 0
};

static int
gccnumber(char **ps, int *pn)
{
	char *s;
	int n;
	
	s = *ps;
	if(!isdigit((uchar)*s))
		return 0;
	n = strtol(s, &s, 10);
	if(*s == '_')
		s++;
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
gccname(char **ps, char **pp, Gccstate *state)
{
	int i, n, m, val;
	char c, *os, *s, *t, *p;
	
	s = *ps;
	os = s;
	p = *pp;

/*	print("\tgccname: %s\n", s); */

#if 0
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
#endif
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
		if(debug) fprint(2, "gccname: %s (%s)\n", os, s);
		return 0;

	case '1': case '2': case '3': case '4':	/* name length */
	case '5': case '6': case '7': case '8': case '9':
		n = strtol(s, &s, 10);
		memmove(p, s, n);
		p += n;
		s += n;
		break;

	case 'C':	/* const */
		s++;
		strcpy(p, "const ");
		p += strlen(p);
		if(!gccname(&s, &p, state))
			return 0;
		break;

	case 'U':	/* unsigned */
		s++;
		strcpy(p, "unsigned ");
		p += strlen(p);
		if(!gccname(&s, &p, state))
			return 0;
		break;

#if 0
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
#endif

	case 'N':	/* repeated name/type */
	case 'X':
		c = *s++;
		if(!isdigit((uchar)*s) || !isdigit((uchar)*(s+1)))
			goto bad;
		n = *s++ - '0';
		m = *s++ - '0';
		sprint(p, "%c%d/%d", c, n, m);
		p += strlen(p);
		break;

	case 'Q':	/* hierarchical name */
		s++;
		if(!isdigit((uchar)*s))
			goto bad;
		n = *s++ - '0';
		for(i=0; i<n; i++){
			if(!gccname(&s, &p, state)){
				if(debug) fprint(2, "bad name in hierarchy: %s in %s\n", s, os);
				return 0;
			}
			if(i+1 < n){
				strcpy(p, "::");
				p += 2;
			}
		}
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
		goto bad;

	case 't':	/* named template */
		c = *s++;
		if(!gccname(&s, &p, state))
			return 0;
		goto template;
	case 'H':	/* nameless template */
		c = *s++;
	template:
		if(!gccnumber(&s, &n))
			goto bad;
		*p++ = '<';
		for(i=0; i<n; i++){
			val = 1;
			if(*s == 'Z'){
				val = 0;
				s++;
			}
			if(!gccname(&s, &p, state))
				goto bad;
			if(val){
				if(!gccnumber(&s, &m))
					goto bad;
				sprint(p, "=%d", m);
				p += strlen(p);
			}
			if(i+1 < n)
				*p++ = ',';
		}
		*p++ = '>';
		if(c == 'H'){
			if(*s != '_')
				goto bad;
			s++;
		}
		break;

	case 'T':	/* previously-seen type??? e.g., T2 */
		t = s;
		for(s++; isdigit((uchar)*s); s++)
			;
		memmove(p, t, s-t);
		p += s-t;
		break;		
	}

suffix:	
	*ps = s;
	*pp = p;
	return 1;
}

