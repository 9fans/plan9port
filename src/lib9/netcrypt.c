#include <u.h>
#include <libc.h>
#include <auth.h>

int
netcrypt(void *key, void *chal)
{
	uchar buf[8], *p;

	strncpy((char*)buf, chal, 7);
	buf[7] = '\0';
	for(p = buf; *p && *p != '\n'; p++)
		;
	*p = '\0';
	encrypt(key, buf, 8);
	sprint(chal, "%.2ux%.2ux%.2ux%.2ux", buf[0], buf[1], buf[2], buf[3]);
	return 1;
}
