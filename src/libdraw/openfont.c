#include <u.h>
#include <libc.h>
#include <draw.h>

extern vlong _drawflength(int);

Font*
openfont(Display *d, char *name)
{
	Font *fnt;
	int fd, i, n;
	char *buf, *nambuf, *root;

	nambuf = 0;
	fd = open(name, OREAD);

	if(fd < 0 && strncmp(name, "/lib/font/bit/", 14) == 0){
		root = getenv("PLAN9");
		if(root == nil)
			return 0;
		nambuf = malloc(strlen(root)+5+strlen(name+13)+1);
		if(nambuf == nil)
			return 0;
		strcpy(nambuf, root);
		strcat(nambuf, "/font");
		strcat(nambuf, name+13);
		if((fd = open(nambuf, OREAD)) < 0){
			free(nambuf);
			return 0;
		}
		name = nambuf;
	}
	if(fd < 0)
		return 0;

	n = _drawflength(fd);
	buf = malloc(n+1);
	if(buf == 0){
		close(fd);
		free(nambuf);
		return 0;
	}
	buf[n] = 0;
	i = read(fd, buf, n);
	close(fd);
	if(i != n){
		free(buf);
		free(nambuf);
		return 0;
	}
	fnt = buildfont(d, buf, name);
	free(buf);
	free(nambuf);
	return fnt;
}
