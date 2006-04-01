#include <u.h>
#include <libc.h>
#include <draw.h>
#include <html.h>
#include "impl.h"

/* Do case-insensitive lookup of key[0:keylen] in t[0:n] (key part), */
/* returning 1 if found, 0 if not. */
/* Array t must be sorted in increasing lexicographic order of key. */
/* If found, return corresponding val in *pans. */
int
_lookup(StringInt* t, int n, Rune* key, int keylen, int* pans)
{
	int	min;
	int	max;
	int	try;
	int	cmpresult;

	min = 0;
	max = n - 1;
	while(min <= max) {
		try = (min + max)/2;
		cmpresult = _Strncmpci(key, keylen, t[try].key);
		if(cmpresult > 0)
			min = try + 1;
		else if(cmpresult < 0)
			max = try - 1;
		else {
			*pans = t[try].val;
			return 1;
		}
	}
	return 0;
}

/* Return first key in t[0:n] that corresponds to val, */
/* nil if none. */
Rune*
_revlookup(StringInt* t, int n, int val)
{
	int	i;

	for(i = 0; i < n; i++)
		if(t[i].val == val)
			return t[i].key;
	return nil;
}

/* Make a StringInt table out of a[0:n], mapping each string */
/* to its index.  Check that entries are in alphabetical order. */
StringInt*
_makestrinttab(Rune** a, int n)
{
	StringInt*	ans;
	int	i;

	ans = (StringInt*)emalloc(n * sizeof(StringInt));
	for(i = 0; i < n; i++) {
		ans[i].key = a[i];
		ans[i].val = i;
		assert(i == 0 || runestrcmp(a[i], a[i - 1]) >= 0);
	}
	return ans;
}
