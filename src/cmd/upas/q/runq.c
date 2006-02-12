#include "common.h"
#include <ctype.h>

void	doalldirs(void);
void	dodir(char*);
void	dofile(Dir*);
void	rundir(char*);
char*	file(char*, char);
void	warning(char*, void*);
void	error(char*, void*);
int	returnmail(char**, char*, char*);
void	logit(char*, char*, char**);
void	doload(int);

#define HUNK 32
char	*cmd;
char	*root;
int	debug;
int	giveup = 2*24*60*60;
int	load;
int	limit;

/* the current directory */
Dir	*dirbuf;
long	ndirbuf = 0;
int	nfiles;
char	*curdir;

char *runqlog = "runq";

int	*pidlist;
char	**badsys;		/* array of recalcitrant systems */
int	nbad;
int	npid = 50;
int	sflag;			/* single thread per directory */
int	aflag;			/* all directories */
int	Eflag;			/* ignore E.xxxxxx dates */
int	Rflag;			/* no giving up, ever */

void
usage(void)
{
	fprint(2, "usage: runq [-adsE] [-q dir] [-l load] [-t time] [-r nfiles] [-n nprocs] q-root cmd\n");
	exits("");
}

void
main(int argc, char **argv)
{
	char *qdir, *x;

	qdir = 0;

	ARGBEGIN{
	case 'l':
		x = ARGF();
		if(x == 0)
			usage();
		load = atoi(x);
		if(load < 0)
			load = 0;
		break;
	case 'E':
		Eflag++;
		break;
	case 'R':	/* no giving up -- just leave stuff in the queue */
		Rflag++;
		break;
	case 'a':
		aflag++;
		break;
	case 'd':
		debug++;
		break;
	case 'r':
		limit = atoi(ARGF());
		break;
	case 's':
		sflag++;
		break;
	case 't':
		giveup = 60*60*atoi(ARGF());
		break;
	case 'q':
		qdir = ARGF();
		if(qdir == 0)
			usage();
		break;
	case 'n':
		npid = atoi(ARGF());
		if(npid == 0)
			usage();
		break;
	}ARGEND;

	if(argc != 2)
		usage();

	pidlist = malloc(npid*sizeof(*pidlist));
	if(pidlist == 0)
		error("can't malloc", 0);

	if(aflag == 0 && qdir == 0) {
		qdir = getuser();
		if(qdir == 0)
			error("unknown user", 0);
	}
	root = argv[0];
	cmd = argv[1];

	if(chdir(root) < 0)
		error("can't cd to %s", root);

	doload(1);
	if(aflag)
		doalldirs();
	else
		dodir(qdir);
	doload(0);
	exits(0);
}

int
emptydir(char *name)
{
	int fd;
	long n;
	char buf[2048];

	fd = open(name, OREAD);
	if(fd < 0)
		return 1;
	n = read(fd, buf, sizeof(buf));
	close(fd);
	if(n <= 0) {
		if(debug)
			fprint(2, "removing directory %s\n", name);
		syslog(0, runqlog, "rmdir %s", name);
		sysremove(name);
		return 1;
	}
	return 0;
}

int
forkltd(void)
{
	int i;
	int pid;

	for(i = 0; i < npid; i++){
		if(pidlist[i] <= 0)
			break;
	}

	while(i >= npid){
		pid = waitpid();
		if(pid < 0){
			syslog(0, runqlog, "forkltd confused");
			exits(0);
		}

		for(i = 0; i < npid; i++)
			if(pidlist[i] == pid)
				break;
	}
	pidlist[i] = fork();
	return pidlist[i];
}

/*
 *  run all user directories, must be bootes (or root on unix) to do this
 */
void
doalldirs(void)
{
	Dir *db;
	int fd;
	long i, n;


	fd = open(".", OREAD);
	if(fd == -1){
		warning("reading %s", root);
		return;
	}
	n = sysdirreadall(fd, &db);
	if(n > 0){
		for(i=0; i<n; i++){
			if(db[i].qid.type & QTDIR){
				if(emptydir(db[i].name))
					continue;
				switch(forkltd()){
				case -1:
					syslog(0, runqlog, "out of procs");
					doload(0);
					exits(0);
				case 0:
					if(sysdetach() < 0)
						error("%r", 0);
					dodir(db[i].name);
					exits(0);
				default:
					break;
				}
			}
		}
		free(db);
	}
	close(fd);
}

/*
 *  cd to a user directory and run it
 */
