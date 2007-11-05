#include <u.h>
#include <libc.h>
#include <bio.h>
#include "diff.h"

static int
itemcmp(const void *v1, const void *v2)
{
	char *const*d1 = v1, *const*d2 = v2;

	return strcmp(*d1, *d2);
}

static char **
scandir(char *name)
{
	char **cp;
	Dir *db;
	int nitems;
	int fd, n;

	if ((fd = open(name, OREAD)) < 0){
		panic(mflag ? 0 : 2, "can't open %s\n", name);
		return nil;
	}
	cp = 0;
	nitems = 0;
	if((n = dirreadall(fd, &db)) > 0){
		while (n--) {
			cp = REALLOC(cp, char *, (nitems+1));
			cp[nitems] = MALLOC(char, strlen((db+n)->name)+1);
			strcpy(cp[nitems], (db+n)->name);
			nitems++;
		}
		free(db);
	}
	cp = REALLOC(cp, char*, (nitems+1));
	cp[nitems] = 0;
	close(fd);
	qsort((char *)cp, nitems, sizeof(char*), itemcmp);
	return cp;
}

static int
isdotordotdot(char *p)
{
	if (*p == '.') {
		if (!p[1])
			return 1;
		if (p[1] == '.' && !p[2])
			return 1;
	}
	return 0;
}

void
diffdir(char *f, char *t, int level)
{
	char  **df, **dt, **dirf, **dirt;
	char *from, *to;
	int res;
	char fb[MAXPATHLEN+1], tb[MAXPATHLEN+1];

	df = scandir(f);
	dt = scandir(t);
	dirf = df;
	dirt = dt;
	if(df == nil || dt == nil)
		goto Out;
	while (*df || *dt) {
		from = *df;
		to = *dt;
		if (from && isdotordotdot(from)) {
			df++;
			continue;
		}
		if (to && isdotordotdot(to)) {
			dt++;
			continue;
		}
		if (!from)
			res = 1;
		else if (!to)
			res = -1;
		else
			res = strcmp(from, to);
		if (res < 0) {
			if (mode == 0 || mode == 'n')
				Bprint(&stdout, "Only in %s: %s\n", f, from);
			df++;
			continue;
		}
		if (res > 0) {
			if (mode == 0 || mode == 'n')
				Bprint(&stdout, "Only in %s: %s\n", t, to);
			dt++;
			continue;
		}
		if (mkpathname(fb, f, from))
			continue;
		if (mkpathname(tb, t, to))
			continue;
		diff(fb, tb, level+1);
		df++; dt++;
	}
Out:
	for (df = dirf; df && *df; df++)
		FREE(*df);
	for (dt = dirt; dt && *dt; dt++)
		FREE(*dt);
	FREE(dirf);
	FREE(dirt);
}
