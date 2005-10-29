#include "common.h"
#include "dat.h"

int cflag;
int aflag;
int rflag;

int createpipeto(char *alfile, char *user, char *listname, int owner);

void
usage(void)
{
	fprint(2, "usage:\t%s -c listname\n", argv0);
	fprint(2, "\t%s -[ar] listname addr\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	char *listname, *addr;
	String *owner, *alfile;

	rfork(RFENVG|RFREND);

	ARGBEGIN{
	case 'c':
		cflag = 1;
		break;
	case 'r':
		rflag = 1;
		break;
	case 'a':
		aflag = 1;
		break;
	}ARGEND;

	if(aflag + rflag + cflag > 1){
		fprint(2, "%s: -a, -r, and -c are mutually exclusive\n", argv0);
		exits("usage");
	}

	if(argc < 1)
		usage();

	listname = argv[0];
	alfile = s_new();
	mboxpath("address-list", listname, alfile, 0);

	if(cflag){
		owner = s_copy(listname);
		s_append(owner, "-owner");
		if(creatembox(listname, nil) < 0)
			sysfatal("creating %s's mbox: %r", listname);
		if(creatembox(s_to_c(owner), nil) < 0)
			sysfatal("creating %s's mbox: %r", s_to_c(owner));
		if(createpipeto(s_to_c(alfile), listname, listname, 0) < 0)
			sysfatal("creating %s's pipeto: %r", s_to_c(owner));
		if(createpipeto(s_to_c(alfile), s_to_c(owner), listname, 1) < 0)
			sysfatal("creating %s's pipeto: %r", s_to_c(owner));
		writeaddr(s_to_c(alfile), "# mlmgr c flag", 0, listname);
	} else if(rflag){
		if(argc != 2)
			usage();
		addr = argv[1];
		writeaddr(s_to_c(alfile), "# mlmgr r flag", 0, listname);
		writeaddr(s_to_c(alfile), addr, 1, listname);
	} else if(aflag){
		if(argc != 2)
			usage();
		addr = argv[1];
		writeaddr(s_to_c(alfile), "# mlmgr a flag", 0, listname);
		writeaddr(s_to_c(alfile), addr, 0, listname);
	} else
		usage();
	exits(0);
}

int
createpipeto(char *alfile, char *user, char *listname, int owner)
{
	String *f;
	int fd;
	Dir *d;

	f = s_new();
	mboxpath("pipeto", user, f, 0);
	fprint(2, "creating new pipeto: %s\n", s_to_c(f));
	fd = create(s_to_c(f), OWRITE, 0775);
	if(fd < 0)
		return -1;
	d = dirfstat(fd);
	if(d == nil){
		fprint(fd, "Couldn't stat %s: %r\n", s_to_c(f));
		return -1;
	}
	d->mode |= 0775;
	if(dirfwstat(fd, d) < 0)
		fprint(fd, "Couldn't wstat %s: %r\n", s_to_c(f));
	free(d);

	fprint(fd, "#!/bin/rc\n");
	if(owner)
		fprint(fd, "/bin/upas/mlowner %s %s\n", alfile, listname);
	else
		fprint(fd, "/bin/upas/ml %s %s\n", alfile, user);
	close(fd);

	return 0;
}
