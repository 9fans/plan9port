#include <u.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#include <grp.h>
#include <termios.h>
#ifdef HAS_SYS_TERMIOS
#include <sys/termios.h>
#endif
#ifdef __linux__
#include <pty.h>
#endif
#include <fcntl.h>
#include <libc.h>
#include <draw.h>
#include "term.h"

#define debug 0

static char *abc =
	"abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"0123456789";
static char *_123 =
	"0123456789"
	"abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ";

int
getpts(int fd[], char *slave)
{
	char *a, *z;
	char pty[] = "/dev/ptyXX";

	for(a=abc; *a; a++)
	for(z=_123; *z; z++){
		pty[8] = *a;
		pty[9] = *z;
		if((fd[1] = open(pty, ORDWR)) < 0){
			if(errno == ENOENT)
				break;
		}else{
			fchmod(fd[1], 0620);
			strcpy(slave, pty);
			slave[5] = 't';
			if((fd[0] = open(slave, ORDWR)) >= 0)
				return 0;
			close(fd[1]);
		}
	}
	sysfatal("no ptys");
	return 0;
}

int
childpty(int fd[], char *slave)
{
	int sfd;

	close(fd[1]);	/* drop master */
	setsid();
	sfd = open(slave, ORDWR);
	if(sfd < 0)
		sysfatal("child open %s: %r\n", slave);
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

	needdisplay(); // in case this is 'win' and not 9term
	// Leave "is this a hidpi display" in the low bit of the ypixel height for mc.
	dy &= ~1;
	if(display != nil && display->dpi >= DefaultDPI*3/2)
		dy |= 1;
	ws.ws_ypixel = dy;

	if(ws.ws_row != ows.ws_row || ws.ws_col != ows.ws_col){
		if(ioctl(rcfd, TIOCSWINSZ, &ws) < 0)
			fprint(2, "ioctl: %r\n");
	}
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
	return ttmode.c_lflag&ECHO;
}

int
getintr(int fd)
{
	if(tcgetattr(fd, &ttmode) < 0)
		return 0x7F;
	return ttmode.c_cc[VINTR];
}
