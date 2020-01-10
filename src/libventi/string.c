#include <u.h>
#include <libc.h>
#include <venti.h>

int
vtputstring(Packet *p, char *s)
{
	uchar buf[2];
	int n;

	if(s == nil){
		werrstr("null string in packet");
		return -1;
	}
	n = strlen(s);
	if(n > VtMaxStringSize){
		werrstr("string too long in packet");
		return -1;
	}
	buf[0] = n>>8;
	buf[1] = n;
	packetappend(p, buf, 2);
	packetappend(p, (uchar*)s, n);
	return 0;
}

int
vtgetstring(Packet *p, char **ps)
{
	uchar buf[2];
	int n;
	char *s;

	if(packetconsume(p, buf, 2) < 0)
		return -1;
	n = (buf[0]<<8) + buf[1];
	if(n > VtMaxStringSize) {
		werrstr("string too long in packet");
		return -1;
	}
	s = vtmalloc(n+1);
	if(packetconsume(p, (uchar*)s, n) < 0){
		vtfree(s);
		return -1;
	}
	s[n] = 0;
	*ps = s;
	return 0;
}
