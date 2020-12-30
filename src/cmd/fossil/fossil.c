#include "stdinc.h"
#include <ctype.h>

#include "9.h"

int Dflag;
int mempcnt;			/* for 9fsys.c */
char* none = "none";
char* foptname = "/none/such";

static void
usage(void)
{
	fprint(2, "usage: %s [-Dt] [-c cmd] [-f partition] [-m %%]\n", argv0);
	threadexitsall("usage");
}

static void
readCmdPart(char *file, char ***pcmd, int *pncmd)
{
	char buf[1024+1], *f[1024];
	char tbuf[1024];
	int nf;
	int i, fd, n;
	char **cmd, *p;
	int ncmd;

	cmd = *pcmd;
	ncmd = *pncmd;

	if((fd = open(file, OREAD)) < 0)
		sysfatal("open %s: %r", file);
	if(seek(fd, 127*1024, 0) != 127*1024)
		sysfatal("seek %s 127kB: %r", file);
	n = readn(fd, buf, sizeof buf-1);
	if(n == 0)
		sysfatal("short read of %s at 127kB", file);
	if(n < 0)
		sysfatal("read %s: %r", file);
	buf[n] = 0;
	if(memcmp(buf, "fossil config\n", 6+1+6+1) != 0)
		sysfatal("bad config magic in %s", file);
	nf = getfields(buf+6+1+6+1, f, nelem(f), 1, "\n");
	for(i=0; i<nf; i++){
		if(f[i][0] == '#')
			continue;
		cmd = vtrealloc(cmd, (ncmd+1)*sizeof(char*));
		/* expand argument '*' to mean current file */
		if((p = strchr(f[i], '*')) && (p==f[i]||isspace(p[-1])) && (p[1]==0||isspace(p[1]))){
			memmove(tbuf, f[i], p-f[i]);
			strecpy(tbuf+(p-f[i]), tbuf+sizeof tbuf, file);
			strecpy(tbuf+strlen(tbuf), tbuf+sizeof tbuf, p+1);
			f[i] = tbuf;
		}
		cmd[ncmd++] = vtstrdup(f[i]);
	}
	close(fd);
	*pcmd = cmd;
	*pncmd = ncmd;
}

int
threadmaybackground(void)
{
	return 1;
}

void
threadmain(int argc, char* argv[])
{
	char **cmd, *p;
	int i, ncmd, tflag;

	fmtinstall('D', dirfmt);
	fmtinstall('F', fcallfmt);
	fmtinstall('M', dirmodefmt);
	quotefmtinstall();

	/*
	 * Insulate from the invoker's environment.
	 */
#ifdef PLAN9PORT
	if(rfork(RFNAMEG) < 0)
#else
	if(rfork(RFREND|RFNOTEG|RFNAMEG) < 0)
#endif
		sysfatal("rfork: %r");

	close(0);
	open("/dev/null", OREAD);
	close(1);
	open("/dev/null", OWRITE);

	cmd = nil;
	ncmd = tflag = 0;

	ARGBEGIN{
	case '?':
	default:
		usage();
		break;
	case 'c':
		p = EARGF(usage());
		currfsysname = p;
		cmd = vtrealloc(cmd, (ncmd+1)*sizeof(char*));
		cmd[ncmd++] = p;
		break;
	case 'D':
		Dflag ^= 1;
		break;
	case 'f':
		p = EARGF(usage());
		currfsysname = foptname = p;
		readCmdPart(p, &cmd, &ncmd);
		break;
	case 'm':
		mempcnt = atoi(EARGF(usage()));
		if(mempcnt <= 0 || mempcnt >= 100)
			usage();
		break;
	case 't':
		tflag = 1;
		break;
	}ARGEND
	if(argc != 0)
		usage();

	consInit();
	cliInit();
	msgInit();
	conInit();
	cmdInit();
	fsysInit();
	exclInit();
	fidInit();

	srvInit();
	lstnInit();
	usersInit();

	for(i = 0; i < ncmd; i++)
		if(cliExec(cmd[i]) == 0)
			fprint(2, "%s: %r\n", cmd[i]);
	vtfree(cmd);

	if(tflag && consTTY() == 0)
		consPrint("%r\n");

	threadexits(0);
}
