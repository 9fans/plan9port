#include <u.h>
#include <libc.h>
#include <draw.h>

Font*
openfont(Display *d, char *name)
{
	Font *fnt;
	int fd, i, n;
	char *buf;

	fd = open(name, OREAD);
	if(fd < 0)
		return 0;

	n = flength(fd);
	buf = malloc(n+1);
	if(buf == 0){
		close(fd);
		return 0;
	}
	buf[n] = 0;
	i = read(fd, buf, n);
	close(fd);
	if(i != n){
		free(buf);
		return 0;
	}
	fnt = buildfont(d, buf, name);
	free(buf);
	return fnt;
}
