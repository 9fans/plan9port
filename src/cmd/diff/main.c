#include <u.h>
#include <libc.h>
#include <bio.h>
#include "diff.h"

#define	DIRECTORY(s)		((s)->qid.type&QTDIR)
#define	REGULAR_FILE(s)		((s)->type == 'M' && !DIRECTORY(s))

Biobuf	stdout;

static char *tmp[] = {"/tmp/diff1XXXXXXXXXXX", "/tmp/diff2XXXXXXXXXXX"};
static int whichtmp;
static char *progname;
static char usage[] = "diff [ -acefmnbwr ] file1 ... file2\n";

static void
rmtmpfiles(void)
{
	while (whichtmp > 0) {
		whichtmp--;
		remove(tmp[whichtmp]);
	}
}

void
done(int status)
{
	rmtmpfiles();
	switch(status)
	{
	case 0:
		exits("");
	case 1:
		exits("some");
	default:
		exits("error");
	}
	/*NOTREACHED*/
}

void
panic(int status, char *fmt, ...)
{
	va_list arg;

	Bflush(&stdout);

	fprint(2, "%s: ", progname);
	va_start(arg, fmt);
	vfprint(2, fmt, arg);
	va_end(arg);
	if (status)
		done(status);
		/*NOTREACHED*/
}

static int
catch(void *a, char *msg)
{
	USED(a);
	panic(2, msg);
	return 1;
}

int
mkpathname(char *pathname, char *path, char *name)
{
	if (strlen(path) + strlen(name) > MAXPATHLEN) {
		panic(0, "pathname %s/%s too long\n", path, name);
		return 1;
	}
	sprint(pathname, "%s/%s", path, name);
	return 0;
}

static char *
mktmpfile(int input, Dir **sb)
{
	int fd, i;
	char *p;
	char buf[8192];

	atnotify(catch, 1);
/*
	p = mktemp(tmp[whichtmp++]);
	fd = create(p, OWRITE, 0600);
*/
	fd = mkstemp(p=tmp[whichtmp++]);
	if (fd < 0) {
		panic(mflag ? 0: 2, "cannot create %s: %r\n", p);
		return 0;
	}
	while ((i = read(input, buf, sizeof(buf))) > 0) {
		if ((i = write(fd, buf, i)) < 0)
			break;
	}
	*sb = dirfstat(fd);
	close(fd);
	if (i < 0) {
		panic(mflag ? 0: 2, "cannot read/write %s: %r\n", p);
		return 0;
	}
	return p;
}

static char *
statfile(char *file, Dir **sb)
{
	Dir *dir;
	int input;

	dir = dirstat(file);
	if(dir == nil) {
		if (strcmp(file, "-") || (dir = dirfstat(0)) == nil) {
			panic(mflag ? 0: 2, "cannot stat %s: %r\n", file);
			return 0;
		}
		free(dir);
		return mktmpfile(0, sb);
	}
	else if (!REGULAR_FILE(dir) && !DIRECTORY(dir)) {
		free(dir);
		if ((input = open(file, OREAD)) == -1) {
			panic(mflag ? 0: 2, "cannot open %s: %r\n", file);
			return 0;
		}
		file = mktmpfile(input, sb);
		close(input);
	}
	else
		*sb = dir;
	return file;
}

void
diff(char *f, char *t, int level)
{
	char *fp, *tp, *p, fb[MAXPATHLEN+1], tb[MAXPATHLEN+1];
	Dir *fsb, *tsb;

	if ((fp = statfile(f, &fsb)) == 0)
		goto Return;
	if ((tp = statfile(t, &tsb)) == 0){
		free(fsb);
		goto Return;
	}
	if (DIRECTORY(fsb) && DIRECTORY(tsb)) {
		if (rflag || level == 0)
			diffdir(fp, tp, level);
		else
			Bprint(&stdout, "Common subdirectories: %s and %s\n",
				fp, tp);
	}
	else if (REGULAR_FILE(fsb) && REGULAR_FILE(tsb))
		diffreg(fp, tp);
	else {
		if (REGULAR_FILE(fsb)) {
			if ((p = utfrrune(f, '/')) == 0)
				p = f;
			else
				p++;
			if (mkpathname(tb, tp, p) == 0)
				diffreg(fp, tb);
		}
		else {
			if ((p = utfrrune(t, '/')) == 0)
				p = t;
			else
				p++;
			if (mkpathname(fb, fp, p) == 0)
				diffreg(fb, tp);
		}
	}
	free(fsb);
	free(tsb);
Return:
	rmtmpfiles();
}

void
main(int argc, char *argv[])
{
	char *p;
	int i;
	Dir *fsb, *tsb;
	extern int _p9usepwlibrary;

	_p9usepwlibrary = 0;
	Binit(&stdout, 1, OWRITE);
	progname = *argv;
	while (--argc && (*++argv)[0] == '-' && (*argv)[1]) {
		for (p = *argv+1; *p; p++) {
			switch (*p) {

			case 'e':
			case 'f':
			case 'n':
			case 'c':
			case 'a':
				mode = *p;
				break;

			case 'w':
				bflag = 2;
				break;

			case 'b':
				bflag = 1;
				break;

			case 'r':
				rflag = 1;
				mflag = 1;
				break;

			case 'm':
				mflag = 1;
				break;

			case 'h':
			default:
				progname = "Usage";
				panic(2, usage);
			}
		}
	}
	if (argc < 2)
		panic(2, usage, progname);
	if ((tsb = dirstat(argv[argc-1])) == nil)
		panic(2, "can't stat %s\n", argv[argc-1]);
	if (argc > 2) {
		if (!DIRECTORY(tsb))
			panic(2, usage, progname);
		mflag = 1;
	}
	else {
		if ((fsb = dirstat(argv[0])) == nil)
			panic(2, "can't stat %s\n", argv[0]);
		if (DIRECTORY(fsb) && DIRECTORY(tsb))
			mflag = 1;
		free(fsb);
	}
	free(tsb);
	for (i = 0; i < argc-1; i++)
		diff(argv[i], argv[argc-1], 0);
	done(anychange);
	/*NOTREACHED*/
}

static char noroom[] = "out of memory - try diff -h\n";

void *
emalloc(unsigned n)
{
	register void *p;

	if ((p = malloc(n)) == 0)
		panic(2, noroom);
	return p;
}

void *
erealloc(void *p, unsigned n)
{
	register void *rp;

	if ((rp = realloc(p, n)) == 0)
		panic(2, noroom);
	return rp;
}
