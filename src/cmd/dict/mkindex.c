#include <u.h>
#include <libc.h>
#include <bio.h>
#include "dict.h"

/*
 * Use this to start making an index for a new dictionary.
 * Get the dictionary-specific nextoff and printentry(_,'h')
 * commands working, add a record to the dicts[] array below,
 * and run this program to get a list of offset,headword
 * pairs
 */
Biobuf	boutbuf;
Biobuf	*bdict;
Biobuf	*bout = &boutbuf;
int	linelen;
int	breaklen = 2000;
int	outinhibit;
int	debug;

Dict	*dict;	/* current dictionary */

Entry	getentry(long);

void
main(int argc, char **argv)
{
	int i;
	long a, ae;
	char *p;
	Entry e;

	Binit(&boutbuf, 1, OWRITE);
	dict = &dicts[0];
	ARGBEGIN {
		case 'd':
			p = ARGF();
			dict = 0;
			if(p) {
				for(i=0; dicts[i].name; i++)
					if(strcmp(p, dicts[i].name)==0) {
						dict = &dicts[i];
						break;
					}
			}
			if(!dict) {
				err("unknown dictionary: %s", p);
				exits("nodict");
			}
			break;
		case 'D':
			debug++;
			break;
	ARGEND }
	USED(argc,argv);
	bdict = Bopen(dict->path, OREAD);
	ae = Bseek(bdict, 0, 2);
	if(!bdict) {
		err("can't open dictionary %s", dict->path);
		exits("nodict");
	}
	for(a = 0; a < ae; a = (*dict->nextoff)(a+1)) {
		linelen = 0;
		e = getentry(a);
		Bprint(bout, "%ld\t", a);
		linelen = 4;	/* only has to be approx right */
		(*dict->printentry)(e, 'h');
	}
	exits(0);
}

Entry
getentry(long b)
{
	long e, n, dtop;
	static Entry ans;
	static int anslen = 0;

	e = (*dict->nextoff)(b+1);
	ans.doff = b;
	if(e < 0) {
		dtop = Bseek(bdict, 0L, 2);
		if(b < dtop) {
			e = dtop;
		} else {
			err("couldn't seek to entry");
			ans.start = 0;
			ans.end = 0;
		}
	}
	n = e-b;
	if(n) {
		if(n > anslen) {
			ans.start = realloc(ans.start, n);
			if(!ans.start) {
				err("out of memory");
				exits("nomem");
			}
			anslen = n;
		}
		Bseek(bdict, b, 0);
		n = Bread(bdict, ans.start, n);
		ans.end = ans.start + n;
	}
	return ans;
}
