#include "stdinc.h"
#include "vac.h"
#include "dat.h"
#include "fns.h"

void usage(void);
int unvac(VacFS *fs);
int readScore(int fd, uchar score[VtScoreSize]);
static void warn(char *fmt, ...);
void dirlist(VacFS *fs, char *path);

static	int	nwant;
static	char	**want;
static	int	dflag = 1;
static	int	cflag;
static	int	lower;
static	int	verbose;
static	int	settimes;

void
main(int argc, char *argv[])
{
	char *zfile;
	int ok, table;
	VtSession *z;
	char *vsrv = nil;
	char *host = nil;
	char *p;
	int ncache = 1000;
	VacFS *fs;

	table = 0;
	zfile = nil;
	ARGBEGIN{
	case 'D':
		dflag++;
		break;
	case 'c':
		cflag++;
		break;
	case 'C':
		p = ARGF();
		if(p == nil)
			usage();
		ncache = atoi(p);
		if(ncache < 10)
			ncache = 10;
		if(ncache > 1000000)
			ncache = 1000000;
		break;
	case 'i':
		lower++;
		break;
	case 'f':
		zfile = ARGF();
		if(zfile == nil)
			usage();
		break;
	case 'h':
		host = ARGF();
		break;
	case 't':
		table++;
		break;
	case 'T':
		settimes++;
		break;
	case 's':
		vsrv = ARGF();
		break;
	case 'v':
		verbose++;
		break;
	default:
		usage();
		break;
	}ARGEND

	nwant = argc;
	want = argv;

	vtAttach();

	if(zfile == nil)
		usage();

	if(vsrv != nil)
		z = vtStdioServer(vsrv);
	else
		z = vtDial(host);
	if(z == nil)
		vtFatal("could not connect to server: %s", vtGetError());
	vtSetDebug(z, 0);
	if(!vtConnect(z, 0))
		vtFatal("vtConnect: %s", vtGetError());
	fs = vfsOpen(z, zfile, 1, ncache);
	if(fs == nil)
		vtFatal("vfsOpen: %s", vtGetError());
	ok = unvac(fs);
	vtClose(z);
	vtDetach();
	
	exits(ok? 0 : "error");
}

void
usage(void)
{
	fprint(2, "usage: %s [-tTcDv] -f zipfile [-s ventid] [-h host] [file ...]\n", argv0);
	exits("usage");
}

void
suck(VacFile *f)
{
	USED(f);
}


void
vacfile(VacFS *fs, char *path, VacDir *vd)
{
	char *path2;

	path2 = vtMemAlloc(strlen(path) + 1 + strlen(vd->elem) + 1);
	if(path[1] == 0)
		sprintf(path2, "/%s", vd->elem);
	else
		sprintf(path2, "%s/%s", path, vd->elem);
fprint(2, "vac file: %s\n", path2);
	if(vd->mode & ModeDir)
		dirlist(fs, path2);
	vtMemFree(path2);
}

void
dirlist(VacFS *fs, char *path)
{
	VacDir vd[50];
	VacDirEnum *ds;
	int i, n;

	ds = vdeOpen(fs, path);
	if(ds == nil) {
		fprint(2, "could not open: %s: %s\n", path, vtGetError());
		return;
	}
	for(;;) {
		n = vdeRead(ds, vd, sizeof(vd)/sizeof(VacDir));
		if(n < 0) {
			warn("vdRead failed: %s: %s", path, vtGetError());
			return;
		}
		if(n == 0)
			break;
		for(i=0; i<n; i++) {
			vacfile(fs, path, &vd[i]);
			vdCleanup(&vd[i]);
		}
	}
	vdeFree(ds);
}

int
unvac(VacFS *fs)
{
	dirlist(fs, "/");

	return 1;
}

static void
warn(char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	fprint(2, "%s: ", argv0);
	vfprint(2, fmt, arg);
	fprint(2, "\n");
	va_end(arg);
}