void
dodir(char *name)
{
	curdir = name;

	if(chdir(name) < 0){
		warning("cd to %s", name);
		return;
	}
	if(debug)
		fprint(2, "running %s\n", name);
	rundir(name);
	chdir("..");
}

/*
 *  run the current directory
 */
void
rundir(char *name)
{
	int fd;
	long i;

	if(aflag && sflag)
		fd = sysopenlocked(".", OREAD);
	else
		fd = open(".", OREAD);
	if(fd == -1){
		warning("reading %s", name);
		return;
	}
	nfiles = sysdirreadall(fd, &dirbuf);
	if(nfiles > 0){
		for(i=0; i<nfiles; i++){
			if(dirbuf[i].name[0]!='C' || dirbuf[i].name[1]!='.')
				continue;
			dofile(&dirbuf[i]);
		}
		free(dirbuf);
	}
	if(aflag && sflag)
		sysunlockfile(fd);
	else
		close(fd);
}

/*
 *  free files matching name in the current directory
 */
void
remmatch(char *name)
{
	long i;

	syslog(0, runqlog, "removing %s/%s", curdir, name);

	for(i=0; i<nfiles; i++){
		if(strcmp(&dirbuf[i].name[1], &name[1]) == 0)
			sysremove(dirbuf[i].name);
	}

	/* error file (may have) appeared after we read the directory */
	/* stomp on data file in case of phase error */
	sysremove(file(name, 'D'));
	sysremove(file(name, 'E'));
}

/*
 *  like trylock, but we've already got the lock on fd,
 *  and don't want an L. lock file.
 */
static Mlock *
keeplockalive(char *path, int fd)
{
	char buf[1];
	Mlock *l;

	l = malloc(sizeof(Mlock));
	if(l == 0)
		return 0;
	l->fd = fd;
	l->name = s_new();
	s_append(l->name, path);

	/* fork process to keep lock alive until sysunlock(l) */
	switch(l->pid = rfork(RFPROC)){
	default:
		break;
	case 0:
		fd = l->fd;
		for(;;){
			sleep(1000*60);
			if(pread(fd, buf, 1, 0) < 0)
				break;
		}
		_exits(0);
	}
	return l;
}

/*
 *  try a message
 */
