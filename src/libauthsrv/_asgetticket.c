#include <u.h>
#include <libc.h>
#include <authsrv.h>

static char *pbmsg = "AS protocol botch";

int
_asgetticket(int fd, char *trbuf, char *tbuf)
{
	if(write(fd, trbuf, TICKREQLEN) < 0){
		close(fd);
		werrstr(pbmsg);
		return -1;
	}
	return _asrdresp(fd, tbuf, 2*TICKETLEN);
}
