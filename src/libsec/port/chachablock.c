#include "os.h"

#define ROTATE(v,c) ((u32int)((v) << (c)) | ((v) >> (32 - (c))))

#define QUARTERROUND(ia,ib,ic,id) { \
	u32int a, b, c, d, t; \
	a = x[ia]; b = x[ib]; c = x[ic]; d = x[id]; \
	a += b; t = d^a; d = ROTATE(t,16); \
	c += d; t = b^c; b = ROTATE(t,12); \
	a += b; t = d^a; d = ROTATE(t,8); \
	c += d; t = b^c; b = ROTATE(t,7); \
	x[ia] = a; x[ib] = b; x[ic] = c; x[id] = d; \
}

void
_chachablock(u32int x[16], int rounds)
{
	for(; rounds > 0; rounds -= 2){
		QUARTERROUND(0, 4, 8, 12)
		QUARTERROUND(1, 5, 9, 13)
		QUARTERROUND(2, 6, 10, 14)
		QUARTERROUND(3, 7, 11, 15)

		QUARTERROUND(0, 5, 10, 15)
		QUARTERROUND(1, 6, 11, 12)
		QUARTERROUND(2, 7, 8, 13)
		QUARTERROUND(3, 4, 9, 14)
	}
}