void
dofile(Dir *dp)
{
	Dir *d;
	int dfd, ac, dtime, efd, pid, i, etime;
	char *buf, *cp, **av;
	Waitmsg *wm;
	Biobuf *b;
	Mlock *l = nil;

	if(debug)
		fprint(2, "dofile %s\n", dp->name);
	/*
	 *  if no data file or empty control or data file, just clean up
	 *  the empty control file must be 15 minutes old, to minimize the
	 *  chance of a race.
	 */
	d = dirstat(file(dp->name, 'D'));
	if(d == nil){
		syslog(0, runqlog, "no data file for %s", dp->name);
		remmatch(dp->name);
		return;
	}
	if(dp->length == 0){
		if(time(0)-dp->mtime > 15*60){
			syslog(0, runqlog, "empty ctl file for %s", dp->name);
			remmatch(dp->name);
		}
		return;
	}
	dtime = d->mtime;
	free(d);

	/*
	 *  retry times depend on the age of the errors file
	 */
	if(!Eflag && (d = dirstat(file(dp->name, 'E'))) != nil){
		etime = d->mtime;
		free(d);
		if(etime - dtime < 60*60){
			/* up to the first hour, try every 15 minutes */
			if(time(0) - etime < 15*60)
				return;
		} else {
			/* after the first hour, try once an hour */
			if(time(0) - etime < 60*60)
				return;
		}

	}

	/*
	 *  open control and data
	 */
	b = sysopen(file(dp->name, 'C'), "rl", 0660);
	if(b == 0) {
		if(debug)
			fprint(2, "can't open %s: %r\n", file(dp->name, 'C'));
		return;
	}
	dfd = open(file(dp->name, 'D'), OREAD);
	if(dfd < 0){
		if(debug)
			fprint(2, "can't open %s: %r\n", file(dp->name, 'D'));
		Bterm(b);
		sysunlockfile(Bfildes(b));
		return;
	}

	/*
	 *  make arg list
	 *	- read args into (malloc'd) buffer
	 *	- malloc a vector and copy pointers to args into it
	 */
	buf = malloc(dp->length+1);
	if(buf == 0){
		warning("buffer allocation", 0);
		Bterm(b);
		sysunlockfile(Bfildes(b));
		close(dfd);
		return;
	}
	if(Bread(b, buf, dp->length) != dp->length){
		warning("reading control file %s\n", dp->name);
		Bterm(b);
		sysunlockfile(Bfildes(b));
		close(dfd);
		free(buf);
		return;
	}
	buf[dp->length] = 0;
	av = malloc(2*sizeof(char*));
	if(av == 0){
		warning("argv allocation", 0);
		close(dfd);
		free(buf);
		Bterm(b);
		sysunlockfile(Bfildes(b));
		return;
	}
	for(ac = 1, cp = buf; *cp; ac++){
		while(isspace(*cp))
			*cp++ = 0;
		if(*cp == 0)
			break;

		av = realloc(av, (ac+2)*sizeof(char*));
		if(av == 0){
			warning("argv allocation", 0);
			close(dfd);
			free(buf);
			Bterm(b);
			sysunlockfile(Bfildes(b));
			return;
		}
		av[ac] = cp;
		while(*cp && !isspace(*cp)){
			if(*cp++ == '"'){
				while(*cp && *cp != '"')
					cp++;
				if(*cp)
					cp++;
			}
		}
	}
	av[0] = cmd;
	av[ac] = 0;

	if(!Eflag &&time(0) - dtime > giveup){
		if(returnmail(av, dp->name, "Giveup") != 0)
			logit("returnmail failed", dp->name, av);
		remmatch(dp->name);
		goto done;
	}

	for(i = 0; i < nbad; i++){
		if(strcmp(av[3], badsys[i]) == 0)
			goto done;
	}

	/*
	 * Ken's fs, for example, gives us 5 minutes of inactivity before
	 * the lock goes stale, so we have to keep reading it.
 	 */
	l = keeplockalive(file(dp->name, 'C'), Bfildes(b));

	/*
	 *  transfer
	 */
	pid = fork();
	switch(pid){
	case -1:
		sysunlock(l);
		sysunlockfile(Bfildes(b));
		syslog(0, runqlog, "out of procs");
		exits(0);
	case 0:
		if(debug) {
			fprint(2, "Starting %s", cmd);
			for(ac = 0; av[ac]; ac++)
				fprint(2, " %s", av[ac]);
			fprint(2, "\n");
		}
		logit("execing", dp->name, av);
		close(0);
		dup(dfd, 0);
		close(dfd);
		close(2);
		efd = open(file(dp->name, 'E'), OWRITE);
		if(efd < 0){
			if(debug) syslog(0, "runq", "open %s as %s: %r", file(dp->name,'E'), getuser());
			efd = create(file(dp->name, 'E'), OWRITE, 0666);
			if(efd < 0){
				if(debug) syslog(0, "runq", "create %s as %s: %r", file(dp->name, 'E'), getuser());
				exits("could not open error file - Retry");
			}
		}
		seek(efd, 0, 2);
		exec(cmd, av);
		error("can't exec %s", cmd);
		break;
	default:
		for(;;){
			wm = wait();
			if(wm == nil)
				error("wait failed: %r", "");
			if(wm->pid == pid)
				break;
			free(wm);
		}
		if(debug)
			fprint(2, "wm->pid %d wm->msg == %s\n", wm->pid, wm->msg);

		if(wm->msg[0]){
			if(debug)
				fprint(2, "[%d] wm->msg == %s\n", getpid(), wm->msg);
			if(!Rflag && atoi(wm->msg) != RetryCode){
				/* return the message and remove it */
				if(returnmail(av, dp->name, wm->msg) != 0)
					logit("returnmail failed", dp->name, av);
				remmatch(dp->name);
			} else {
				/* add sys to bad list and try again later */
				nbad++;
				badsys = realloc(badsys, nbad*sizeof(char*));
				badsys[nbad-1] = strdup(av[3]);
			}
		} else {
			/* it worked remove the message */
			remmatch(dp->name);
		}
		free(wm);

	}
done:
	if (l)
		sysunlock(l);
	Bterm(b);
	sysunlockfile(Bfildes(b));
	free(buf);
	free(av);
	close(dfd);
}


/*
 *  return a name starting with the given character
 */
char*
file(char *name, char type)
{
	static char nname[Elemlen+1];

	strncpy(nname, name, Elemlen);
	nname[Elemlen] = 0;
	nname[0] = type;
	return nname;
}

/*
 *  send back the mail with an error message
 *
 *  return 0 if successful
 */
