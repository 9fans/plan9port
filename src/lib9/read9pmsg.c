#include <u.h>
#include <libc.h>
#include <fcall.h>

int
read9pmsg(int fd, void *abuf, uint n)
{
	int m, len;
	uchar *buf;

	buf = abuf;

	/* read count */
	m = readn(fd, buf, BIT32SZ);
	if(m != BIT32SZ){
		if(m < 0)
			return -1;
		return 0;
	}

	len = GBIT32(buf);
	if(len <= BIT32SZ || len > n){
		werrstr("bad length in 9P2000 message header");
		return -1;
	}
	len -= BIT32SZ;
	m = readn(fd, buf+BIT32SZ, len);
	if(m < len)
		return 0;
	return BIT32SZ+m;
}
