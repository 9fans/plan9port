#include <u.h>
#include <libc.h>

char*
sysname(void)
{
	char buf[300], *p, *q;

	if((q = getenv("sysname")) == nil){
		if(gethostname(buf, sizeof buf) < 0)
			goto err;
		buf[sizeof buf-1] = 0;
		q = strdup(buf);
		if(q == nil)
			goto err;
	}
	if((p = strchr(q, '.')) != nil)
		*p = 0;
	return q;

err:
	return "gnot";
}
