#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>

/*
 * These routines assume that if the number is representable
 * in IEEE floating point, it will be representable in the native
 * double format.  Naive but workable, probably.
 */
int
ieeeftoa64(char *buf, uint n, u32int h, u32int l)
{
	double fr;
	int exp;

	if (n <= 0)
		return 0;


	if(h & (1UL<<31)){
		*buf++ = '-';
		h &= ~(1UL<<31);
	}else
		*buf++ = ' ';
	n--;
	if(l == 0 && h == 0)
		return snprint(buf, n, "0.");
	exp = (h>>20) & ((1L<<11)-1L);
	if(exp == 0)
		return snprint(buf, n, "DeN(%.8lux%.8lux)", h, l);
	if(exp == ((1L<<11)-1L)){
		if(l==0 && (h&((1L<<20)-1L)) == 0)
			return snprint(buf, n, "Inf");
		else
			return snprint(buf, n, "NaN(%.8lux%.8lux)", h&((1L<<20)-1L), l);
	}
	exp -= (1L<<10) - 2L;
	fr = l & ((1L<<16)-1L);
	fr /= 1L<<16;
	fr += (l>>16) & ((1L<<16)-1L);
	fr /= 1L<<16;
	fr += (h & (1L<<20)-1L) | (1L<<20);
	fr /= 1L<<21;
	fr = ldexp(fr, exp);
	return snprint(buf, n, "%.18g", fr);
}

int
ieeeftoa32(char *buf, uint n, u32int h)
{
	double fr;
	int exp;

	if (n <= 0)
		return 0;

	if(h & (1UL<<31)){
		*buf++ = '-';
		h &= ~(1UL<<31);
	}else
		*buf++ = ' ';
	n--;
	if(h == 0)
		return snprint(buf, n, "0.");
	exp = (h>>23) & ((1L<<8)-1L);
	if(exp == 0)
		return snprint(buf, n, "DeN(%.8lux)", h);
	if(exp == ((1L<<8)-1L)){
		if((h&((1L<<23)-1L)) == 0)
			return snprint(buf, n, "Inf");
		else
			return snprint(buf, n, "NaN(%.8lux)", h&((1L<<23)-1L));
	}
	exp -= (1L<<7) - 2L;
	fr = (h & ((1L<<23)-1L)) | (1L<<23);
	fr /= 1L<<24;
	fr = ldexp(fr, exp);
	return snprint(buf, n, "%.9g", fr);
}

int
beieeeftoa32(char *buf, uint n, void *s)
{
	return ieeeftoa32(buf, n, beswap4(*(u32int*)s));
}

int
beieeeftoa64(char *buf, uint n, void *s)
{
	return ieeeftoa64(buf, n, beswap4(*(u32int*)s), beswap4(((u32int*)(s))[1]));
}

int
leieeeftoa32(char *buf, uint n, void *s)
{
	return ieeeftoa32(buf, n, leswap4(*(u32int*)s));
}

int
leieeeftoa64(char *buf, uint n, void *s)
{
	return ieeeftoa64(buf, n, leswap4(((u32int*)(s))[1]), leswap4(*(u32int*)s));
}

/* packed in 12 bytes, with s[2]==s[3]==0; mantissa starts at s[4]*/
int
beieeeftoa80(char *buf, uint n, void *s)
{
	uchar *reg = (uchar*)s;
	int i;
	ulong x;
	uchar ieee[8+8];	/* room for slop */
	uchar *p, *q;

	memset(ieee, 0, sizeof(ieee));
	/* sign */
	if(reg[0] & 0x80)
		ieee[0] |= 0x80;

	/* exponent */
	x = ((reg[0]&0x7F)<<8) | reg[1];
	if(x == 0)		/* number is ±0 */
		goto done;
	if(x == 0x7FFF){
		if(memcmp(reg+4, ieee+1, 8) == 0){ /* infinity */
			x = 2047;
		}else{				/* NaN */
			x = 2047;
			ieee[7] = 0x1;		/* make sure */
		}
		ieee[0] |= x>>4;
		ieee[1] |= (x&0xF)<<4;
		goto done;
	}
	x -= 0x3FFF;		/* exponent bias */
	x += 1023;
	if(x >= (1<<11) || ((reg[4]&0x80)==0 && x!=0))
		return snprint(buf, n, "not in range");
	ieee[0] |= x>>4;
	ieee[1] |= (x&0xF)<<4;

	/* mantissa */
	p = reg+4;
	q = ieee+1;
	for(i=0; i<56; i+=8, p++, q++){	/* move one byte */
		x = (p[0]&0x7F) << 1;
		if(p[1] & 0x80)
			x |= 1;
		q[0] |= x>>4;
		q[1] |= (x&0xF)<<4;
	}
    done:
	return beieeeftoa64(buf, n, (void*)ieee);
}


int
leieeeftoa80(char *buf, uint n, void *s)
{
	int i;
	char *cp;
	char b[12];

	cp = (char*) s;
	for(i=0; i<12; i++)
		b[11-i] = *cp++;
	return beieeeftoa80(buf, n, b);
}
