#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <bio.h>
#include "page.h"

Document*
initfilt(Biobuf *b, int argc, char **argv, uchar *buf, int nbuf, char *type, char *cmd, int docopy)
{
	int ofd;
	int p[2];
	char xbuf[8192];
	int n;

	if(argc > 1) {
		fprint(2, "can only view one %s file at a time\n", type);
		return nil;
	}

	fprint(2, "converting from %s to postscript...\n", type);

	if(docopy){
		if(pipe(p) < 0){
			fprint(2, "pipe fails: %r\n");
			exits("Epipe");
		}
	}else{
		p[0] = open("/dev/null", ORDWR);
		p[1] = open("/dev/null", ORDWR);
	}

	ofd = opentemp("/tmp/pagecvtXXXXXXXXX");
	switch(fork()){
	case -1:
		fprint(2, "fork fails: %r\n");
		exits("Efork");
	default:
		close(p[1]);
		if(docopy){
			write(p[0], buf, nbuf);
			if(b)
				while((n = Bread(b, xbuf, sizeof xbuf)) > 0)
					write(p[0], xbuf, n);
			else
				while((n = read(stdinfd, xbuf, sizeof xbuf)) > 0)
					write(p[0], xbuf, n);
		}
		close(p[0]);
		waitpid();
		break;
	case 0:
		close(p[0]);
		dup(p[1], 0);
		dup(ofd, 1);
		/* stderr shines through */
		execl("/bin/rc", "rc", "-c", cmd, nil);
		break;
	}

	if(b)
		Bterm(b);
	seek(ofd, 0, 0);
	b = emalloc(sizeof(Biobuf));
	Binit(b, ofd, OREAD);

	return initps(b, argc, argv, nil, 0);
}

Document*
initdvi(Biobuf *b, int argc, char **argv, uchar *buf, int nbuf)
{
	int fd;
	char *name;
	char cmd[256];
	char fdbuf[20];

	/*
	 * Stupid DVIPS won't take standard input.
	 */
	if(b == nil){	/* standard input; spool to disk (ouch) */
		fd = spooltodisk(buf, nbuf, &name);
		sprint(fdbuf, "/fd/%d", fd);
		b = Bopen(fdbuf, OREAD);
		if(b == nil){
			fprint(2, "cannot open disk spool file\n");
			wexits("Bopen temp");
		}
		argv = &name;
		argc = 1;
	}

	snprint(cmd, sizeof cmd, "dvips -Pps -r0 -q1 -f1 '%s'", argv[0]);
	return initfilt(b, argc, argv, buf, nbuf, "dvi", cmd, 0);
}

Document*
inittroff(Biobuf *b, int argc, char **argv, uchar *buf, int nbuf)
{
	return initfilt(b, argc, argv, buf, nbuf, "troff", "lp -dstdout", 1);
}

Document*
initmsdoc(Biobuf *b, int argc, char **argv, uchar *buf, int nbuf)
{
	return initfilt(b, argc, argv, buf, nbuf, "microsoft office", "doc2ps", 1);
}
