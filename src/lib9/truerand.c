#include <u.h>
#include <libc.h>

ulong
truerand(void)
{
	int i, n;
	uchar buf[sizeof(ulong)];
	static int randfd = -1;

	if(randfd < 0){
		randfd = open("/dev/random", OREAD);
		fcntl(randfd, F_SETFD, FD_CLOEXEC);
	}
	if(randfd < 0)
		sysfatal("can't open /dev/random: %r");
	for(i=0; i<sizeof(buf); i += n)
		if((n = readn(randfd, buf+i, sizeof(buf)-i)) < 0)
			sysfatal("can't read /dev/random: %r");
	return *((ulong*)buf);
}
