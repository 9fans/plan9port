#include <u.h>
#include <libc.h>

char*
sysname(void)
{
	static char buf[512];
	char *p, *q;

	if(buf[0])
		return buf;

	if((q = getenv("sysname")) != nil && q[0] != 0){
		utfecpy(buf, buf+sizeof buf, q);
		free(q);
		return buf;
	}
	if(q)
		free(q);

	if(gethostname(buf, sizeof buf) >= 0){
		buf[sizeof buf-1] = 0;
		if((p = strchr(buf, '.')) != nil)
			*p = 0;
		return buf;
	}

	strcpy(buf, "gnot");
	return buf;
}
