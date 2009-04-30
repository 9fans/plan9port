#include <u.h>
#include <libc.h>
#include	<bio.h>
#include "sky.h"

/*
 * dec varies from -89 to 89, inclusive.
 * ra varies depending on dec; each patch is about 1 square degree.
 *
 * Northern hemisphere (0<=dec<=89):
 *	from  0<=dec<=59, ra is every 4m,  360 values
 *	from 60<=dec<=69, ra is every 8m,  180 values
 *	from 70<=dec<=79, ra is every 12m, 120 values
 *	from 80<=dec<=84, ra is every 24m,  60 values
 *	at dec=85 and 86, ra is every 48m,  30 values
 *	at dec=87,        ra is every 60m,  24 values
 *	at dec=88,        ra is every 120m, 12 values
 *	at dec=89,        ra is 12h,	     1 value
 *
 * Total number of patches in northern hemisphere is therefore:
 * 	360*60+180*10+120*10+60*5+30*2+24*1+12*1+1 = 24997
 * Total number of patches is therefore
 *	2*24997-360 = 49634	(dec=0 has been counted twice)
 * (cf. 41253 square degrees in the sky)
 */

void
radec(int p, int *rah, int *ram, int *deg)
{
	*deg = (p&255)-90;
	p >>= 8;
	*rah = p/15;
	*ram = (p%15)*4;
	if(*deg<0)
		(*deg)++;
}

int32
patcha(Angle ra, Angle dec)
{
	ra = DEG(ra);
	dec = DEG(dec);
	if(dec >= 0)
		return patch(floor(ra/15), ((int)floor(ra*4))%60, floor(dec));
	dec = -dec;
	return patch(floor(ra/15), ((int)floor(ra*4))%60, -floor(dec));
}

#define round scatround

char round[91]={	/* extra 0 is to offset the array */
	/*  0 */    0,	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	/* 10 */	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	/* 20 */	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	/* 30 */	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	/* 40 */	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	/* 50 */	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	/* 60 */	 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
	/* 70 */	 3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
	/* 80 */	 6,  6,  6,  6,  6, 12, 12, 15, 30, -1,
	/* 90 */
};

int32
patch(int rah, int ram, int deg)
{
	int ra, dec;

	/*
	 * patches go from lower limit <= position < upper limit.
	 * therefore dec ' " can be ignored; always inc. dec degrees.
	 * the computed angle is then the upper limit (ignoring sign).
	 * when done, +ve values are shifted down so 90 (0 degrees) is a value;
	 */
	if(rah<0 || rah>=24 || ram<0 || abs(deg)>=90){
		fprint(2, "scat: patch: bad ra or dec %dh%dm %d\n", rah, ram, deg);
		abort();
	}
	if(deg < 0)
		deg--;
	else if(deg < 90)
		deg++;
	dec = deg+90;
	deg = abs(deg);
	if(deg<1 || deg>90){
		fprint(2, "scat: patch: panic %dh%dm %d\n", rah, ram, deg);
		abort();
	}
	if(deg == 90)
		ra = 180;
	else{
		ra = 15*rah+ram/4;
		ra -= ra%round[deg];
	}
	/* close the hole at 0 */
	if(dec > 90)
		--dec;
	if(ra >= 360)
		ra -= 360;
	return (ra<<8)|dec;
}
