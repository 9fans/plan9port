#include "stdinc.h"
#include "vac.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

// Convert globbish pattern to regular expression
// The wildcards are
//
//	*	any non-slash characters
//	...	any characters including /
//	?	any single character except /
//	[a-z]	character class
//	[~a-z]	negated character class
//

Reprog*
glob2regexp(char *glob)
{
	char *s, *p, *w;
	Reprog *re;
	int boe;	// beginning of path element

	s = malloc(20*(strlen(glob)+1));
	if(s == nil)
		return nil;
	w = s;
	boe = 1;
	*w++ = '^';
	*w++ = '(';
	for(p=glob; *p; p++){
		if(p[0] == '.' && p[1] == '.' && p[2] == '.'){
			strcpy(w, ".*");
			w += strlen(w);
			p += 3-1;
			boe = 0;
			continue;
		}
		if(p[0] == '*'){
			if(boe)
				strcpy(w, "([^./][^/]*)?");
			else
				strcpy(w, "[^/]*");
			w += strlen(w);
			boe = 0;
			continue;
		}
		if(p[0] == '?'){
			if(boe)
				strcpy(w, "[^./]");
			else
				strcpy(w, "[^/]");
			w += strlen(w);
			boe = 0;
			continue;
		}
		if(p[0] == '['){
			*w++ = '[';
			if(*++p == '~'){
				*w++ = '^';
				p++;
			}
			while(*p != ']'){
				if(*p == '/')
					goto syntax;
				if(*p == '^' || *p == '\\')
					*w++ = '\\';
				*w++ = *p++;
			}
			*w++ = ']';
			boe = 0;
			continue;
		}
		if(strchr("()|^$[]*?+\\.", *p)){
			*w++ = '\\';
			*w++ = *p;
			boe = 0;
			continue;
		}
		if(*p == '/'){
			*w++ = '/';
			boe = 1;
			continue;
		}
		*w++ = *p;
		boe = 0;
		continue;
	}
	*w++ = ')';
	*w++ = '$';
	*w = 0;

	re = regcomp(s);
	if(re == nil){
	syntax:
		free(s);
		werrstr("glob syntax error");
		return nil;
	}
	free(s);
	return re;
}

typedef struct Pattern Pattern;
struct Pattern
{
	Reprog *re;
	int include;
};

Pattern *pattern;
int npattern;

void
loadexcludefile(char *file)
{
	Biobuf *b;
	char *p, *q;
	int n, inc;
	Reprog *re;

	if((b = Bopen(file, OREAD)) == nil)
		sysfatal("open %s: %r", file);
	for(n=1; (p=Brdstr(b, '\n', 1)) != nil; free(p), n++){
		q = p+strlen(p);
		while(q > p && isspace((uchar)*(q-1)))
			*--q = 0;
		switch(p[0]){
		case '\0':
		case '#':
			continue;
		}

		inc = 0;
		if(strncmp(p, "include ", 8) == 0){
			inc = 1;
		}else if(strncmp(p, "exclude ", 8) == 0){
			inc = 0;
		}else
			sysfatal("%s:%d: line does not begin with include or exclude", file, n);

		if(strchr(p+8, ' '))
			fprint(2, "%s:%d: warning: space in pattern\n", file, n);

		if((re = glob2regexp(p+8)) == nil)
			sysfatal("%s:%d: bad glob pattern", file, n);

		pattern = vtrealloc(pattern, (npattern+1)*sizeof pattern[0]);
		pattern[npattern].re = re;
		pattern[npattern].include = inc;
		npattern++;
	}
	Bterm(b);
}

void
excludepattern(char *p)
{
	Reprog *re;

	if((re = glob2regexp(p)) == nil)
		sysfatal("bad glob pattern %s", p);

	pattern = vtrealloc(pattern, (npattern+1)*sizeof pattern[0]);
	pattern[npattern].re = re;
	pattern[npattern].include = 0;
	npattern++;
}

int
includefile(char *file)
{
	Pattern *p, *ep;

	for(p=pattern, ep=p+npattern; p<ep; p++)
		if(regexec(p->re, file, nil, 0))
			return p->include;
	return 1;
}
