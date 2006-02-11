#include "dat.h"
#include <thread.h>
#include "common.h"

void
usage(void)
{
	fprint(2, "usage: %s recipient fromaddr-file mbox\n", argv0);
	threadexitsall("usage");
}

void
main(int argc, char **argv)
{
	int fd;
	char now[30];
	Addr *a;
	char *deliveredto;
	Mlock *l;
	int bytes;

	ARGBEGIN{
	}ARGEND;

	if(argc != 3)
		usage();

	deliveredto = strrchr(argv[0], '!');
	if(deliveredto == nil)
		deliveredto = argv[0];
	else
		deliveredto++;
	a = readaddrs(argv[1], nil);
	if(a == nil)
		sysfatal("missing from address");

	l = syslock(argv[2]);

	/* append to mbox */
	fd = open(argv[2], OWRITE);
	if(fd < 0)
		sysfatal("opening mailbox: %r");
	seek(fd, 0, 2);
	strncpy(now, ctime(time(0)), sizeof(now));
	now[28] = 0;
	if(fprint(fd, "From %s %s\n", a->val, now) < 0)
		sysfatal("writing mailbox: %r");

	/* copy message handles escapes and any needed new lines */
	bytes = appendfiletombox(0, fd);
	if(bytes < 0)
		sysfatal("writing mailbox: %r");

	close(fd);
	sysunlock(l);

	/* log it */
	syslog(0, "mail", "delivered %s From %s %s (%s) %d", deliveredto,
		a->val, now, argv[0], bytes);
	exits(0);
}
