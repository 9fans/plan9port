#include <u.h>
#include "9term.h"
#include <sys/types.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/termios.h>
#include <util.h>
#include <libc.h>

int
getpts(int fd[], char *slave)
{
	return openpty(&fd[1], &fd[0], slave, 0, 0);
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
	if(ioctl(sfd, TIOCSCTTY, 0) < 0)
		fprint(2, "ioctl TIOCSCTTY: %r\n");
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
	if(ioctl(rcfd[0], TIOCSWINSZ, &ws) < 0)
		fprint(2, "ioctl: %r\n");
	ows = ws;
}
