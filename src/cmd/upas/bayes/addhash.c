#include <u.h>
#include <libc.h>
#include <bio.h>
#include <regexp.h>
#include "hash.h"

Hash hash;

void
usage(void)
{
	fprint(2, "addhash [-o out] file scale [file scale]...\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	int i, fd, n;
	char err[ERRMAX], *out;
	Biobuf *b, bout;

	out = nil;
	ARGBEGIN{
	case 'o':
		out = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND;

	if(argc==0 || argc%2)
		usage();

	while(argc > 0){
		if((b = Bopenlock(argv[0], OREAD)) == nil)
			sysfatal("open %s: %r", argv[0]);
		n = atoi(argv[1]);
		if(n == 0)
			sysfatal("0 scale given");
		Breadhash(b, &hash, n);
		Bterm(b);
		argv += 2;
		argc -= 2;
	}

	fd = 1;
	if(out){
		for(i=0; i<120; i++){
			if((fd = create(out, OWRITE, 0666|DMEXCL)) >= 0)
				break;
			rerrstr(err, sizeof err);
			if(strstr(err, "file is locked")==nil && strstr(err, "exclusive lock")==nil)
				break;
			sleep(1000);
		}
		if(fd < 0)
			sysfatal("could not open %s: %r\n", out);
	}

	Binit(&bout, fd, OWRITE);
	Bwritehash(&bout, &hash);
	Bterm(&bout);
	exits(0);
}
