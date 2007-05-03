#include "stdinc.h"
#include "dat.h"
#include "fns.h"

u8int zeroscore[VtScoreSize];

/* Call this function to force linking of score.o for zeroscore on OS X */
void needzeroscore(void) { }

void
scoremem(u8int *score, u8int *buf, int n)
{
	DigestState s;

	memset(&s, 0, sizeof s);
	sha1(buf, n, score, &s);
}

static int
hexv(int c)
{
	if(c >= '0' && c <= '9')
		return c - '0';
	if(c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if(c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

int
strscore(char *s, u8int *score)
{
	int i, c, d;

	for(i = 0; i < VtScoreSize; i++){
		c = hexv(s[2 * i]);
		if(c < 0)
			return -1;
		d = hexv(s[2 * i + 1]);
		if(d < 0)
			return -1;
		score[i] = (c << 4) + d;
	}
	return s[2 * i] == '\0';
}
