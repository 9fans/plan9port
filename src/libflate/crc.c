#include <u.h>
#include <libc.h>
#include <flate.h>

ulong*
mkcrctab(ulong poly)
{
	ulong *crctab;
	ulong crc;
	int i, j;

	crctab = malloc(256 * sizeof(ulong));
	if(crctab == nil)
		return nil;

	for(i = 0; i < 256; i++){
		crc = i;
		for(j = 0; j < 8; j++){
			if(crc & 1)
				crc = (crc >> 1) ^ poly;
			else
				crc >>= 1;
		}
		crctab[i] = crc;
	}
	return crctab;
}

ulong
blockcrc(ulong *crctab, ulong crc, void *vbuf, int n)
{
	uchar *buf, *ebuf;

	crc ^= 0xffffffff;
	buf = vbuf;
	ebuf = buf + n;
	while(buf < ebuf)
		crc = crctab[(crc & 0xff) ^ *buf++] ^ (crc >> 8);
	return crc ^ 0xffffffff;
}
