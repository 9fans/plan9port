#include <u.h>
#include <libc.h>
#include <thread.h>
#include <fcall.h>
#include <fs.h>
#include "plumb.h"

Plumbmsg*
threadplumbrecv(int fd)
{
	char *buf;
	Plumbmsg *m;
	int n, more;

	buf = malloc(8192);
	if(buf == nil)
		return nil;
	n = threadread(fd, buf, 8192);
	m = nil;
	if(n > 0){
		m = plumbunpackpartial(buf, n, &more);
		if(m==nil && more>0){
			/* we now know how many more bytes to read for complete message */
			buf = realloc(buf, n+more);
			if(buf == nil)
				return nil;
			if(threadreadn(fd, buf+n, more) == more)
				m = plumbunpackpartial(buf, n+more, nil);
		}
	}
	free(buf);
	return m;
}
