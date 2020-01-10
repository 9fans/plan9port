/*
 * gs interface for page.
 * ps.c and pdf.c both use these routines.
 * a caveat: if you run more than one gs, only the last
 * one gets killed by killgs
 */
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <bio.h>
#include <cursor.h>
#include "page.h"

static int gspid;	/* globals for atexit */
static int gsfd;
static void	killgs(void);

static void
killgs(void)
{
	char tmpfile[100];

	close(gsfd);
	postnote(PNGROUP, getpid(), "die");

	/*
	 * from ghostscript's use.txt:
	 * ``Ghostscript currently doesn't do a very good job of deleting temporary
	 * files when it exits; you may have to delete them manually from time to
	 * time.''
	 */
	sprint(tmpfile, "/tmp/gs_%.5da", (gspid+300000)%100000);
	if(chatty) fprint(2, "remove %s...\n", tmpfile);
	remove(tmpfile);
	sleep(100);
	postnote(PNPROC, gspid, "die yankee pig dog");
}

void
spawnreader(void *cp)
{
	int n, fd, pfd[2];
	char buf[1024];

	recv(cp, &fd);

	if(pipe(pfd)<0)
		wexits("pipe failed");

	send(cp, &pfd[1]);

	while((n=read(pfd[0], buf, sizeof buf)) > 0) {
		write(1, buf, n);
		write(fd, buf, n);
	}

	close(pfd[0]);
	threadexits(0);
}

void
spawnmonitor(void *cp)
{
	char buf[4096];
	char *xbuf;
	int fd;
	int n;
	int out;
	int first;

	recv(cp, &fd);

	out = open("/dev/tty", OWRITE);
	if(out < 0)
		out = 2;

	xbuf = buf;	/* for ease of acid */
	first = 1;
	while((n = read(fd, xbuf, sizeof buf)) > 0){
		if(first){
			first = 0;
			fprint(2, "Ghostscript Error:\n");
		}
		write(out, xbuf, n);
		alarm(500);
	}
	threadexits(0);
}

int
spawngs(GSInfo *g, char *safer)
{
	Channel *cp;
	char *args[16];
	char tb[32], gb[32];
	int i, nargs;
	int devnull;
	int stdinp[2];
	int stdoutp[2];
	int dataout[2];
	int errout[2];

	/*
	 * spawn gs
	 *
 	 * gs's standard input is fed from stdinout.
	 * gs output written to fd-2 (i.e. output we generate intentionally) is fed to stdinout.
	 * gs output written to fd 1 (i.e. ouptut gs generates on error) is fed to errout.
	 * gs data output is written to fd 3, which is dataout.
	 */
	if(pipe(stdinp)<0 || pipe(stdoutp)<0 || pipe(dataout)<0 || pipe(errout)<0)
		return -1;

	nargs = 0;
	args[nargs++] = "gs";
	args[nargs++] = "-dNOPAUSE";
	args[nargs++] = "-dNOPROMPT";
	args[nargs++] = "-dDELAYSAFER";
	args[nargs++] = "-dQUIET";
	args[nargs++] = "-sDEVICE=bmp16m";
	args[nargs++] = "-sOutputFile=/dev/fd/3";
	args[nargs++] = "-r100";
	sprint(tb, "-dTextAlphaBits=%d", textbits);
	sprint(gb, "-dGraphicsAlphaBits=%d", gfxbits);
	if(textbits)
		args[nargs++] = tb;
	if(gfxbits)
		args[nargs++] = gb;
	args[nargs] = nil;

	gspid = fork();
	if(gspid == 0) {
		close(stdinp[1]);
		close(stdoutp[0]);
		close(dataout[0]);
		close(errout[0]);

		/*
		 * Horrible problem: we want to dup fd's 0-4 below,
		 * but some of the source fd's might have those small numbers.
		 * So we need to reallocate those.  In order to not step on
		 * anything else, we'll dup the fd's to higher ones using
		 * dup(x, -1), but we need to use up the lower ones first.
		 */
		while((devnull = open("/dev/null", ORDWR)) < 5)
			;

		stdinp[0] = dup(stdinp[0], -1);
		stdoutp[1] = dup(stdoutp[1], -1);
		errout[1] = dup(errout[1], -1);
		dataout[1] = dup(dataout[1], -1);

		dup(stdinp[0], 0);
		dup(errout[1], 1);
		dup(devnull, 2);	/* never anything useful */
		dup(dataout[1], 3);
		dup(stdoutp[1], 4);
		for(i=5; i<20; i++)
			close(i);
		execvp("gs", args);
		wexits("exec");
	}
	close(stdinp[0]);
	close(stdoutp[1]);
	close(errout[1]);
	close(dataout[1]);
	atexit(killgs);

	cp = chancreate(sizeof(int), 0);
	if(teegs) {
		proccreate(spawnreader, cp, mainstacksize);
		send(cp, &stdoutp[0]);
		recv(cp, &stdoutp[0]);
	}

	gsfd = g->gsfd = stdinp[1];
	g->gspid = gspid;
	g->g.fd = dataout[0];
	g->g.name = "gs pipe";
	g->g.type = Ibmp;

	proccreate(spawnmonitor, cp, mainstacksize);
	send(cp, &errout[0]);
	chanfree(cp);

	Binit(&g->gsrd, stdoutp[0], OREAD);

	gscmd(g, "/PAGEDIDSHOWPAGE false def\n");
	gscmd(g, "/showpage { /PAGEDIDSHOWPAGE true def showpage } bind def\n");
	gscmd(g, "/PAGEFLUSH { PAGEDIDSHOWPAGE not {showpage} if /PAGEDIDSHOWPAGE false def } def\n");

	gscmd(g, "/PAGEOUT (/dev/fd/4) (w) file def\n");
	if(!strcmp(safer, "-dSAFER"))
		gscmd(g, ".setsafe\n");
	gscmd(g, "/PAGE== { PAGEOUT exch write==only PAGEOUT (\\n) writestring PAGEOUT flushfile } def\n");
	waitgs(g);

	return 0;
}

