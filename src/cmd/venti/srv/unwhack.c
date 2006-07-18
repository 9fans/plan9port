#include "stdinc.h"
#include "whack.h"

enum
{
	DMaxFastLen	= 7,
	DBigLenCode	= 0x3c,		/* minimum code for large lenth encoding */
	DBigLenBits	= 6,
	DBigLenBase	= 1		/* starting items to encode for big lens */
};

static uchar lenval[1 << (DBigLenBits - 1)] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	3, 3, 3, 3, 3, 3, 3, 3,
	4, 4, 4, 4,
	5,
	6,
	255,
	255
};

static uchar lenbits[] =
{
	0, 0, 0,
	2, 3, 5, 5,
};

static uchar offbits[16] =
{
	5, 5, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 12, 13
};

static ushort offbase[16] =
{
	0, 0x20,
	0x40, 0x60,
	0x80, 0xc0,
	0x100, 0x180,
	0x200, 0x300,
	0x400, 0x600,
	0x800, 0xc00,
	0x1000,
	0x2000
};

void
unwhackinit(Unwhack *uw)
{
	uw->err[0] = '\0';
}

int
unwhack(Unwhack *uw, uchar *dst, int ndst, uchar *src, int nsrc)
{
	uchar *s, *d, *dmax, *smax, lit;
	ulong uwbits, lithist;
	int i, off, len, bits, use, code, uwnbits, overbits;

	d = dst;
	dmax = d + ndst;

	smax = src + nsrc;
	uwnbits = 0;
	uwbits = 0;
	overbits = 0;
	lithist = ~0;
	while(src < smax || uwnbits - overbits >= MinDecode){
		while(uwnbits <= 24){
			uwbits <<= 8;
			if(src < smax)
				uwbits |= *src++;
			else
				overbits += 8;
			uwnbits += 8;
		}

		/*
		 * literal
		 */
		len = lenval[(uwbits >> (uwnbits - 5)) & 0x1f];
		if(len == 0){
			if(lithist & 0xf){
				uwnbits -= 9;
				lit = (uwbits >> uwnbits) & 0xff;
				lit &= 255;
			}else{
				uwnbits -= 8;
				lit = (uwbits >> uwnbits) & 0x7f;
				if(lit < 32){
					if(lit < 24){
						uwnbits -= 2;
						lit = (lit << 2) | ((uwbits >> uwnbits) & 3);
					}else{
						uwnbits -= 3;
						lit = (lit << 3) | ((uwbits >> uwnbits) & 7);
					}
					lit = (lit - 64) & 0xff;
				}
			}
			if(d >= dmax){
				snprint(uw->err, WhackErrLen, "too much output");
				return -1;
			}
			*d++ = lit;
			lithist = (lithist << 1) | (lit < 32) | (lit > 127);
			continue;
		}

		/*
		 * length
		 */
		if(len < 255)
			uwnbits -= lenbits[len];
		else{
			uwnbits -= DBigLenBits;
			code = ((uwbits >> uwnbits) & ((1 << DBigLenBits) - 1)) - DBigLenCode;
			len = DMaxFastLen;
			use = DBigLenBase;
			bits = (DBigLenBits & 1) ^ 1;
			while(code >= use){
				len += use;
				code -= use;
				code <<= 1;
				uwnbits--;
				if(uwnbits < 0){
					snprint(uw->err, WhackErrLen, "len out of range");
					return -1;
				}
				code |= (uwbits >> uwnbits) & 1;
				use <<= bits;
				bits ^= 1;
			}
			len += code;

			while(uwnbits <= 24){
				uwbits <<= 8;
				if(src < smax)
					uwbits |= *src++;
				else
					overbits += 8;
				uwnbits += 8;
			}
		}

		/*
		 * offset
		 */
		uwnbits -= 4;
		bits = (uwbits >> uwnbits) & 0xf;
		off = offbase[bits];
		bits = offbits[bits];

		uwnbits -= bits;
		off |= (uwbits >> uwnbits) & ((1 << bits) - 1);
		off++;

		if(off > d - dst){
			snprint(uw->err, WhackErrLen, "offset out of range: off=%d d=%ld len=%d nbits=%d", off, d - dst, len, uwnbits);
			return -1;
		}
		if(d + len > dmax){
			snprint(uw->err, WhackErrLen, "len out of range");
			return -1;
		}
		s = d - off;
		for(i = 0; i < len; i++)
			d[i] = s[i];
		d += len;
	}
	if(uwnbits < overbits){
		snprint(uw->err, WhackErrLen, "compressed data overrun");
		return -1;
	}

	len = d - dst;

	return len;
}
