#include "stdinc.h"

#define TWID64	((u64int)~(u64int)0)

u64int
unittoull(char *s)
{
	char *es;
	u64int n;

	if(s == nil)
		return TWID64;
	n = strtoul(s, &es, 0);
	if(*es == 'k' || *es == 'K'){
		n *= 1024;
		es++;
	}else if(*es == 'm' || *es == 'M'){
		n *= 1024*1024;
		es++;
	}else if(*es == 'g' || *es == 'G'){
		n *= 1024*1024*1024;
		es++;
	}else if(*es == 't' || *es == 'T'){
		n *= 1024*1024;
		n *= 1024*1024;
	}
	if(*es != '\0')
		return TWID64;
	return n;
}
