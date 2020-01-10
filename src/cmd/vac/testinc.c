#include "stdinc.h"
#include "vac.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

void
threadmain(int argc, char **argv)
{
	Biobuf b;
	char *p;

	ARGBEGIN{
	default:
		goto usage;
	}ARGEND

	if(argc != 1){
	usage:
		fprint(2, "usage: testinc includefile\n");
		threadexitsall("usage");
	}

	loadexcludefile(argv[0]);
	Binit(&b, 0, OREAD);
	while((p = Brdline(&b, '\n')) != nil){
		p[Blinelen(&b)-1] = 0;
		print("%d %s\n", includefile(p), p);
	}
	threadexitsall(0);
}
