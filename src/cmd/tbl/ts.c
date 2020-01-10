/* ts.c: minor string processing subroutines */
#include "t.h"

int
match (char *s1, char *s2)
{
	while (*s1 == *s2)
		if (*s1++ == '\0')
			return(1);
		else
			s2++;
	return(0);
}


int
prefix(char *small, char *big)
{
	int	c;

	while ((c = *small++) == *big++)
		if (c == 0)
			return(1);
	return(c == 0);
}


int
letter (int ch)
{
	if (ch >= 'a' && ch <= 'z')
		return(1);
	if (ch >= 'A' && ch <= 'Z')
		return(1);
	return(0);
}


int
numb(char *str)
{
				/* convert to integer */
	int	k;
	for (k = 0; *str >= '0' && *str <= '9'; str++)
		k = k * 10 + *str - '0';
	return(k);
}


int
digit(int x)
{
	return(x >= '0' && x <= '9');
}


int
max(int a, int b)
{
	return( a > b ? a : b);
}


void
tcopy (char *s, char *t)
{
	while (*s++ = *t++)
		;
}
