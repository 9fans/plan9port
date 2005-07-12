#include "stdinc.h"
#include "whack.h"

typedef struct Huff	Huff;
int compressblocks = 1;

enum
{
	MaxFastLen	= 9,
	BigLenCode	= 0x1f4,	/* minimum code for large lenth encoding */
	BigLenBits	= 9,
	BigLenBase	= 4,		/* starting items to encode for big lens */

	MinOffBits	= 6,
	MaxOffBits	= MinOffBits + 8,

	MaxLen		= 2051		/* max. length encodable in 24 bits */
};

enum
{
	StatBytes,
	StatOutBytes,
	StatLits,
	StatMatches,
	StatLitBits,
	StatOffBits,
	StatLenBits,

	MaxStat
};

struct Huff
{
	short	bits;				/* length of the code */
	ulong	encode;				/* the code */
};

static	Huff	lentab[MaxFastLen] =
{
	{2,	0x2},		/* 10 */
	{3,	0x6},		/* 110 */
	{5,	0x1c},		/* 11100 */
	{5,	0x1d},		/* 11101 */
	{6,	0x3c},		/* 111100 */
	{7,	0x7a},		/* 1111010 */
	{7,	0x7b},		/* 1111011 */
	{8,	0xf8},		/* 11111000 */
	{8,	0xf9},		/* 11111001 */
};

static int	thwmaxcheck;

void
whackinit(Whack *tw, int level)
{
	thwmaxcheck = (1 << level);
	thwmaxcheck -= thwmaxcheck >> 2;
	if(thwmaxcheck < 2)
		thwmaxcheck = 2;
	else if(thwmaxcheck > 1024)
		thwmaxcheck = 1024;
	memset(tw, 0, sizeof *tw);
	tw->begin = 2 * WhackMaxOff;
}

/*
 * find a string in the dictionary
 */
static int
whackmatch(Whack *b, uchar **ss, uchar *esrc, ulong h, ulong now)
{
	ushort then, off, last;
	int bestoff, bestlen, check;
	uchar *s, *t;

	s = *ss;
	if(esrc < s + MinMatch)
		return -1;
	if(s + MaxLen < esrc)
		esrc = s + MaxLen;

	bestoff = 0;
	bestlen = 0;
	check = thwmaxcheck;
	last = 0;
	for(then = b->hash[h]; check-- > 0; then = b->next[then & (WhackMaxOff - 1)]){
		off = now - then;
		if(off <= last || off > WhackMaxOff)
			break;

		/*
		 * don't need to check for the end because
		 * 1) s too close check above
		 */
		t = s - off;
		if(s[0] == t[0] && s[1] == t[1] && s[2] == t[2]){
			if(!bestlen || esrc - s > bestlen && s[bestlen] == t[bestlen]){
				t += 3;
				for(s += 3; s < esrc; s++){
					if(*s != *t)
						break;
					t++;
				}
				if(s - *ss > bestlen){
					bestlen = s - *ss;
					bestoff = off;
					if(bestlen > thwmaxcheck)
						break;
				}
			}
		}
		s = *ss;
		last = off;
	}
	*ss += bestlen;
	return bestoff;
}

/*
 * knuth vol. 3 multiplicative hashing
 * each byte x chosen according to rules
 * 1/4 < x < 3/10, 1/3 x < < 3/7, 4/7 < x < 2/3, 7/10 < x < 3/4
 * with reasonable spread between the bytes & their complements
 *
 * the 3 byte value appears to be as almost good as the 4 byte value,
 * and might be faster on some machines
 */
/*
#define hashit(c)	((((ulong)(c) * 0x6b43a9) >> (24 - HashLog)) & HashMask)
*/
#define hashit(c)	(((((ulong)(c) & 0xffffff) * 0x6b43a9b5) >> (32 - HashLog)) & HashMask)

/*
 * lz77 compression with single lookup in a hash table for each block
 */
