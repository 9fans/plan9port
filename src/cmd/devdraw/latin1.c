#include <u.h>
#include <libc.h>
#include <draw.h>

/*
 * The code makes two assumptions: strlen(ld) is 1 or 2; latintab[i].ld can be a
 * prefix of latintab[j].ld only when j<i.
 */
static struct cvlist
{
	char	*ld;		/* must be seen before using this conversion */
	char	*si;		/* options for last input characters */
	Rune	so[60];		/* the corresponding Rune for each si entry */
} latintab[] = {
#include "latin1.h"
	0, 0, { 0 }
};

/*
 * Given 5 characters k[0]..k[4], find the rune or return -1 for failure.
 */
static long
unicode(Rune *k)
{
	long i, c;

	k++;	/* skip 'X' */
	c = 0;
	for(i=0; i<4; i++,k++){
		c <<= 4;
		if('0'<=*k && *k<='9')
			c += *k-'0';
		else if('a'<=*k && *k<='f')
			c += 10 + *k-'a';
		else if('A'<=*k && *k<='F')
			c += 10 + *k-'A';
		else
			return -1;
	}
	return c;
}

/*
 * Given n characters k[0]..k[n-1], find the corresponding rune or return -1 for
 * failure, or something < -1 if n is too small.  In the latter case, the result
 * is minus the required n.
 */
int
_latin1(Rune *k, int n)
{
	struct cvlist *l;
	int c;
	char* p;

	if(k[0] == 'X'){
		if(n>=5)
			return unicode(k);
		else
			return -5;
	}
	
	for(l=latintab; l->ld!=0; l++)
		if(k[0] == l->ld[0]){
			if(n == 1)
				return -2;
			if(l->ld[1] == 0)
				c = k[1];
			else if(l->ld[1] != k[1])
				continue;
			else if(n == 2)
				return -3;
			else
				c = k[2];
			for(p=l->si; *p!=0; p++)
				if(*p == c)
					return l->so[p - l->si];
			return -1;
		}
	return -1;
}
