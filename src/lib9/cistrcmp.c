#include <u.h>
#include <libc.h>

int
cistrcmp(char *s1, char *s2)
{
	int c1, c2;

	while(*s1){
		c1 = *(uchar*)s1++;
		c2 = *(uchar*)s2++;

		if(c1 == c2)
			continue;

		if(c1 >= 'A' && c1 <= 'Z')
			c1 -= 'A' - 'a';

		if(c2 >= 'A' && c2 <= 'Z')
			c2 -= 'A' - 'a';

		if(c1 != c2)
			return c1 - c2;
	}
	return -*s2;
}
