#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>

/*
 * big-endian short
 */
u16int
beswap2(u16int s)
{
	uchar *p;

	p = (uchar*)&s;
	return (p[0]<<8) | p[1];
}

/*
 * big-endian long
 */
u32int
beswap4(u32int l)
{
	uchar *p;

	p = (uchar*)&l;
	return (p[0]<<24) | (p[1]<<16) | (p[2]<<8) | p[3];
}

/*
 * big-endian vlong
 */
u64int
beswap8(u64int v)
{
	uchar *p;

	p = (uchar*)&v;
	return ((u64int)p[0]<<56) | ((u64int)p[1]<<48) | ((u64int)p[2]<<40)
				 | ((u64int)p[3]<<32) | ((u64int)p[4]<<24)
				 | ((u64int)p[5]<<16) | ((u64int)p[6]<<8)
				 | (u64int)p[7];
}

/*
 * little-endian short
 */
u16int
leswap2(u16int s)
{
	uchar *p;

	p = (uchar*)&s;
	return (p[1]<<8) | p[0];
}

/*
 * little-endian long
 */
u32int
leswap4(u32int l)
{
	uchar *p;

	p = (uchar*)&l;
	return (p[3]<<24) | (p[2]<<16) | (p[1]<<8) | p[0];
}

/*
 * little-endian vlong
 */
u64int
leswap8(u64int v)
{
	uchar *p;

	p = (uchar*)&v;
	return ((u64int)p[7]<<56) | ((u64int)p[6]<<48) | ((u64int)p[5]<<40)
				 | ((u64int)p[4]<<32) | ((u64int)p[3]<<24)
				 | ((u64int)p[2]<<16) | ((u64int)p[1]<<8)
				 | (u64int)p[0];
}

u16int
leload2(uchar *b)
{
	return b[0] | (b[1]<<8);
}

u32int
leload4(uchar *b)
{
	return b[0] | (b[1]<<8) | (b[2]<<16) | (b[3]<<24);
}

u64int
leload8(uchar *b)
{
	return leload4(b) | ((uvlong)leload4(b+4) << 32);
}

u16int
beload2(uchar *b)
{
	return (b[0]<<8) | b[1];
}

u32int
beload4(uchar *b)
{
	return (b[0]<<24) | (b[1]<<16) | (b[2]<<8) | b[3];
}

u64int
beload8(uchar *b)
{
	return ((uvlong)beload4(b) << 32) | beload4(b+4);
}
