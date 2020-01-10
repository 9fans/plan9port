/*
 * cat standard input until you get a zero byte
 */

#include <u.h>
#include <libc.h>

void
main(void)
{
	char buf[4096];
	char *p;
	int n;

	while((n = read(0, buf, sizeof(buf))) > 0){
		p = memchr(buf, 0, n);
		if(p != nil)
			n = p-buf;
		if(n > 0)
			write(1, buf, n);
		if(p != nil)
			break;
	}
	exits(0);
}
