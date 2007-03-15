#include <u.h>
#include <libc.h>
#include <bio.h>

#define openlog auxclog_openlog	/* libc symbol on OS X */

char *argv0;

int
openlog(char *name)
{
	int fd;

	fd = open(name, OWRITE);
	if(fd < 0)
		fd = create(name, OWRITE, DMAPPEND|0666);
	if(fd < 0){
		fprint(2, "%s: can't open %s: %r\n", argv0, name);
		return -1;
	}
	seek(fd, 0, 2);
	return fd;
}

void
main(int argc, char **argv)
{
	Biobuf in;
	int fd;
	char *p, *t;
	char buf[8192];

	argv0 = argv[0];
	if(argc !=  3){
		fprint(2, "usage: %s console logfile \n", argv0);
		exits("usage");
	}

	if(strcmp(argv[1], "-") == 0)
		fd = 0;
	else
		fd = open(argv[1], OREAD);
	if(fd < 0){
		fprint(2, "%s: can't open %s: %r\n", argv0, argv[1]);
		exits("open");
	}
	Binit(&in, fd, OREAD);

	fd = openlog(argv[2]);

	for(;;){
		if(p = Brdline(&in, '\n')){
			p[Blinelen(&in)-1] = 0;
			t = ctime(time(0));
			t[19] = 0;
			if(fprint(fd, "%s: %s\n", t, p) < 0){
				close(fd);
				fd = openlog(argv[2]);
				fprint(fd, "%s: %s\n", t, p);
			}
		} else if(Blinelen(&in) == 0)	/* true eof */
			break;
		else {
			Bread(&in, buf, sizeof buf);
		}
	}
	exits(0);
}
