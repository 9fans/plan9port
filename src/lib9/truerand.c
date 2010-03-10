#include <u.h>
#include <libc.h>

ulong
truerand(void)
{
	int i, n;
	uchar buf[sizeof(ulong)];
	ulong x;
	static int randfd = -1;
	static char *randfile;

	if(randfd < 0){
		randfd = open(randfile="/dev/random", OREAD);
		/* OpenBSD lets you open /dev/random but not read it! */
		if(randfd < 0 || read(randfd, buf, 1) != 1)
			randfd = open(randfile="/dev/srandom", OREAD);	/* OpenBSD */
		if(randfd < 0)
			sysfatal("can't open %s: %r", randfile);
		fcntl(randfd, F_SETFD, FD_CLOEXEC);
	}
	for(i=0; i<sizeof(buf); i += n)
		if((n = readn(randfd, buf+i, sizeof(buf)-i)) < 0)
			sysfatal("can't read %s: %r", randfile);
	memmove(&x, buf, sizeof x);
	return x;
}
