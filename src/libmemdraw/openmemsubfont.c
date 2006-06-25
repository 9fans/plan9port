#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

Memsubfont*
openmemsubfont(char *name)
{
	Memsubfont *sf;
	Memimage *i;
	Fontchar *fc;
	int fd, n;
	char hdr[3*12+4+1];
	uchar *p;

	fd = open(name, OREAD);
	if(fd < 0)
		return nil;
	p = nil;
	i = readmemimage(fd);
	if(i == nil)
		goto Err;
	if(read(fd, hdr, 3*12) != 3*12){
		werrstr("openmemsubfont: header read error: %r");
		goto Err;
	}
	n = atoi(hdr);
	p = malloc(6*(n+1));
	if(p == nil)
		goto Err;
	if(read(fd, p, 6*(n+1)) != 6*(n+1)){
		werrstr("openmemsubfont: fontchar read error: %r");
		goto Err;
	}
	fc = malloc(sizeof(Fontchar)*(n+1));
	if(fc == nil)
		goto Err;
	_unpackinfo(fc, p, n);
	sf = allocmemsubfont(name, n, atoi(hdr+12), atoi(hdr+24), fc, i);
	if(sf == nil){
		free(fc);
		goto Err;
	}
	free(p);
	return sf;
Err:
	close(fd);
	if (i != nil)
		freememimage(i);
	if (p != nil)
		free(p);
	return nil;
}
