#include <u.h>
#include <termios.h>
#include <stropts.h>
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

	fd[0] = open(slave, ORDWR);
	if(fd[0] < 0)
		sysfatal("open %s: %r\n", slave);

	/* set up the right streams modules for a tty */
	ioctl(fd[0], I_PUSH, "ptem");        /* push ptem */
	ioctl(fd[0], I_PUSH, "ldterm");      /* push ldterm */

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
		fprint(2, "ioctl TIOCSWINSZ: %r\n");
	ows = ws;
}

/*
 * israw has been inspired by Matty Farrow's 9term.
 * The code below is probably a gross simplification --
 * for the few cases tested it seems to be enough.
 * However, for example, Matty's code also looks at ISIG,
 * whereas, we do not (yet?). Axel.
 *
 *Note: I guess only the get/set terminal mode attribute
 * code needs to be here; the logic around it could be
 * elswhere (9term.c) - but if the code below is split,
 * the question is what a nice interface would be. Axel.
 */

static struct termios ttmode;

int
israw(int fd)
{
	int e, c, i;

	tcgetattr(fd, &ttmode);
	c = (ttmode.c_lflag & ICANON) ? 1 : 0;
	e = (ttmode.c_lflag & ECHO) ? 1 : 0;
	i = (ttmode.c_lflag & ISIG) ? 1 : 0;

	if(0) fprint(2, "israw: icanon=%d echo=%d isig=%d\n", c, e, i);

	return !c || !e ;
}


int
setecho(int fd, int on)
{
	int e, c, i;
	int oldecho;

	tcgetattr(fd, &ttmode);
	c = (ttmode.c_lflag & ICANON) ? 1 : 0;
	e = (ttmode.c_lflag & ECHO) ? 1 : 0;
	i = (ttmode.c_lflag & ISIG) ? 1 : 0;

	if(0) fprint(2, "setecho(%d) pre: icanon=%d echo=%d isig=%d\n", on, c, e, i);

	oldecho = e;

	if (oldecho == on)
		return  oldecho;

	if (on) {
		ttmode.c_lflag |= ECHO;
		tcsetattr(fd, TCSANOW, &ttmode);
	} else {
		ttmode.c_lflag &= ~ECHO;
		tcsetattr(fd, TCSANOW, &ttmode);
	}

	if (0){
		tcgetattr(fd, &ttmode);
		c = (ttmode.c_lflag & ICANON) ? 1 : 0;
		e =  (ttmode.c_lflag & ECHO) ? 1 : 0;
		i = (ttmode.c_lflag & ISIG) ? 1 : 0;

		fprint(2, "setecho(%d) post: icanon=%d echo=%d isig=%d\n", on, c, e, i);
	}

	return oldecho;
}

