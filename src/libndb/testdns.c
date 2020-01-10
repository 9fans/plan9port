#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ndb.h>

void
main(int argc, char **argv)
{
	Ndbtuple *t, *t0;

	ARGBEGIN{
	default:
		goto usage;
	}ARGEND

	if(argc != 2){
	usage:
		fprint(2, "usage: testdns name val\n");
		exits("usage");
	}

	quotefmtinstall();
	if((t = dnsquery(nil, argv[0], argv[1])) == nil)
		sysfatal("dnsquery: %r");

	for(t0=t; t; t=t->entry){
		print("%s=%q ", t->attr, t->val);
		if(t->line == t0){
			print("\n");
			t0 = t->entry;
		}
	}
	exits(0);
}
