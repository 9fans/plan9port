#include <u.h>
#include <libc.h>
#include <bin.h>
#include <httpd.h>

char*
hlower(char *p)
{
	char c;
	char *x;

	if(p == nil)
		return p;

	for(x = p; c = *x; x++)
		if(c >= 'A' && c <= 'Z')
			*x -= 'A' - 'a';
	return p;
}
