#include <u.h>
#include <libc.h>
#include <draw.h>

/*
 * This code (and the devdraw interface) will have to change
 * if we ever get bitmaps with ldepth > 3, because the
 * colormap will have to be written in chunks
 */

void
writecolmap(Display *d, RGB *m)
{
	int i, n, fd;
	char buf[64], *t;
	ulong r, g, b;

	sprint(buf, "/dev/draw/%d/colormap", d->dirno);
	fd = open(buf, OWRITE);
	if(fd < 0)
		drawerror(d, "wrcolmap: open colormap failed");
	t = malloc(8192);
	n = 0;
	for(i = 0; i < 256; i++) {
		r = m[i].red>>24;
		g = m[i].green>>24;
		b = m[i].blue>>24;
		n += sprint(t+n, "%d %lud %lud %lud\n", 255-i, r, g, b);
	}
	i = write(fd, t, n);
	free(t);
	close(fd);
	if(i != n)
		drawerror(d, "wrcolmap: bad write");
}
