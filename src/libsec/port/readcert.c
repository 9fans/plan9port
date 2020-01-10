#include <u.h>
#include <libc.h>
#include <auth.h>
#include <mp.h>
#include <libsec.h>

static char*
readfile(char *name)
{
	int fd;
	char *s;
	Dir *d;

	fd = open(name, OREAD);
	if(fd < 0)
		return nil;
	if((d = dirfstat(fd)) == nil)
		return nil;
	s = malloc(d->length + 1);
	if(s == nil || readn(fd, s, d->length) != d->length){
		free(s);
		free(d);
		close(fd);
		return nil;
	}
	close(fd);
	s[d->length] = '\0';
	free(d);
	return s;
}

uchar*
readcert(char *filename, int *pcertlen)
{
	char *pem;
	uchar *binary;

	pem = readfile(filename);
	if(pem == nil){
		werrstr("can't read %s", filename);
		return nil;
	}
	binary = decodepem(pem, "CERTIFICATE", pcertlen, nil);
	free(pem);
	if(binary == nil){
		werrstr("can't parse %s", filename);
		return nil;
	}
	return binary;
}


PEMChain *
readcertchain(char *filename)
{
	char *chfile;
	PEMChain *chp;

	chfile = readfile(filename);
	if (chfile == nil) {
		werrstr("can't read %s", filename);
		return nil;
	}
	chp = decodepemchain(chfile, "CERTIFICATE");
	return chp;
}
