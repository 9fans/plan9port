#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

void
usage(void)
{
	fprint(2, "usage: iconv [-u] [-c chanstr] [file]\n");
	exits("usage");
}

void
writeuncompressed(int fd, Memimage *m)
{
	char chanstr[32];
	int bpl, y, j;
	uchar *buf;

	if(chantostr(chanstr, m->chan) == nil)
		sysfatal("can't convert channel descriptor: %r");
	fprint(fd, "%11s %11d %11d %11d %11d ",
		chanstr, m->r.min.x, m->r.min.y, m->r.max.x, m->r.max.y);

	bpl = bytesperline(m->r, m->depth);
	buf = malloc(bpl);
	if(buf == nil)
		sysfatal("malloc failed: %r");
	for(y=m->r.min.y; y<m->r.max.y; y++){
		j = unloadmemimage(m, Rect(m->r.min.x, y, m->r.max.x, y+1), buf, bpl);
		if(j != bpl)
			sysfatal("image unload failed: %r");
		if(write(fd, buf, bpl) != bpl)
			sysfatal("write failed: %r");
	}
	free(buf);
}

void
main(int argc, char *argv[])
{
	char *tostr, *file;
	int fd, uncompressed;
	ulong tochan;
	Memimage *m, *n;

	tostr = nil;
	uncompressed = 0;
	ARGBEGIN{
	case 'c':
		tostr = EARGF(usage());
		break;
	case 'u':
		uncompressed = 1;
		break;
	default:
		usage();
	}ARGEND

	memimageinit();

	file = "<stdin>";
	m = nil;

	switch(argc){
	case 0:
		m = readmemimage(0);
		break;
	case 1:
		file = argv[0];
		fd = open(file, OREAD);
		if(fd < 0)
			sysfatal("can't open %s: %r", file);
		m = readmemimage(fd);
		close(fd);
		break;
	default:
		usage();
	}

	if(m == nil)
		sysfatal("can't read %s: %r", file);

	if(tostr == nil)
		tochan = m->chan;
	else{
		tochan = strtochan(tostr);
		if(tochan == 0)
			sysfatal("bad channel descriptor '%s'", tostr);
	}

	n = allocmemimage(m->r, tochan);
	if(n == nil)
		sysfatal("can't allocate new image: %r");

	memimagedraw(n, n->r, m, m->r.min, nil, ZP, S);
	if(uncompressed)
		writeuncompressed(1, n);
	else
		writememimage(1, n);
	exits(nil);
}
