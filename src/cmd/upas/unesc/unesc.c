/*
 *	upas/unesc - interpret =?foo?bar?=char?= escapes
 */

#include <stdio.h>
#include <stdlib.h>

int
hex(int c)
{
	if('0' <= c && c <= '9')
		return c - '0';
	if('A' <= c && c <= 'F')
		return c - 'A' + 10;
	if('a' <= c && c <= 'f')
		return c - 'a' + 10;
	return 0;
}

int
main(int argc, char **argv)
{
	int c;

	while((c=getchar()) != EOF){
		if(c == '='){
			if((c=getchar()) == '?'){
				while((c=getchar()) != EOF && c != '?')
					continue;
				while((c=getchar()) != EOF && c != '?')
					continue;
				while((c=getchar()) != EOF && c != '?'){
					if(c == '='){
						c = hex(getchar()) << 4;
						c |= hex(getchar());
					}
					putchar(c);
				}
				(void) getchar();	/* consume '=' */
			}else{
				putchar('=');
				putchar(c);
			}
		}else
			putchar(c);
	}
	exit(0);
}
