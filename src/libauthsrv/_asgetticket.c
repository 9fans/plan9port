#include <u.h>
#include <libc.h>
#include <authsrv.h>

extern int form1check(char*, int);

static int
ticketwirelen(char *buf, int n)
{
	if(n >= 8 && form1check(buf, 8) >= 0)
		return MAXTICKETLEN;
	return TICKETLEN;
}

int
_asgetticket(int fd, Ticketreq *tr, char *tbuf, int tbuflen)
{
	char err[ERRMAX];
	int i, m, n, r;

	strcpy(err, "AS protocol botch");
	errstr(err, ERRMAX);

	if(_asrequest(fd, tr) < 0)
		return -1;
	if(_asrdresp(fd, tbuf, 0) < 0)
		return -1;

	r = 0;
	for(i = 0; i < 2; i++){
		if(tbuflen-r < TICKETLEN)
			return -1;
		if(readn(fd, tbuf, 8) != 8)
			return -1;
		n = ticketwirelen(tbuf, 8);
		if(n > tbuflen-r)
			return -1;
		m = n-8;
		if(readn(fd, tbuf+8, m) != m)
			return -1;
		r += n;
		tbuf += n;
	}

	errstr(err, ERRMAX);
	return r;
}
