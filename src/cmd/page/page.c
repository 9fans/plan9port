#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <bio.h>
#include <cursor.h>
#include "page.h"

int resizing;
int mknewwindow;
int doabort;
int chatty;
int reverse = -1;
int goodps = 1;
int ppi = 100;
int teegs = 0;
int truetoboundingbox;
int textbits=4, gfxbits=4;
int stdinfd;
int truecolor;
int imagemode;
int notewatcher;
int notegp;
int fitwin;
char tempfile[40];

int
watcher(void *v, char *x)
{
	USED(v);
	if(strcmp(x, "die") != 0)
		postnote(PNGROUP, notegp, x);
	threadexitsall(0);
	return 0;
}

void
watcherproc(void *v)
{
	threadnotify(watcher, 1);
	for(;;)
		sleep(1000);
}

int
bell(void *u, char *x)
{
	if(x && strcmp(x, "hangup") == 0)
		threadexitsall(0);

	if(x && strstr(x, "die") == nil)
		fprint(2, "postnote %d: %s\n", getpid(), x);

	/* alarms come from the gs monitor */
	if(x && strstr(x, "alarm")){
		postnote(PNGROUP, getpid(), "die (gs error)");
		postnote(PNPROC, notewatcher, "die (gs error)");
	}

	/* function mentions u so that it's in the stack trace */
	if((u == nil || u != x) && doabort)
		abort();

/*	fprint(2, "exiting %d\n", getpid()); */
	wexits("note");
	return 0;
}

static int
afmt(Fmt *fmt)
{
	char *s;

	s = va_arg(fmt->args, char*);
	if(s == nil || s[0] == '\0')
		return fmtstrcpy(fmt, "");
	else
		return fmtprint(fmt, "%#q", s);
}

void
usage(void)
{
	fprint(2, "usage: page [-biRrwf] [-p ppi] file...\n");
	wexits("usage");
}

void
cleanup(void)
{
	remove(tempfile);
}

void
threadmain(int argc, char **argv)
{
	Document *doc;
	Biobuf *b;
	char *basename = argv[0];
	enum { Ninput = 16 };
	uchar buf[Ninput+1];
	int readstdin;

	ARGBEGIN{
	/* "temporary" debugging options */
	case 'P':
		goodps = 0;
		break;
	case 'v':
		chatty++;
		break;
	case 'V':
		teegs++;
		break;
	case 'a':
		doabort++;
		break;
	case 'T':
		textbits = atoi(EARGF(usage()));
		gfxbits = atoi(EARGF(usage()));
		break;

	/* real options */
	case 'R':
		resizing = 1;
		break;
	case 'r':
		reverse = 1;
		break;
	case 'p':
		ppi = atoi(EARGF(usage()));
		break;
	case 'b':
		truetoboundingbox = 1;
		break;
	case 'w':
		fprint(2, "%s: -w has only the effect of -R X11 systems\n", basename);
		resizing = 1;
		break;
	case 'i':
		imagemode = 1;
		break;
	case 'W':
		winsize = EARGF(usage());
		break;
	case 'f':
		fitwin = 1;
		break;
	default:
		usage();
	}ARGEND;

	notegp = getpid();

	notewatcher = proccreate(watcherproc, NULL, 1024);
	if(notewatcher == -1){
		sysfatal("proccreate");
		threadexitsall(0);
	}

	rfork(RFNOTEG);
	threadnotify(bell, 1);

	readstdin = 0;
	if(imagemode == 0 && argc == 0){
		readstdin = 1;
		stdinfd = dup(0, -1);
		close(0);
		open("/dev/tty", OREAD);
	}

	quotefmtinstall();
	fmtinstall('a', afmt);

	fmtinstall('R', Rfmt);
	fmtinstall('P', Pfmt);
	/*
	if(mknewwindow)
		newwin(); */

	if(readstdin){
		b = nil;
		if(readn(stdinfd, buf, Ninput) != Ninput){
			fprint(2, "page: short read reading %s\n", argv[0]);
			wexits("read");
		}

		atexit(cleanup);
	}else if(argc != 0){
		if(!(b = Bopen(argv[0], OREAD))) {
			fprint(2, "page: cannot open \"%s\"\n", argv[0]);
			wexits("open");
		}

		if(Bread(b, buf, Ninput) != Ninput) {
			fprint(2, "page: short read reading %s\n", argv[0]);
			wexits("read");
		}
	}else
		b = nil;

	buf[Ninput] = '\0';
	if(imagemode)
		doc = initgfx(nil, 0, nil, nil, 0);
	else if(strncmp((char*)buf, "%PDF-", 5) == 0)
		doc = initpdf(b, argc, argv, buf, Ninput);
	else if(strncmp((char*)buf, "\x04%!", 2) == 0)
		doc = initps(b, argc, argv, buf, Ninput);
	else if(buf[0] == '\x1B' && strstr((char*)buf, "@PJL"))
		doc = initps(b, argc, argv, buf, Ninput);
	else if(strncmp((char*)buf, "%!", 2) == 0)
		doc = initps(b, argc, argv, buf, Ninput);
	else if(strcmp((char*)buf, "\xF7\x02\x01\x83\x92\xC0\x1C;") == 0)
		doc = initdvi(b, argc, argv, buf, Ninput);
	else if(strncmp((char*)buf, "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1", 8) == 0)
		doc = initmsdoc(b, argc, argv, buf, Ninput);
	else if(strncmp((char*)buf, "x T ", 4) == 0)
		doc = inittroff(b, argc, argv, buf, Ninput);
	else {
		if(ppi != 100) {
			fprint(2, "page: you can't specify -p with graphic files\n");
			wexits("-p and graphics");
		}
		doc = initgfx(b, argc, argv, buf, Ninput);
	}

	if(doc == nil) {
		fprint(2, "page: error reading file: %r\n");
		wexits("document init");
	}

	if(doc->npage < 1 && !imagemode) {
		fprint(2, "page: no pages found?\n");
		wexits("pagecount");
	}

	if(reverse == -1) /* neither cmdline nor ps reader set it */
		reverse = 0;

	if(initdraw(0, 0, "page") < 0){
		fprint(2, "page: initdraw failed: %r\n");
		wexits("initdraw");
	}
	display->locking = 1;

	truecolor = screen->depth > 8;
	viewer(doc);
	wexits(0);
}

void
wexits(char *s)
{
	if(s && *s && strcmp(s, "note") != 0 && mknewwindow)
		sleep(10*1000);
	postnote(PNPROC, notewatcher, "die");
	postnote(PNGROUP, getpid(), "die");
	threadexitsall(s);
}
