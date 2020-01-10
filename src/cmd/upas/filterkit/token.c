#include <u.h>
#include <libc.h>
#include <libsec.h>
#include <libString.h>
#include "dat.h"

void
usage(void)
{
	fprint(2, "usage: %s key [token]\n", argv0);
	exits("usage");
}

static String*
mktoken(char *key, long thetime)
{
	char *now;
	uchar digest[SHA1dlen];
	char token[64];
	String *s;

	now = ctime(thetime);
	memset(now+11, ':', 8);
	hmac_sha1((uchar*)now, strlen(now), (uchar*)key, strlen(key), digest, nil);
	enc64(token, sizeof token, digest, sizeof digest);
	s = s_new();
	s_nappend(s, token, 5);
	return s;
}

static char*
check_token(char *key, char *file)
{
	String *s;
	long now;
	int i;
	char buf[1024];
	int fd;

	fd = open(file, OREAD);
	if(fd < 0)
		return "no match";
	i = read(fd, buf, sizeof(buf)-1);
	close(fd);
	if(i < 0)
		return "no match";
	buf[i] = 0;

	now = time(0);

	for(i = 0; i < 14; i++){
		s = mktoken(key, now-24*60*60*i);
		if(strstr(buf, s_to_c(s)) != nil){
			s_free(s);
			return nil;
		}
		s_free(s);
	}
	return "no match";
}

static char*
create_token(char *key)
{
	String *s;

	s = mktoken(key, time(0));
	print("%s", s_to_c(s));
	return nil;
}

void
main(int argc, char **argv)
{
	ARGBEGIN {
	} ARGEND;

	switch(argc){
	case 2:
		exits(check_token(argv[0], argv[1]));
		break;
	case 1:
		exits(create_token(argv[0]));
		break;
	default:
		usage();
	}
	exits(0);
}
