#include <u.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <libc.h>
#include "term.h"

int myopenpty(int[], char*);

int
getpts(int fd[], char *slave)
{
	return myopenpty(fd, slave);
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
	if(ioctl(rcfd, TIOCSWINSZ, &ws) < 0)
		fprint(2, "ioctl: %r\n");
	ows = ws;
}








/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

int
myopenpty(int fd[], char *name)
{
	char pty[] = "/dev/ptyXX";
	char *s, *t;
	struct group *gr;
	int ttygid;

	if((gr = getgrnam("tty")) != NULL)
		ttygid = gr->gr_gid;
	else
		ttygid = -1;

	for(s="pqrstuvw"; *s; s++)
	for(t="0123456789abcdef"; *t; t++){
		pty[5] = 'p';
		pty[8] = *s;
		pty[9] = *t;
		if((fd[1] = open(pty, O_RDWR)) < 0){
			if(errno == ENOENT)
				return -1;
		}else{
			pty[5] = 't';
			chown(pty, getuid(), ttygid);
			chmod(pty, 620);
			revoke(pty);
			if((fd[0] = open(pty, O_RDWR)) < 0){
				close(fd[1]);
				continue;
			}
			if(name)
				strcpy(name, pty);
			return 0;
		}
	}
	errno = ENOENT;
	return -1;

}

