#include "os.h"
#include <mp.h>
#include "dat.h"

int
mpveccmp(mpdigit *a, int alen, mpdigit *b, int blen)
{
	mpdigit x;

	while(alen > blen)
		if(a[--alen] != 0)
			return 1;
	while(blen > alen)
		if(b[--blen] != 0)
			return -1;
	while(alen > 0){
		--alen;
		x = a[alen] - b[alen];
		if(x == 0)
			continue;
		if(x > a[alen])
			return -1;
		else
			return 1;
	}
	return 0;
}
