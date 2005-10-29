#include <u.h>
#include <libc.h>
#include <bio.h>
#include "msgdb.h"

void
usage(void)
{
	fprint(2, "usage: msgdb [-c] file\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	int create = 0;
	Msgdb *db;
	char *tok, *p;
	long val;
	int input;
	Biobuf b;

	input = 0;
	ARGBEGIN{
	case 'c':
		create = 1;
		break;
	case 'i':
		input = 1;
		break;
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	if((db = mdopen(argv[0], create)) == nil)
		sysfatal("open db: %r");

	if(input){
		Binit(&b, 0, OREAD);
		while((tok = Brdline(&b, '\n')) != nil){
			tok[Blinelen(&b)-1] = '\0';
			p = strrchr(tok, ' ');
			if(p == nil)
				val = mdget(db, tok)+1;
			else{
				*p++ = 0;
				val = atoi(p);
			}
			mdput(db, tok, val);
		}
	}else{
		mdenum(db);
		Binit(&b, 1, OWRITE);
		while(mdnext(db, &tok, &val) >= 0)
			Bprint(&b, "%s %ld\n", tok, val);
		Bterm(&b);
	}
	mdclose(db);
	exits(nil);
}