int
whack(Whack *w, uchar *dst, uchar *src, int n, ulong stats[WhackStats])
{
	uchar *s, *ss, *sss, *esrc, *half, *wdst, *wdmax;
	ulong cont, code, wbits;
	ushort now;
	int toff, lithist, h, len, bits, use, wnbits, lits, matches, offbits, lenbits;

	if(!compressblocks || n < MinMatch)
		return -1;

	wdst = dst;
	wdmax = dst + n;

	now = w->begin;
	s = src;
	w->data = s;

	cont = (s[0] << 16) | (s[1] << 8) | s[2];

	esrc = s + n;
	half = s + (n >> 1);
	wnbits = 0;
	wbits = 0;
	lits = 0;
	matches = 0;
	offbits = 0;
	lenbits = 0;
	lithist = ~0;
	while(s < esrc){
		h = hashit(cont);

		sss = s;
		toff = whackmatch(w, &sss, esrc, h, now);
		ss = sss;

		len = ss - s;
		for(; wnbits >= 8; wnbits -= 8){
			if(wdst >= wdmax){
				w->begin = now;
				return -1;
			}
			*wdst++ = wbits >> (wnbits - 8);
		}
		if(len < MinMatch){
			toff = *s;
			lithist = (lithist << 1) | toff < 32 | toff > 127;
			if(lithist & 0x1e){
				wbits = (wbits << 9) | toff;
				wnbits += 9;
			}else if(lithist & 1){
				toff = (toff + 64) & 0xff;
				if(toff < 96){
					wbits = (wbits << 10) | toff;
					wnbits += 10;
				}else{
					wbits = (wbits << 11) | toff;
					wnbits += 11;
				}
			}else{
				wbits = (wbits << 8) | toff;
				wnbits += 8;
			}
			lits++;

			/*
			 * speed hack
			 * check for compression progress, bail if none achieved
			 */
			if(s > half){
				if(4 * (s - src) < 5 * lits){
					w->begin = now;
					return -1;
				}
				half = esrc;
			}

			if(s + MinMatch <= esrc){
				w->next[now & (WhackMaxOff - 1)] = w->hash[h];
				w->hash[h] = now;
				if(s + MinMatch < esrc)
					cont = (cont << 8) | s[MinMatch];
			}
			now++;
			s++;
			continue;
		}

		matches++;

		/*
		 * length of match
		 */
		if(len > MaxLen){
			len = MaxLen;
			ss = s + len;
		}
		len -= MinMatch;
		if(len < MaxFastLen){
			bits = lentab[len].bits;
			wbits = (wbits << bits) | lentab[len].encode;
			wnbits += bits;
			lenbits += bits;
		}else{
			code = BigLenCode;
			bits = BigLenBits;
			use = BigLenBase;
			len -= MaxFastLen;
			while(len >= use){
				len -= use;
				code = (code + use) << 1;
				use <<= (bits & 1) ^ 1;
				bits++;
			}

			wbits = (wbits << bits) | (code + len);
			wnbits += bits;
			lenbits += bits;

			for(; wnbits >= 8; wnbits -= 8){
				if(wdst >= wdmax){
					w->begin = now;
					return -1;
				}
				*wdst++ = wbits >> (wnbits - 8);
			}
		}

		/*
		 * offset in history
		 */
		toff--;
		for(bits = MinOffBits; toff >= (1 << bits); bits++)
			;
		if(bits < MaxOffBits-1){
			wbits = (wbits << 3) | (bits - MinOffBits);
			if(bits != MinOffBits)
				bits--;
			wnbits += bits + 3;
			offbits += bits + 3;
		}else{
			wbits = (wbits << 4) | 0xe | (bits - (MaxOffBits-1));
			bits--;
			wnbits += bits + 4;
			offbits += bits + 4;
		}
		wbits = (wbits << bits) | toff & ((1 << bits) - 1);

		for(; s != ss; s++){
			if(s + MinMatch <= esrc){
				h = hashit(cont);
				w->next[now & (WhackMaxOff - 1)] = w->hash[h];
				w->hash[h] = now;
				if(s + MinMatch < esrc)
					cont = (cont << 8) | s[MinMatch];
			}
			now++;
		}
	}

	w->begin = now;

	stats[StatBytes] += esrc - src;
	stats[StatLits] += lits;
	stats[StatMatches] += matches;
	stats[StatLitBits] += (wdst - (dst + 2)) * 8 + wnbits - offbits - lenbits;
	stats[StatOffBits] += offbits;
	stats[StatLenBits] += lenbits;

	if(wnbits & 7){
		wbits <<= 8 - (wnbits & 7);
		wnbits += 8 - (wnbits & 7);
	}
	for(; wnbits >= 8; wnbits -= 8){
		if(wdst >= wdmax)
			return -1;
		*wdst++ = wbits >> (wnbits - 8);
	}

	stats[StatOutBytes] += wdst - dst;

	return wdst - dst;
}

int
whackblock(uchar *dst, uchar *src, int ssize)
{
	Whack w;
	ulong stats[MaxStat];
	int r;

	whackinit(&w, 6);
	r = whack(&w, dst, src, ssize, stats);
	return r;
}
