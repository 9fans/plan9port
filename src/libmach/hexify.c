#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>

char *
_hexify(char *buf, u64int p, int zeros)
{
	ulong d;

	d = p/16;
	if(d)
		buf = _hexify(buf, d, zeros-1);
	else
		while(zeros--)
			*buf++ = '0';
	*buf++ = "0123456789abcdef"[p&0x0f];
	return buf;
}
