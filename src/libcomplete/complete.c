#include <u.h>
#include <libc.h>
#include "complete.h"

static int
longestprefixlength(char *a, char *b, int n)
{
	int i, w;
	Rune ra, rb;

	for(i=0; i<n; i+=w){
		w = chartorune(&ra, a);
		chartorune(&rb, b);
		if(ra != rb)
			break;
		a += w;
		b += w;
	}
	return i;
}

void
freecompletion(Completion *c)
{
	if(c){
		free(c->filename);
		free(c);
	}
}

static int
strpcmp(const void *va, const void *vb)
{
	char *a, *b;

	a = *(char**)va;
	b = *(char**)vb;
	return strcmp(a, b);
}

Completion*
complete(char *dir, char *s)
{
	long i, l, n, nfile, len, nbytes;
	int fd, minlen;
	Dir *dirp;
	char **name, *p;
	ulong* mode;
	Completion *c;

	if(strchr(s, '/') != nil){
		werrstr("slash character in name argument to complete()");
		return nil;
	}

	fd = open(dir, OREAD);
	if(fd < 0)
		return nil;

	n = dirreadall(fd, &dirp);
	if(n <= 0){
		close(fd);
		return nil;
	}

	/* find longest string, for allocation */
	len = 0;
	for(i=0; i<n; i++){
		l = strlen(dirp[i].name) + 1 + 1; /* +1 for /   +1 for \0 */
		if(l > len)
			len = l;
	}

	name = malloc(n*sizeof(char*));
	mode = malloc(n*sizeof(ulong));
	c = malloc(sizeof(Completion) + len);
	if(name == nil || mode == nil || c == nil)
		goto Return;
	memset(c, 0, sizeof(Completion));

	/* find the matches */
	len = strlen(s);
	nfile = 0;
	minlen = 1000000;
	for(i=0; i<n; i++)
		if(strncmp(s, dirp[i].name, len) == 0){
			name[nfile] = dirp[i].name;
			mode[nfile] = dirp[i].mode;
			if(minlen > strlen(dirp[i].name))
				minlen = strlen(dirp[i].name);
			nfile++;
		}

	if(nfile > 0) {
		/* report interesting results */
		/* trim length back to longest common initial string */
		for(i=1; i<nfile; i++)
			minlen = longestprefixlength(name[0], name[i], minlen);

		/* build the answer */
		c->complete = (nfile == 1);
		c->advance = c->complete || (minlen > len);
		c->string = (char*)(c+1);
		memmove(c->string, name[0]+len, minlen-len);
		if(c->complete)
			c->string[minlen++ - len] = (mode[0]&DMDIR)? '/' : ' ';
		c->string[minlen - len] = '\0';
		c->nmatch = nfile;
	} else {
		/* no match, so return all possible strings */
		for(i=0; i<n; i++){
			name[i] = dirp[i].name;
			mode[i] = dirp[i].mode;
		}
		nfile = n;
		c->nmatch = 0;
	}

	/* attach list of names */
	nbytes = nfile * sizeof(char*);
	for(i=0; i<nfile; i++)
		nbytes += strlen(name[i]) + 1 + 1;
	c->filename = malloc(nbytes);
	if(c->filename == nil)
		goto Return;
	p = (char*)(c->filename + nfile);
	for(i=0; i<nfile; i++){
		c->filename[i] = p;
		strcpy(p, name[i]);
		p += strlen(p);
		if(mode[i] & DMDIR)
			*p++ = '/';
		*p++ = '\0';
	}
	c->nfile = nfile;
	qsort(c->filename, c->nfile, sizeof(c->filename[0]), strpcmp);

  Return:
	free(name);
	free(mode);
	free(dirp);
	close(fd);
	return c;
}
