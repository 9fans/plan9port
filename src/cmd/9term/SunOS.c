#include <u.h>
#include <termios.h>
#include <sys/termios.h>
#include <libc.h>
#include "term.h"

int
getpts(int fd[], char *slave)
{
	fd[1] = open("/dev/ptmx", ORDWR);
	if ((grantpt(fd[1]) < 0) || (unlockpt(fd[1]) < 0))
		return -1;
	fchmod(fd[1], 0622);
	strcpy(slave, ptsname(fd[1]));
	fd[0] = open(slave, OREAD);
	return 0;
}

int
childpty(int fd[], char *slave)
{
	int sfd;

	close(fd[1]);
	setsid();
	sfd = open(slave, ORDWR);
	if(sfd < 0)
		sysfatal("open %s: %r\n", slave);
	return sfd;
}

struct winsize ows;

void
updatewinsize(int row, int col, int dx, int dy)
{
	struct winsize ws;

	ws.ws_row = row;
	ws.ws_col = col;
	ws.ws_xpixel = dx;
	ws.ws_ypixel = dy;
	if(ws.ws_row != ows.ws_row || ws.ws_col != ows.ws_col)
	if(ioctl(rcfd, TIOCSWINSZ, &ws) < 0)
		fprint(2, "ioctl: %r\n");
	ows = ws;
}

