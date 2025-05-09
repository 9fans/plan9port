#include "rc.h"
#include "exec.h"
#include "fns.h"

/*
 * delete all the GLOB marks from s, in place
 */
char*
deglob(char *s)
{
	char *r = strchr(s, GLOB);
	if(r){
		char *w = r++;
		do{
			if(*r==GLOB)
				r++;
			*w++=*r;
		}while(*r++);
	}
	return s;
}

static int
globcmp(const void *s, const void *t)
{
	return strcmp(*(char**)s, *(char**)t);
}

static void
globsort(word *left, word *right)
{
	char **list;
	word *a;
	int n = 0;
	for(a = left;a!=right;a = a->next) n++;
	list = (char **)emalloc(n*sizeof(char *));
	for(a = left,n = 0;a!=right;a = a->next,n++) list[n] = a->word;
	qsort((void *)list, n, sizeof(void *), globcmp);
	for(a = left,n = 0;a!=right;a = a->next,n++) a->word = list[n];
	free(list);
}

/*
 * Does the string s match the pattern p
 * . and .. are only matched by patterns starting with .
 * * matches any sequence of characters
 * ? matches any single character
 * [...] matches the enclosed list of characters
 */

static int
matchfn(char *s, char *p)
{
	if(s[0]=='.' && (s[1]=='\0' || s[1]=='.' && s[2]=='\0') && p[0]!='.')
		return 0;
	return match(s, p, '/');
}

static void
pappend(char **pdir, char *name)
{
	char *path = makepath(*pdir, name);
	free(*pdir);
	*pdir = path;
}

static word*
globdir(word *list, char *pattern, char *name)
{
	char *slash, *glob, *entry;
	void *dir;

#ifdef Plan9
	/* append slashes, Readdir() already filtered directories */
	while(*pattern=='/'){
		pappend(&name, "/");
		pattern++;
	}
#endif
	if(*pattern=='\0')
		return Newword(name, list);

	/* scan the pattern looking for a component with a metacharacter in it */
	glob=strchr(pattern, GLOB);

	/* If we ran out of pattern, append the name if accessible */
	if(glob==0){
		pappend(&name, pattern);
		if(access(name, 0)==0)
			return Newword(name, list);
		goto out;
	}

	*glob='\0';
	slash=strrchr(pattern, '/');
	if(slash){
		*slash='\0';
		pappend(&name, pattern);
		*slash='/';
		pattern=slash+1;
	}
	*glob=GLOB;

	/* read the directory and recur for any entry that matches */
	dir = Opendir(name[0]?name:".");
	if(dir==0)
		goto out;
	slash=strchr(glob, '/');
	while((entry=Readdir(dir, slash!=0)) != 0){
		if(matchfn(entry, pattern))
			list = globdir(list, slash?slash:"", makepath(name, entry));
	}
	Closedir(dir);
out:
	free(name);
	return list;
}

/*
 * Subsitute a word with its glob in place.
 */
void
globword(word *w)
{
	word *left, *right;

	if(w==0 || strchr(w->word, GLOB)==0)
		return;
	right = w->next;
	left = globdir(right, w->word, estrdup(""));
	if(left == right) {
		deglob(w->word);
	} else {
		free(w->word);
		globsort(left, right);
		w->next = left->next;
		w->word = Freeword(left);
	}
}

/*
 * Return a pointer to the next utf code in the string,
 * not jumping past nuls in broken utf codes!
 */
static char*
nextutf(char *p)
{
	int i, n, c = *p;

	if(onebyte(c))
		return p+1;
	if(twobyte(c))
		n = 2;
	else if(threebyte(c))
		n = 3;
	else
		n = 4;
	for(i = 1; i < n; i++)
		if(!xbyte(p[i]))
			break;
	return p+i;
}

/*
 * Convert the utf code at *p to a unicode value
 */
static int
unicode(char *p)
{
	int c = *p;

	if(onebyte(c))
		return c&0xFF;
	if(twobyte(c)){
		if(xbyte(p[1]))
			return ((c&0x1F)<<6) | (p[1]&0x3F);
	} else if(threebyte(c)){
		if(xbyte(p[1]) && xbyte(p[2]))
			return ((c&0x0F)<<12) | ((p[1]&0x3F)<<6) | (p[2]&0x3F);
	} else if(fourbyte(c)){
		if(xbyte(p[1]) && xbyte(p[2]) && xbyte(p[3]))
			return ((c&0x07)<<18) | ((p[1]&0x3F)<<12) | ((p[2]&0x3F)<<6) | (p[3]&0x3F);
	}
	return -1;
}

/*
 * Do p and q point at equal utf codes
 */
static int
equtf(char *p, char *q)
{
	if(*p!=*q)
 		return 0;
	return unicode(p) == unicode(q);
}

int
match(char *s, char *p, int stop)
{
	int compl, hit, lo, hi, t, c;

	for(; *p!=stop && *p!='\0'; s = nextutf(s), p = nextutf(p)){
		if(*p!=GLOB){
			if(!equtf(p, s)) return 0;
		}
		else switch(*++p){
		case GLOB:
			if(*s!=GLOB)
				return 0;
			break;
		case '*':
			for(;;){
				if(match(s, nextutf(p), stop)) return 1;
				if(!*s)
					break;
				s = nextutf(s);
			}
			return 0;
		case '?':
			if(*s=='\0')
				return 0;
			break;
		case '[':
			if(*s=='\0')
				return 0;
			c = unicode(s);
			p++;
			compl=*p=='~';
			if(compl)
				p++;
			hit = 0;
			while(*p!=']'){
				if(*p=='\0')
					return 0;		/* syntax error */
				lo = unicode(p);
				p = nextutf(p);
				if(*p!='-')
					hi = lo;
				else{
					p++;
					if(*p=='\0')
						return 0;	/* syntax error */
					hi = unicode(p);
					p = nextutf(p);
					if(hi<lo){ t = lo; lo = hi; hi = t; }
				}
				if(lo<=c && c<=hi)
					hit = 1;
			}
			if(compl)
				hit=!hit;
			if(!hit)
				return 0;
			break;
		}
	}
	return *s=='\0';
}
