#include <u.h>
#include <libc.h>
#include <draw.h>

/*
 * Default version: convert to file name
 */

char*
subfontname(char *cfname, char *fname, int maxdepth)
{
	char *t, *u, tmp1[64], tmp2[64];
	int i;

	if(strcmp(cfname, "*default*") == 0)
		return strdup(cfname);
	t = cfname;
	if(t[0] != '/'){
		snprint(tmp2, sizeof tmp2, "%s", fname);
		u = utfrrune(tmp2, '/');
		if(u)
			u[0] = 0;
		else
			strcpy(tmp2, ".");
		snprint(tmp1, sizeof tmp1, "%s/%s", tmp2, t);
		t = tmp1;
	}

	if(maxdepth > 8)
		maxdepth = 8;

	for(i=log2[maxdepth]; i>=0; i--){
		/* try i-bit grey */
		snprint(tmp2, sizeof tmp2, "%s.%d", t, i);
		if(access(tmp2, AREAD) == 0)
			return strdup(tmp2);
	}

	/* try default */
	if(access(t, AREAD) == 0)
		return strdup(t);

	return nil;
}
