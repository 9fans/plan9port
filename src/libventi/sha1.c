#include <u.h>
#include <libc.h>
#include <venti.h>
#include <libsec.h>

void
vtsha1(uchar score[VtScoreSize], uchar *p, int n)
{
	DigestState ds;

	memset(&ds, 0, sizeof ds);
	sha1(p, n, score, &ds);
}

int
vtsha1check(uchar score[VtScoreSize], uchar *p, int n)
{
	DigestState ds;
	uchar score2[VtScoreSize];

	memset(&ds, 0, sizeof ds);
	sha1(p, n, score2, &ds);
	if(memcmp(score, score2, VtScoreSize) != 0) {
		werrstr("vtsha1check failed");
		return -1;
	}
	return 0;
}
