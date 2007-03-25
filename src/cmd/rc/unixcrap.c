#include <u.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <errno.h>
#include <fcntl.h>
#include <libc.h>
#include "rc.h"
#include "exec.h"
#include "io.h"
#include "fns.h"
#include "getflags.h"

extern char **mkargv(word*);
extern int mapfd(int);

static char *eargs = "cdflmnstuv";
static int rlx[] = {
	RLIMIT_CORE,
	RLIMIT_DATA,
	RLIMIT_FSIZE,
#ifdef RLIMIT_MEMLOCK
	RLIMIT_MEMLOCK,
#else
	0,
#endif
#ifdef RLIMIT_RSS
	RLIMIT_RSS,
#else
	0,
#endif
	RLIMIT_NOFILE,
	RLIMIT_STACK,
	RLIMIT_CPU,
#ifdef RLIMIT_NPROC
	RLIMIT_NPROC,
#else
	0,
#endif
#ifdef RLIMIT_RSS
	RLIMIT_RSS,
#else
	0,
#endif
};

static void
eusage(void)
{
	fprint(mapfd(2), "usage: ulimit [-SHa%s [limit]]\n", eargs);
}

#define Notset -4
#define Unlimited -3
#define Hard -2
#define Soft -1

void
execulimit(void)
{
	int fd, n, argc, sethard, setsoft, limit;
	int flag[256];
	char **argv, **oargv, *p;
	char *argv0;
	struct rlimit rl;

	argv0 = nil;
	setstatus("");
	oargv = mkargv(runq->argv->words);
	argv = oargv+1;
	for(argc=0; argv[argc]; argc++)
		;

	memset(flag, 0, sizeof flag);
	ARGBEGIN{
	default:
		if(strchr(eargs, ARGC()) == nil){
			eusage();
			return;
		}
	case 'S':
	case 'H':
	case 'a':
		flag[ARGC()] = 1;
		break;
	}ARGEND

	if(argc > 1){
		eusage();
		goto out;
	}

	fd = mapfd(1);

	sethard = 1;
	setsoft = 1;
	if(flag['S'] && flag['H'])
		;
	else if(flag['S'])
		sethard = 0;
	else if(flag['H'])
		setsoft = 0;

	limit = Notset;
	if(argc>0){
		if(strcmp(argv[0], "unlimited") == 0)
			limit = Unlimited;
		else if(strcmp(argv[0], "hard") == 0)
			limit = Hard;
		else if(strcmp(argv[0], "soft") == 0)
			limit = Soft;
		else if((limit = strtol(argv[0], &p, 0)) < 0 || *p != 0){
			eusage();
			goto out;
		}
	}
	if(flag['a']){
		for(p=eargs; *p; p++){
			getrlimit(rlx[p-eargs], &rl);
			n = flag['H'] ? rl.rlim_max : rl.rlim_cur;
			if(n == -1)
				fprint(fd, "ulimit -%c unlimited\n", *p);
			else
				fprint(fd, "ulimit -%c %d\n", *p, n);
		}
		goto out;
	}
	for(p=eargs; *p; p++){
		if(flag[(uchar)*p]){
			n = 0;
			getrlimit(rlx[p-eargs], &rl);
			switch(limit){
			case Notset:
				n = flag['H'] ? rl.rlim_max : rl.rlim_cur;
				if(n == -1)
					fprint(fd, "ulimit -%c unlimited\n", *p);
				else
					fprint(fd, "ulimit -%c %d\n", *p, n);
				break;
			case Hard:
				n = rl.rlim_max;
				goto set;
			case Soft:
				n = rl.rlim_cur;
				goto set;
			case Unlimited:
				n = -1;
				goto set;
			default:
				n = limit;
			set:
				if(setsoft)
					rl.rlim_cur = n;
				if(sethard)
					rl.rlim_max = n;
				if(setrlimit(rlx[p-eargs], &rl) < 0)
					fprint(mapfd(2), "setrlimit: %r\n");
			}
		}
	}

out:
	free(oargv);
	poplist();
	flush(err);
}

void
execumask(void)
{
	int n, argc;
	char **argv, **oargv, *p;
	char *argv0;

	argv0 = nil;
	setstatus("");
	oargv = mkargv(runq->argv->words);
	argv = oargv+1;
	for(argc=0; argv[argc]; argc++)
		;

	ARGBEGIN{
	default:
	usage:
		fprint(mapfd(2), "usage: umask [mode]\n");
		goto out;
	}ARGEND

	if(argc > 1)
		goto usage;

	if(argc == 1){
		n = strtol(argv[0], &p, 8);
		if(*p != 0 || p == argv[0])
			goto usage;
		umask(n);
		goto out;
	}

	n = umask(0);
	umask(n);
	if(n < 0){
		fprint(mapfd(2), "umask: %r\n");
		goto out;
	}

	fprint(mapfd(1), "umask %03o\n", n);

out:
	free(oargv);
	poplist();
	flush(err);
}

/*
 * Cope with non-blocking read.
 */
long
readnb(int fd, char *buf, long cnt)
{
	int n, didreset;
	int flgs;

	didreset = 0;
	while((n = read(fd, buf, cnt)) == -1)
		if(!didreset && errno == EAGAIN){
			if((flgs = fcntl(fd, F_GETFL, 0)) == -1)
				return -1;
			flgs &= ~O_NONBLOCK;
			if(fcntl(fd, F_SETFL, flgs) == -1)
				return -1;
			didreset = 1;
		}

	return n;
}
