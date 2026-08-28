#include "os.h"
#include <mp.h>
#include "dat.h"

int
mpvectscmp(mpdigit *a, int alen, mpdigit *b, int blen)
{
	mpdigit x, y, z, v;
	int m, p;

	if(alen > blen){
		v = 0;
		while(alen > blen)
			v |= a[--alen];
		m = p = ((-v^v)|v)>>(Dbits-1);
	} else if(blen > alen){
		v = 0;
		while(blen > alen)
			v |= b[--blen];
		m = ((-v^v)|v)>>(Dbits-1);
		p = m^1;
	} else
		m = p = 0;
	while(alen-- > 0){
		x = a[alen];
		y = b[alen];
		z = x - y;
		x = ~x;
		v = (((-z^z)|z)>>(Dbits-1)) & ~m;
		p = ((~((x&y)|(x&z)|(y&z))>>(Dbits-1)) & v) | (p & ~v);
		m |= v;
	}
	return (p-m) | m;
}
