#include	<u.h>
#include	<libc.h>
#include	<bio.h>
#include	<libg.h>
#include	"hdr.h"
#include	"../big5.h"

/*
	map: put big5 for runes from..to into chars
*/

void
bmap(int from, int to, long *chars)
{
	long *l, *ll;
	int k, k1, n;

	for(n = from; n <= to; n++)
		chars[n-from] = 0;
	for(l = tabbig5, ll = tabbig5+BIG5MAX; l < ll; l++)
		if((*l >= from) && (*l <= to))
			chars[*l-from] = l-tabbig5;
	k = 0;
	k1 = 0;		/* not necessary; just shuts ken up */
	for(n = from; n <= to; n++)
		if(chars[n-from] == 0){
			k++;
			k1 = n;
		}
	if(k){
		fprint(2, "%s: %d/%d chars found (missing include 0x%x=%d)\n", argv0, (to-from+1-k), to-from+1, k1, k1);
		/*exits("map problem");/**/
	}
}
