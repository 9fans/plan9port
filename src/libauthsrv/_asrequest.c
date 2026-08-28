#include <u.h>
#include <libc.h>
#include <authsrv.h>

int
_asrequest(int fd, Ticketreq *tr)
{
	char trbuf[TICKREQLEN];
	int n;

	n = convTR2M(tr, trbuf);
	if(write(fd, trbuf, n) != n)
		return -1;

	return 0;
}
