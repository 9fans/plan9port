#include <u.h>
#include <libc.h>
#include <draw.h>

extern vlong _drawflength(int);

Font*
openfont(Display *d, char *name)
{
	Font *fnt;
	int fd, i, n;
	char *buf, *nambuf;

	nambuf = 0;
	fd = open(name, OREAD);

	if(fd < 0 && strncmp(name, "/lib/font/bit/", 14) == 0){
		nambuf = smprint("#9/font/%s", name+14);
		if(nambuf == nil)
			return 0;
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
