#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define nil ((void*)0)

char*
mkfname(char *tmpdir, char *prefix)
{
	int n;
	char *p, *fname;

	if((p = getenv("TMPDIR")) != nil)
		goto Mktemp;
	if((p = tmpdir) != nil)
		goto Mktemp;
	p = "/tmp";

 Mktemp:
	n = strlen(p)+1+strlen(prefix)+1+8+1;
	if((fname = malloc(n)) == nil)
		return nil;
	memset(fname, 0, n);
	strcat(fname, p);
	if((n = strlen(p)) > 0 && p[n-1] != '/')
		strcat(fname, "/");
	strcat(fname, prefix);
	strcat(fname, ".XXXXXXXX");

	return fname;
}

extern	int	mkstemp(char*);

char*
safe_tempnam(char *tmpdir, char *prefix)
{
	int fd;
	char *fname;

	if((fname = mkfname(tmpdir, prefix)) == nil)
		return nil;

	if((fd = mkstemp(fname)) < 0){		/* XXX: leak fd, fname */
		free(fname);
		return nil;
	}
	return fname;
}

int
safe_tmpnam(char *fname)
{
	char *p;

	if((p = mkfname(nil, "tmpfile")) == nil)
		return -1;
	strcpy(fname, p);
	free(p);
	return mkstemp(fname);
}
