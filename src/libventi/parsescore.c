#include <u.h>
#include <libc.h>
#include <venti.h>

int
vtparsescore(char *s, char **prefix, uchar score[VtScoreSize])
{
	int i, c;
	char *buf, *colon;

	if((colon = strchr(s, ':')) != nil)
		buf = colon+1;
	else
		buf = s;

	if(strlen(buf) != 2*VtScoreSize)
		return -1;

	memset(score, 0, VtScoreSize);
	for(i=0; i<2*VtScoreSize; i++){
		if(buf[i] >= '0' && buf[i] <= '9')
			c = buf[i] - '0';
		else if(buf[i] >= 'a' && buf[i] <= 'z')
			c = buf[i] - 'a' + 10;
		else if(buf[i] >= 'A' && buf[i] <= 'Z')
			c = buf[i] - 'A' + 10;
		else
			return -1;

		if((i & 1) == 0)
			c <<= 4;
		score[i>>1] |= c;
	}
	if(colon){
		*colon = 0;
		if(prefix)
			*prefix = s;
	}else{
		if(prefix)
			*prefix = nil;
	}
	return 0;
}
