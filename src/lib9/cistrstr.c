#include <u.h>
#include <libc.h>

char*
cistrstr(char *s, char *sub)
{
	int c, csub, n;

	csub = *sub;
	if(csub == '\0')
		return s;
	if(csub >= 'A' && csub <= 'Z')
		csub -= 'A' - 'a';
	sub++;
	n = strlen(sub);
	for(; c = *s; s++){
		if(c >= 'A' && c <= 'Z')
			c -= 'A' - 'a';
		if(c == csub && cistrncmp(s+1, sub, n) == 0)
			return s;
	}
	return nil;
}
