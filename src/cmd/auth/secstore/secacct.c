#include <u.h>
#include <libc.h>
#include <ip.h>

int verbose = 1;
static char testmess[] = "__secstore\tPAK\nC=%s\nm=0\n";

void
main(int argc, char **argv)
{
	int n, m, fd;
	uchar buf[500];

	if(argc != 2)
		exits("usage: secacct userid");

	n = snprint((char*)buf, sizeof buf, testmess, argv[1]);
	hnputs(buf, 0x8000+n-2);

	fd = dial("tcp!ruble.cs.bell-labs.com!5356", 0, 0, 0);
	if(fd < 0)
		exits("cannot dial ruble");
	if(write(fd, buf, n) != n || readn(fd, buf, 2) != 2)
		exits("cannot exchange first round");
	n = ((buf[0]&0x7f)<<8) + buf[1];
	if(n+1 > sizeof buf)
		exits("implausibly large count");
	m = readn(fd, buf, n);
	close(fd);
	if(m != n)
		fprint(2,"short read from secstore\n");
	buf[m] = 0;
	print("%s\n", (char*)buf);
	exits(0);
}
