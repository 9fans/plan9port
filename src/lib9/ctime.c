#include <u.h>
#include <libc.h>

static
void
ct_numb(char *cp, int n)
{

	cp[0] = ' ';
	if(n >= 10)
		cp[0] = (n/10)%10 + '0';
	cp[1] = n%10 + '0';
}

char*
asctime(Tm *t)
{
	int i;
	char *ncp;
	static char cbuf[30];

	strcpy(cbuf, "Thu Jan 01 00:00:00 GMT 1970\n");
	ncp = &"SunMonTueWedThuFriSat"[t->wday*3];
	cbuf[0] = *ncp++;
	cbuf[1] = *ncp++;
	cbuf[2] = *ncp;
	ncp = &"JanFebMarAprMayJunJulAugSepOctNovDec"[t->mon*3];
	cbuf[4] = *ncp++;
	cbuf[5] = *ncp++;
	cbuf[6] = *ncp;
	ct_numb(cbuf+8, t->mday);
	ct_numb(cbuf+11, t->hour+100);
	ct_numb(cbuf+14, t->min+100);
	ct_numb(cbuf+17, t->sec+100);
	ncp = t->zone;
	for(i=0; i<3; i++)
		if(ncp[i] == 0)
			break;
	for(; i<3; i++)
		ncp[i] = '?';
	ncp = t->zone;
	cbuf[20] = *ncp++;
	cbuf[21] = *ncp++;
	cbuf[22] = *ncp;
	if(t->year >= 100) {
		cbuf[24] = '2';
		cbuf[25] = '0';
	}
	ct_numb(cbuf+26, t->year+100);
	return cbuf;
}

char*
ctime(long t)
{
	return asctime(localtime(t));
}

