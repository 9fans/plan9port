#include "os.h"
#include <mp.h>
#include <libsec.h>

char *tests[] = {
	"",
	"a",
	"abc",
	"message digest",
	"abcdefghijklmnopqrstuvwxyz",
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
	"12345678901234567890123456789012345678901234567890123456789012345678901234567890",
	0
};

void
main(void)
{
	char **pp;
	uchar *p;
	int i;
	uchar digest[MD5dlen];

	for(pp = tests; *pp; pp++){
		p = (uchar*)*pp;
		md4(p, strlen(*pp), digest, 0);
		for(i = 0; i < MD5dlen; i++)
			print("%2.2ux", digest[i]);
		print("\n");
	}
}
