#include "stdinc.h"
#include "vac.h"
#include "dat.h"
#include "fns.h"

int
vtGetUint16(uchar *p)
{
	return (p[0]<<8)|p[1];
}

ulong
vtGetUint32(uchar *p)
{
	return (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
}

uvlong
vtGetUint48(uchar *p)
{
	return ((uvlong)p[0]<<40)|((uvlong)p[1]<<32)|
		(p[2]<<24)|(p[3]<<16)|(p[4]<<8)|p[5];
}

uvlong
vtGetUint64(uchar *p)
{
	return ((uvlong)p[0]<<56)|((uvlong)p[1]<<48)|((uvlong)p[2]<<40)|
		((uvlong)p[3]<<32)|(p[4]<<24)|(p[5]<<16)|(p[6]<<8)|p[7];
}


void
vtPutUint16(uchar *p, int x)
{
	p[0] = x>>8;
	p[1] = x;
}

void
vtPutUint32(uchar *p, ulong x)
{
	p[0] = x>>24;
	p[1] = x>>16;
	p[2] = x>>8;
	p[3] = x;
}

void
vtPutUint48(uchar *p, uvlong x)
{
	p[0] = x>>40;
	p[1] = x>>32;
	p[2] = x>>24;
	p[3] = x>>16;
	p[4] = x>>8;
	p[5] = x;
}

void
vtPutUint64(uchar *p, uvlong x)
{
	p[0] = x>>56;
	p[1] = x>>48;
	p[2] = x>>40;
	p[3] = x>>32;
	p[4] = x>>24;
	p[5] = x>>16;
	p[6] = x>>8;
	p[7] = x;
}