int
returnmail(char **av, char *name, char *msg)
{
	int pfd[2];
	Waitmsg *wm;
	int fd;
	char buf[256];
	char attachment[256];
	int i;
	long n;
	String *s;
	char *sender;

	if(av[1] == 0 || av[2] == 0){
		logit("runq - dumping bad file", name, av);
		return 0;
	}

	s = unescapespecial(s_copy(av[2]));
	sender = s_to_c(s);

	if(!returnable(sender) || strcmp(sender, "postmaster") == 0) {
		logit("runq - dumping p to p mail", name, av);
		return 0;
	}

	if(pipe(pfd) < 0){
		logit("runq - pipe failed", name, av);
		return -1;
	}

	switch(rfork(RFFDG|RFPROC|RFENVG)){
	case -1:
		logit("runq - fork failed", name, av);
		return -1;
	case 0:
		logit("returning", name, av);
		close(pfd[1]);
		close(0);
		dup(pfd[0], 0);
		close(pfd[0]);
		putenv("upasname", "/dev/null");
		snprint(buf, sizeof(buf), "%s/marshal", UPASBIN);
		snprint(attachment, sizeof(attachment), "%s", file(name, 'D'));
		execl(buf, "send", "-A", attachment, "-s", "permanent failure", sender, nil);
		error("can't exec", 0);
		break;
	default:
		break;
	}

	close(pfd[0]);
	fprint(pfd[1], "\n");	/* get out of headers */
	if(av[1]){
		fprint(pfd[1], "Your request ``%.20s ", av[1]);
		for(n = 3; av[n]; n++)
			fprint(pfd[1], "%s ", av[n]);
	}
	fprint(pfd[1], "'' failed (code %s).\nThe symptom was:\n\n", msg);
	fd = open(file(name, 'E'), OREAD);
	if(fd >= 0){
		for(;;){
			n = read(fd, buf, sizeof(buf));
			if(n <= 0)
				break;
			if(write(pfd[1], buf, n) != n){
				close(fd);
				goto out;
			}
		}
		close(fd);
	}
	close(pfd[1]);
out:
	wm = wait();
	if(wm == nil){
		syslog(0, "runq", "wait: %r");
		logit("wait failed", name, av);
		return -1;
	}
	i = 0;
	if(wm->msg[0]){
		i = -1;
		syslog(0, "runq", "returnmail child: %s", wm->msg);
		logit("returnmail child failed", name, av);
	}
	free(wm);
	return i;
}

/*
 *  print a warning and continue
 */
void
warning(char *f, void *a)
{
	char err[65];
	char buf[256];

	rerrstr(err, sizeof(err));
	snprint(buf, sizeof(buf), f, a);
	fprint(2, "runq: %s: %s\n", buf, err);
}

/*
 *  print an error and die
 */
void
error(char *f, void *a)
{
	char err[Errlen];
	char buf[256];

	rerrstr(err, sizeof(err));
	snprint(buf, sizeof(buf), f, a);
	fprint(2, "runq: %s: %s\n", buf, err);
	exits(buf);
}

void
logit(char *msg, char *file, char **av)
{
	int n, m;
	char buf[256];

	n = snprint(buf, sizeof(buf), "%s/%s: %s", curdir, file, msg);
	for(; *av; av++){
		m = strlen(*av);
		if(n + m + 4 > sizeof(buf))
			break;
		sprint(buf + n, " '%s'", *av);
		n += m + 3;
	}
	syslog(0, runqlog, "%s", buf);
}

char *loadfile = ".runqload";

/*
 *  load balancing
 */
void
doload(int start)
{
	int fd;
	char buf[32];
	int i, n;
	Mlock *l;
	Dir *d;

	if(load <= 0)
		return;

	if(chdir(root) < 0){
		load = 0;
		return;
	}

	l = syslock(loadfile);
	fd = open(loadfile, ORDWR);
	if(fd < 0){
		fd = create(loadfile, 0666, ORDWR);
		if(fd < 0){
			load = 0;
			sysunlock(l);
			return;
		}
	}

	/* get current load */
	i = 0;
	n = read(fd, buf, sizeof(buf)-1);
	if(n >= 0){
		buf[n] = 0;
		i = atoi(buf);
	}
	if(i < 0)
		i = 0;

	/* ignore load if file hasn't been changed in 30 minutes */
	d = dirfstat(fd);
	if(d != nil){
		if(d->mtime + 30*60 < time(0))
			i = 0;
		free(d);
	}

	/* if load already too high, give up */
	if(start && i >= load){
		sysunlock(l);
		exits(0);
	}

	/* increment/decrement load */
	if(start)
		i++;
	else
		i--;
	seek(fd, 0, 0);
	fprint(fd, "%d\n", i);
	sysunlock(l);
	close(fd);
}
