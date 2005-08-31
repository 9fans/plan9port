#include <u.h>
#include <libc.h>

/* MAXHOSTNAMELEN is in sys/param.h */
#define MAXHOSTNAMELEN	64

char lockstring[MAXHOSTNAMELEN+8];

void
main(int argc, char *argv[]) {
	char *lockfile;
	int fd, ppid, ssize;
	struct Dir *statbuf;

	if (argc != 4) {
		fprint(2, "usage: LOCK lockfile hostname ppid\n");
		exits("lock failed on usage");
	}
	lockfile = argv[1];
	if ((fd=create(lockfile, OLOCK|ORDWR, 0666)) < 0) {
		exits("lock failed on create");
	}
	ppid = atoi(argv[3]);
	ssize = sprint(lockstring, "%s %s\n", argv[2], argv[3]);
	if (write(fd, lockstring, ssize) != ssize) {
		fprint(2, "LOCK:write(): %r\n");
		exits("lock failed on write to lockfile");
	}

	switch(fork()) {
	default:
		exits("");
	case 0:
		break;
	case -1:
		fprint(2, "LOCK:fork(): %r\n");
		exits("lock failed on fork");
	}

	for(;;) {
		statbuf = dirfstat(fd);
		if(statbuf == nil)
			break;
		if (statbuf->length == 0){
			free(statbuf);
			break;
		}
		free(statbuf);
		if (write(fd, "", 0) < 0)
			break;
		sleep(3000);
	}

	close(fd);
	postnote(PNGROUP, ppid, "kill");
	exits("");
}
