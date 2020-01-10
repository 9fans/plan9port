#include <u.h>
#include <libc.h>

void
main(void)
{
	char buf[512], *p;

	p = "???";
	if(getwd(buf, sizeof buf)){
		p = strrchr(buf, '/');
		if(p == nil)
			p = buf;
		else if(p>buf || p[1]!='\0')
			p++;
	}
	write(1, p, strlen(p));
	exits(0);
}
