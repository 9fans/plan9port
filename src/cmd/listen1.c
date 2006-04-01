#include <u.h>
#include <libc.h>
#include <auth.h>

int verbose;
int trusted;

void
usage(void)
{
	fprint(2, "usage: listen1 [-v] address cmd args...\n");
	exits("usage");
}

char*
remoteaddr(char *dir)
{
	static char buf[128];
	char *p;
	int n, fd;

	snprint(buf, sizeof buf, "%s/remote", dir);
	fd = open(buf, OREAD);
	if(fd < 0)
		return "";
	n = read(fd, buf, sizeof(buf));
	close(fd);
	if(n > 0){
		buf[n] = 0;
		p = strchr(buf, '!');
		if(p)
			*p = 0;
		return buf;
	}
	return "";
}

void
main(int argc, char **argv)
{
	char dir[40], ndir[40];
	int ctl, nctl, fd;

	ARGBEGIN{
	default:
		usage();
	case 't':
		trusted = 1;
		break;
	case 'v':
		verbose = 1;
		break;
	}ARGEND

	if(argc < 2)
		usage();

	if(!verbose){
		close(1);
		fd = open("/dev/null", OWRITE);
		if(fd != 1){
			dup(fd, 1);
			close(fd);
		}
	}

	print("listen started\n");
	ctl = announce(argv[0], dir);
	if(ctl < 0)
		sysfatal("announce %s: %r", argv[0]);

	for(;;){
		nctl = listen(dir, ndir);
		if(nctl < 0)
			sysfatal("listen %s: %r", argv[0]);

		switch(rfork(RFFDG|RFPROC|RFNOWAIT|RFNOTEG)){
		case -1:
			close(nctl);
			continue;
		case 0:
			fd = accept(nctl, ndir);
			if(fd < 0){
				fprint(2, "accept %s: can't open  %s/data: %r", argv[0], ndir);
				_exits(0);
			}
			print("incoming call for %s from %s in %s\n", argv[0], remoteaddr(ndir), ndir);
			close(ctl);
			close(nctl);
			/*putenv("net", ndir); */
			/*sprint(data, "%s/data", ndir); */
			/*bind(data, "/dev/cons", MREPL); */
			dup(fd, 0);
			dup(fd, 1);
			dup(fd, 2);
			close(fd);
			exec(argv[1], argv+1);
			fprint(2, "exec: %r");
			exits(nil);
		default:
			close(nctl);
			break;
		}
	}
}
