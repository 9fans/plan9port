#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>
#include "dat.h"

int verbose;

void
usage(void)
{
	fprint(2, "usage: acidtypes [-v] [-p prefix] executable...\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	int i, have;
	Fhdr *fp;
	Biobuf b;
	char err[ERRMAX];

	quotefmtinstall();
	fmtinstall('B', Bfmt);

	ARGBEGIN{
	case 'v':
		verbose = 1;
		break;
	case 'p':
		prefix = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	if(argc < 1)
		usage();

	Binit(&b, 1, OWRITE);
	for(i=0; i<argc; i++){
		Bprint(&b, "\n//\n// symbols for %s\n//\n\n", argv[i]);
		if((fp = crackhdr(argv[i], OREAD)) == nil){
			rerrstr(err, sizeof err);
			Bprint(&b, "// open %s: %s\n\n", argv[i], err);
			fprint(2, "open %s: %s\n", argv[i], err);
			continue;
		}
		have = 0;
		if(fp->dwarf){
			if(dwarf2acid(fp->dwarf, &b) < 0){
				rerrstr(err, sizeof err);
				Bprint(&b, "// dwarf2acid %s: %s\n\n", argv[i], err);
				fprint(2, "dwarf2acid %s: %s\n", argv[i], err);
			}
			have = 1;
		}
		if(fp->stabs.stabbase){
			if(stabs2acid(&fp->stabs, &b) < 0){
				rerrstr(err, sizeof err);
				Bprint(&b, "// dwarf2acid %s: %s\n\n", argv[i], err);
				fprint(2, "dwarf2acid %s: %s\n", argv[i], err);
			}
			have = 1;
		}

		if(!have){
			Bprint(&b, "// no debugging symbols in %s\n\n", argv[i]);
		/*	fprint(2, "no debugging symbols in %s\n", argv[i]); */
		}
		uncrackhdr(fp);
	}
	Bflush(&b);
	Bterm(&b);
	exits(0);
}
