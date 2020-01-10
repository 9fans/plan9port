/*
 * Multiplexor for sftp sessions.
 * Assumes can parse session with sftp> prompts.
 * Assumes clients are well-behaved and don't hang up the system.
 *
 * Stupid sftp bug: sftp invokes ssh, which always set O_NONBLOCK
 * on 0, 1, and 2.  Ssh inherits sftp's 2, so we can't use the output pipe
 * on fd 2, since it will get set O_NONBLOCK, sftp won't notice, and
 * writes will be lost.  So instead we use a separate pipe for errors
 * and consult it after each command.  Assume the pipe buffer is
 * big enough to hold the error output.
 */
#include <u.h>
#include <fcntl.h>
#include <libc.h>
#include <bio.h>

#undef pipe

int debug;
#define dprint if(debug)print
int sftpfd;
int sftperr;
Biobuf bin;

void
usage(void)
{
	fprint(2, "usage: sftpcache system\n");
	exits("usage");
}

char*
Brd(Biobuf *bin)
{
	static char buf[1000];
	int c, tot;

	tot = 0;
	while((c = Bgetc(bin)) >= 0 && tot<sizeof buf){
		buf[tot++] = c;
		if(c == '\n'){
			buf[tot] = 0;
			dprint("OUT %s", buf);
			return buf;
		}
		if(c == ' ' && tot == 6 && memcmp(buf, "sftp> ", 5) == 0){
			buf[tot] = 0;
			dprint("OUT %s\n", buf);
			return buf;
		}
	}
	if(tot == sizeof buf)
		sysfatal("response too long");
	return nil;
}

int
readstr(int fd, char *a, int n)
{
	int i;

	for(i=0; i<n; i++){
		if(read(fd, a+i, 1) != 1)
			return -1;
		if(a[i] == '\n'){
			a[i] = 0;
			return i;
		}
	}
	return n;
}

void
doerrors(int fd)
{
	char buf[100];
	int n, first;

	first = 1;
	while((n = read(sftperr, buf, sizeof buf)) > 0){
		if(debug){
			if(first){
				first = 0;
				fprint(2, "OUT errors:\n");
			}
			write(1, buf, n);
		}
		write(fd, buf, n);
	}
}

void
bell(void *x, char *msg)
{
	if(strcmp(msg, "sys: child") == 0 || strcmp(msg, "sys: write on closed pipe") == 0)
		sysfatal("sftp exited");
	if(strcmp(msg, "alarm") == 0)
		noted(NCONT);
	noted(NDFLT);
}

void
main(int argc, char **argv)
{
	char buf[200], cmd[1000], *q, *s;
	char dir[100], ndir[100];
	int p[2], px[2], pe[2], pid, ctl, nctl, fd, n;

	notify(bell);
	fmtinstall('H', encodefmt);

	ARGBEGIN{
	case 'D':
		debug = 1;
		break;
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	if(pipe(p) < 0 || pipe(px) < 0 || pipe(pe) < 0)
		sysfatal("pipe: %r");
	pid = fork();
	if(pid < 0)
		sysfatal("fork: %r");
	if(pid == 0){
		close(p[1]);
		close(px[0]);
		close(pe[0]);
		dup(p[0], 0);
		dup(px[1], 1);
		dup(pe[1], 2);
		if(p[0] > 2)
			close(p[0]);
		if(px[1] > 2)
			close(px[1]);
		if(pe[1] > 2)
			close(pe[1]);
		execl("sftp", "sftp", "-b", "/dev/stdin", argv[0], nil);
		sysfatal("exec sftp: %r");
	}

	close(p[0]);
	close(px[1]);
	close(pe[1]);

	sftpfd = p[1];
	sftperr = pe[0];
	Binit(&bin, px[0], OREAD);

	fcntl(sftperr, F_SETFL, fcntl(sftperr, F_GETFL, 0)|O_NONBLOCK);

	do
		q = Brd(&bin);
	while(q && strcmp(q, "sftp> ") != 0);
	if(q == nil)
		sysfatal("unexpected eof");

	snprint(buf, sizeof buf, "unix!%s/%s.sftp", getns(), argv[0]);
	ctl = announce(buf, dir);
	if(ctl < 0)
		sysfatal("announce %s: %r", buf);

	pid = fork();
	if(pid < 0)
		sysfatal("fork");
	if(pid != 0)
		exits(nil);

	for(;;){
		nctl = listen(dir, ndir);
		if(nctl < 0)
			sysfatal("listen %s: %r", buf);
		fd = accept(ctl, ndir);
		close(nctl);
		if(fd < 0)
			continue;
		for(;;){
		/*	alarm(1000); */
			n = readstr(fd, cmd, sizeof cmd);
		/*	alarm(0); */
			if(n <= 0)
				break;
			dprint("CMD %s\n", cmd);
			if(strcmp(cmd, "DONE") == 0){
				fprint(fd, "DONE\n");
				break;
			}
			fprint(sftpfd, "-%s\n", cmd);
			q = Brd(&bin);
			if(*q==0 || q[strlen(q)-1] != '\n')
				sysfatal("unexpected response");
			q[strlen(q)-1] = 0;
			if(q[0] != '-' || strcmp(q+1, cmd) != 0)
				sysfatal("unexpected response");
			while((q = Brd(&bin)) != nil){
				if(strcmp(q, "sftp> ") == 0){
					doerrors(fd);
					break;
				}
				s = q+strlen(q);
				while(s > q && (s[-1] == ' ' || s[-1] == '\n' || s[-1] == '\t' || s[-1] == '\r'))
					s--;
				*s = 0;
				fprint(fd, "%s\n", q);
			}
			if(q == nil){
				fprint(fd, "!!! unexpected eof\n");
				sysfatal("unexpected eof");
			}
		}
		close(fd);
	}
}
