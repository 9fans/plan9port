#include "std.h"
#include "dat.h"

Proto *prototab[] = {
	&apop,
	&cram,
	&p9any,
	&p9sk1,
	&p9sk2,
	nil,
};

Proto*
protolookup(char *name)
{
	int i;

	for(i=0; prototab[i]; i++)
		if(strcmp(prototab[i]->name, name) == 0)
			return prototab[i];
	return nil;
}
