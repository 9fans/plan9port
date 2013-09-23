/*
 * Clumsy hack to take snapshots and dumps.
 */
#include <u.h>
#include <libc.h>

void
usage(void)
{
	fprint(2, "usage: fossil/dump [-i snap-interval] [-n name] fscons /n/fossil\n");
	exits("usage");
}

char*
snapnow(void)
{
	Tm t;
	static char buf[100];

	t = *localtime(time(0)-5*60*60);	/* take dumps at 5:00 am */

	sprint(buf, "archive/%d/%02d%02d", t.year+1900, t.mon+1, t.mday);
	return buf;
}

void
main(int argc, char **argv)
{
	int onlyarchive, cons, s;
	ulong t, i;
	char *name;

	name = "main";
	s = 0;
	onlyarchive = 0;
	i = 60*60;		/* one hour */
	ARGBEGIN{
	case 'i':
		i = atoi(EARGF(usage()));
		if(i == 0){
			onlyarchive = 1;
			i = 60*60;
		}
		break;
	case 'n':
		name = EARGF(usage());
		break;
	case 's':
		s = atoi(EARGF(usage()));
		break;
	}ARGEND

	if(argc != 2)
		usage();

	if((cons = open(argv[0], OWRITE)) < 0)
		sysfatal("open %s: %r", argv[0]);

	if(chdir(argv[1]) < 0)
		sysfatal("chdir %s: %r", argv[1]);

	rfork(RFNOTEG);
	switch(fork()){
	case -1:
		sysfatal("fork: %r");
	case 0:
		break;
	default:
		exits(0);
	}

	/*
	 * pause at boot time to let clock stabilize.
	 */
	if(s)
		sleep(s*1000);

	for(;;){
		if(access(snapnow(), AEXIST) < 0)
			fprint(cons, "\nfsys %s snap -a\n", name);
		t = time(0);
		sleep((i - t%i)*1000+200);
		if(!onlyarchive)
			fprint(cons, "\nfsys %s snap\n", name);
	}
}
