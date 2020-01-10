#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>

static char *(*demanglers[])(char*, char*) =
{
	demanglegcc2,
	demanglegcc3,
};

char*
demangle(char *s, char *buf, int strip)
{
	char *t;
	char *r, *w;
	int i, nangle, nparen;

	t = nil;
	for(i=0; i<nelem(demanglers); i++){
		t = demanglers[i](s, buf);
		if(t != s)
			break;
	}
	if(t == s || !strip)
		return t;

	/* copy name without <> and () - not right, but convenient */
	/* convert :: to $ - not right, but convenient (should fix acid) */
	nangle = 0;
	nparen = 0;
	for(r=w=buf; *r; r++){
		switch(*r){
		case '<':
			nangle++;
			break;
		case '>':
			nangle--;
			break;
		case '(':
			nparen++;
			break;
		case ')':
			nparen--;
			break;
		default:
			if(nparen == 0 && nangle == 0)
				*w++ = *r;
			break;
		}
	}
	*w = 0;
	return buf;
}

#ifdef TEST
void
main(int argc, char **argv)
{
	int i;

	for(i=1; i<argc; i++){
		print("%s\n", demangle(argv[i], 0));
		print("\t%s\n", demangle(argv[i], 1));
	}
	exits(nil);
}
#endif
