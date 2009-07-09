#include <u.h>
#include <libc.h>
#include <draw.h>

extern vlong _drawflength(int);
int _fontpipe(char*);

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
		nambuf = unsharp(nambuf);
		if(nambuf == nil)
			return 0;
		if((fd = open(nambuf, OREAD)) < 0){
			free(nambuf);
			return 0;
		}
		name = nambuf;
	}
	if(fd >= 0)
		n = _drawflength(fd);
	if(fd < 0 && strncmp(name, "/mnt/font/", 10) == 0) {
		fd = _fontpipe(name+10);
		n = 8192;
	}
	if(fd < 0)
		return 0;

	buf = malloc(n+1);
	if(buf == 0){
		close(fd);
		free(nambuf);
		return 0;
	}
	i = readn(fd, buf, n);
	close(fd);
	if(i <= 0){
		free(buf);
		free(nambuf);
		return 0;
	}
	buf[i] = 0;
	fnt = buildfont(d, buf, name);
	free(buf);
	free(nambuf);
	return fnt;
}

int
_fontpipe(char *name)
{
	int p[2];
	char c;
	char buf[1024], *argv[10];
	int nbuf, pid;
	
	if(pipe(p) < 0)
		return -1;
	pid = rfork(RFNOWAIT|RFFDG|RFPROC);
	if(pid < 0) {
		close(p[0]);
		close(p[1]);
		return -1;
	}
	if(pid == 0) {
		close(p[0]);
		dup(p[1], 1);
		dup(p[1], 2);
		if(p[1] > 2)
			close(p[1]);
		argv[0] = "fontsrv";
		argv[1] = "-pp";
		argv[2] = name;
		argv[3] = nil;
		execvp("fontsrv", argv);
		print("exec fontsrv: %r\n");
		_exit(0);
	}
	close(p[1]);
	
	// success marked with leading \001.
	// otherwise an error happened.
	for(nbuf=0; nbuf<sizeof buf-1; nbuf++) {
		if(read(p[0], &c, 1) < 1 || c == '\n') {
			buf[nbuf] = '\0';
			werrstr(buf);
			close(p[0]);
			return -1;
		}
		if(c == '\001')
			break;
	}
	return p[0];
}
