#include <u.h>
#include <libc.h>
#include <draw.h>

static char channames[] = "rgbkamx";
char*
chantostr(char *buf, u32int cc)
{
	u32int c, rc;
	char *p;

	if(chantodepth(cc) == 0)
		return nil;

	/* reverse the channel descriptor so we can easily generate the string in the right order */
	rc = 0;
	for(c=cc; c; c>>=8){
		rc <<= 8;
		rc |= c&0xFF;
	}

	p = buf;
	for(c=rc; c; c>>=8) {
		*p++ = channames[TYPE(c)];
		*p++ = '0'+NBITS(c);
	}
	*p = 0;

	return buf;
}

u32int
strtochan(char *s)
{
	char *p, *q;
	u32int c;
	int t, n;

	c = 0;
	p=s;
	while(*p && isspace((uchar)*p))
		p++;

	while(*p && !isspace((uchar)*p)){
		if((q = strchr(channames, p[0])) == nil)
			return 0;
		t = q-channames;
		if(p[1] < '0' || p[1] > '9')
			return 0;
		n = p[1]-'0';
		c = (c<<8) | __DC(t, n);
		p += 2;
	}
	return c;
}

int
chantodepth(u32int c)
{
	int n;

	for(n=0; c; c>>=8){
		if(TYPE(c) >= NChan || NBITS(c) > 8 || NBITS(c) <= 0)
			return 0;
		n += NBITS(c);
	}
	if(n==0 || (n>8 && n%8) || (n<8 && 8%n))
		return 0;
	return n;
}
