#include "lib9.h"
#include <regexp9.h>


main(int ac, char **av)
{
	Resub rs[10];
	Reprog *p;
	char *s;
	int i;

	p = regcomp("[^a-z]");
	s = "\n";
	if(regexec(p, s, rs, 10))
		print("%s %lux %lux %lux\n", s, s, rs[0].sp, rs[0].ep);
	s = "0";
	if(regexec(p, s, rs, 10))
		print("%s %lux %lux %lux\n", s, s, rs[0].sp, rs[0].ep);
	exit(0);
}
