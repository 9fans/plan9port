#include <u.h>
#include <libc.h>
#include <httpd.h>

/*
 * parse a search string of the form
 * tag=val&tag1=val1...
 */
HSPairs*
hparsequery(HConnect *c, char *search)
{
	HSPairs *q;
	char *tag, *val, *s;

	while((s = strchr(search, '?')) != nil)
		search = s + 1;
	s = search;
	while((s = strchr(s, '+')) != nil)
		*s++ = ' ';
	q = nil;
	while(*search){
		tag = search;
		while(*search != '='){
			if(*search == '\0')
				return q;
			search++;
		}
		*search++ = 0;
		val = search;
		while(*search != '&'){
			if(*search == '\0')
				return hmkspairs(c, hurlunesc(c, tag), hurlunesc(c, val), q);
			search++;
		}
		*search++ = '\0';
		q = hmkspairs(c, hurlunesc(c, tag), hurlunesc(c, val), q);
	}
	return q;
}
