#define _ALL_SOURCE
#include <termios.h>
#include <stropts.h>
#include <fcntl.h>
#include "term.h"

#define debug 0

int
getpts(int fd[], char *slave)
{
	fd[1] = open("/dev/ptc", O_RDWR | O_NOCTTY);
	if (fd[1] < 0){
		sysfatal("no ptys");
		return 0;
	}

	slave = ttyname(fd[1]);

	if (!slave) {
		sysfatal("no ptys");
		return 0;
	}

	fd[0] = open(slave, O_RDWR | O_NOCTTY);
	if (fd[0] < 0) {
		sysfatal("no ptys");
		return 0;
	}

	return 0;
}

int
childpty(int fd[], char *slave)
{
	int sfd;

	close(fd[1]);
	setsid();
	sfd = open(slave, O_RDWR | O_NOCTTY);
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
	if(ws.ws_row != ows.ws_row || ws.ws_col != ows.wscol)
	if(ioctl(rcfd, TIOCSWINSZ, &ws) < 0)
		fprint(2, "ioctl TIOCSWINSZ: %r\n");
	ows = ws;
}

static struct termios ttmode;

int
isecho(int fd)
{
	if(tcgetattr(fd, &ttmode) < 0)
		fprint(2, "tcgetattr: %r\n");
	if(debug) fprint(2, "israw %c%c\n",
		ttmode.c_lflag&ICANON ? 'c' : '-',
		ttmode.c_lflag&ECHO ? 'e' : '-');
	return (ttmode.c_lflag&ICANON && ttmode.c_lflag&ECHO);
}

int
getintr(int fd)
{
	if(tcgetattr(fd, &ttmode) < 0)
		return 0x7F;
	return ttmode.c_cc[VINTR];
}