int
gscmd(GSInfo *gs, char *fmt, ...)
{
	char buf[1024];
	int n;

	va_list v;
	va_start(v, fmt);
	n = vseprint(buf, buf+sizeof buf, fmt, v) - buf;
	if(n <= 0)
		return n;

	if(chatty) {
		fprint(2, "cmd: ");
		write(2, buf, n);
	}

	if(write(gs->gsfd, buf, n) != 0)
		return -1;

	return n;
}

/*
 * set the dimensions of the bitmap we expect to get back from GS.
 */
void
setdim(GSInfo *gs, Rectangle bbox, int ppi, int landscape)
{
	Rectangle pbox;

	if(chatty)
		fprint(2, "setdim: bbox=%R\n", bbox);

	if(ppi)
		gs->ppi = ppi;

	gscmd(gs, "mark\n");
	if(ppi)
		gscmd(gs, "/HWResolution [%d %d]\n", ppi, ppi);

	if(!Dx(bbox))
		bbox = Rect(0, 0, 612, 792);	/* 8½×11 */

	switch(landscape){
	case 0:
		pbox = bbox;
		break;
	default:
		pbox = Rect(bbox.min.y, bbox.min.x, bbox.max.y, bbox.max.x);
		break;
	}
	gscmd(gs, "/PageSize [%d %d]\n", Dx(pbox), Dy(pbox));
	gscmd(gs, "/Margins [%d %d]\n", -pbox.min.x, -pbox.min.y);
	gscmd(gs, "currentdevice putdeviceprops pop\n");
	gscmd(gs, "/#copies 1 store\n");

	if(!eqpt(bbox.min, ZP))
		gscmd(gs, "%d %d translate\n", -bbox.min.x, -bbox.min.y);

	switch(landscape){
	case 0:
		break;
	case 1:
		gscmd(gs, "%d 0 translate\n", Dy(bbox));
		gscmd(gs, "90 rotate\n");
		break;
	}

	waitgs(gs);
}

void
waitgs(GSInfo *gs)
{
	/* we figure out that gs is done by telling it to
	 * print something and waiting until it does.
	 */
	char *p;
	Biobuf *b = &gs->gsrd;
	uchar buf[1024];
	int n;

//	gscmd(gs, "(\\n**bstack\\n) print flush\n");
//	gscmd(gs, "stack flush\n");
//	gscmd(gs, "(**estack\\n) print flush\n");
	gscmd(gs, "(\\n//GO.SYSIN DD\\n) PAGE==\n");

	alarm(300*1000);
	for(;;) {
		p = Brdline(b, '\n');
		if(p == nil) {
			n = Bbuffered(b);
			if(n <= 0)
				break;
			if(n > sizeof buf)
				n = sizeof buf;
			Bread(b, buf, n);
			continue;
		}
		p[Blinelen(b)-1] = 0;
		if(chatty) fprint(2, "p: ");
		if(chatty) write(2, p, Blinelen(b)-1);
		if(chatty) fprint(2, "\n");
		if(strstr(p, "Error:")) {
			alarm(0);
			fprint(2, "ghostscript error: %s\n", p);
			wexits("gs error");
		}

		if(strstr(p, "//GO.SYSIN DD")) {
			break;
		}
	}
	alarm(0);
}
