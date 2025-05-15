#include <u.h>
#include <libc.h>
#include "libsec.h"

char *tests[] = {
	"",
	"a",
	"abc",
	"message digest",
	"abcdefghijklmnopqrstuvwxyz",
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
	"123456789012345678901234567890123456789012345678901234567890"
		"12345678901234567890",
	"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
	"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhi"
		"jklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
	0
};

void
main(void)
{
	int i;
	char **pp;
	uchar *p;
	uchar digest[SHA2_512dlen];

	print("SHA2_224 tests:\n");
	for(pp = tests; *pp; pp++){
		p = (uchar*)*pp;
		sha2_224(p, strlen(*pp), digest, 0);
		for(i = 0; i < SHA2_224dlen; i++)
			print("%2.2ux", digest[i]);
		print("\n");
	}

	print("\nSHA256 tests:\n");
	for(pp = tests; *pp; pp++){
		p = (uchar*)*pp;
		sha2_256(p, strlen(*pp), digest, 0);
		for(i = 0; i < SHA2_256dlen; i++)
			print("%2.2ux", digest[i]);
		print("\n");
	}

	print("\nSHA384 tests:\n");
	for(pp = tests; *pp; pp++){
		p = (uchar*)*pp;
		sha2_384(p, strlen(*pp), digest, 0);
		for(i = 0; i < SHA2_384dlen; i++)
			print("%2.2ux", digest[i]);
		print("\n");
	}

	print("\nSHA512 tests:\n");
	for(pp = tests; *pp; pp++){
		p = (uchar*)*pp;
		sha2_512(p, strlen(*pp), digest, 0);
		for(i = 0; i < SHA2_512dlen; i++)
			print("%2.2ux", digest[i]);
		print("\n");
	}
}
