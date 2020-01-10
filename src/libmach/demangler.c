#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>

void
main(void)
{
	Biobuf b, bout;
	char *p, *s;
	char buf[100000];

	Binit(&b, 0, OREAD);
	Binit(&bout, 1, OWRITE);

	while((p = Brdline(&b, '\n')) != nil){
		p[Blinelen(&b)-1] = 0;
		werrstr("no error");
		s = demanglegcc2(p, buf);
		if(s == p)
			Bprint(&bout, "# %s (%r)\n", p);
		else
			Bprint(&bout, "%s\t%s\n", p, s);
	}
	Bflush(&bout);
	exits(0);
}
