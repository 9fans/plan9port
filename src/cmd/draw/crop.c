#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

enum
{
	None,
	Inset,	/* move border in or out uniformly */
	Insetxy,	/* move border in or out; different parameters for x and y */
	Set,		/* set rectangle to absolute values */
	Blank	/* cut off blank region according to color value */
			/* Blank is not actually set as a mode; it can be combined with others */
};

void
usage(void)
{
	fprint(2, "usage: crop [-c rgb] [-i ±inset | -r R | -x ±inset | -y ±inset] [-t tx ty] [-b rgb ] [imagefile]\n");
	fprint(2, "\twhere R is a rectangle minx miny maxx maxy\n");
	fprint(2, "\twhere rgb is a color red green blue\n");
	exits("usage");
}

int
getint(char *s)
{
	if(s == nil)
		usage();
	if(*s == '+')
		return atoi(s+1);
	if(*s == '-')
		return -atoi(s+1);
	return atoi(s);
}

Rectangle
crop(Memimage *m, uint32 c)
{
	Memimage *n;
	int x, y, bpl, wpl;
	int left, right, top, bottom;
	uint32 *buf;

	left = m->r.max.x;
	right = m->r.min.x;
	top = m->r.max.y;
	bottom = m->r.min.y;
	n = nil;
	if(m->chan != RGBA32){
		/* convert type for simplicity */
		n = allocmemimage(m->r, RGBA32);
		if(n == nil)
			sysfatal("can't allocate temporary image: %r");
		memimagedraw(n, n->r, m, m->r.min, nil, ZP, S);
		m = n;
	}
	wpl = wordsperline(m->r, m->depth);
	bpl = wpl*sizeof(uint32);
	buf = malloc(bpl);
	if(buf == nil)
		sysfatal("can't allocate buffer: %r");

	for(y=m->r.min.y; y<m->r.max.y; y++){
		x = unloadmemimage(m, Rect(m->r.min.x, y, m->r.max.x, y+1), (uchar*)buf, bpl);
		if(x != bpl)
			sysfatal("unloadmemimage");
		for(x=0; x<wpl; x++)
			if(buf[x] != c){
				if(x < left)
					left = x;
				if(x > right)
					right = x;
				if(y < top)
					top = y;
				bottom = y;
			}
	}

	if(n != nil)
		freememimage(n);
	return Rect(left, top, right+1, bottom+1);
}

void
main(int argc, char *argv[])
{
	int fd, mode, red, green, blue;
	Rectangle r, rparam;
	Point t;
	Memimage *m, *new;
	char *file;
	uint32 bg, cropval;
	long dw;

	memimageinit();
	mode = None;
	bg = 0;
	cropval = 0;
	t = ZP;
	memset(&rparam, 0, sizeof rparam);

	ARGBEGIN{
	case 'b':
		if(bg != 0)
			usage();
		red = getint(ARGF())&0xFF;
		green = getint(ARGF())&0xFF;
		blue = getint(ARGF())&0xFF;
		bg = (red<<24)|(green<<16)|(blue<<8)|0xFF;
		break;
	case 'c':
		if(cropval != 0)
			usage();
		red = getint(ARGF())&0xFF;
		green = getint(ARGF())&0xFF;
		blue = getint(ARGF())&0xFF;
		cropval = (red<<24)|(green<<16)|(blue<<8)|0xFF;
		break;
	case 'i':
		if(mode != None)
			usage();
		mode = Inset;
		rparam.min.x = getint(ARGF());
		break;
	case 'x':
		if(mode != None && mode != Insetxy)
			usage();
		mode = Insetxy;
		rparam.min.x = getint(ARGF());
		break;
	case 'y':
		if(mode != None && mode != Insetxy)
			usage();
		mode = Insetxy;
		rparam.min.y = getint(ARGF());
		break;
	case 'r':
		if(mode != None)
			usage();
		mode = Set;
		rparam.min.x = getint(ARGF());
		rparam.min.y = getint(ARGF());
		rparam.max.x = getint(ARGF());
		rparam.max.y = getint(ARGF());
		break;
	case 't':
		t.x = getint(ARGF());
		t.y = getint(ARGF());
		break;
	default:
		usage();
	}ARGEND

	if(mode == None && cropval == 0 && eqpt(ZP, t))
		usage();

	file = "<stdin>";
	fd = 0;
	if(argc > 1)
		usage();
	else if(argc == 1){
		file = argv[0];
		fd = open(file, OREAD);
		if(fd < 0)
			sysfatal("can't open %s: %r", file);
	}

	m = readmemimage(fd);
	if(m == nil)
		sysfatal("can't read %s: %r", file);

	r = m->r;
	if(cropval != 0){
		r = crop(m, cropval);
		m->clipr = r;
	}

	switch(mode){
	case None:
		break;
	case Inset:
		r = insetrect(r, rparam.min.x);
		break;
	case Insetxy:
		r.min.x += rparam.min.x;
		r.max.x -= rparam.min.x;
		r.min.y += rparam.min.y;
		r.max.y -= rparam.min.y;
		break;
	case Set:
		r = rparam;
		break;
	}

	new = allocmemimage(r, m->chan);
	if(new == nil)
		sysfatal("can't allocate new image: %r");
	if(bg != 0)
		memfillcolor(new, bg);
	else
		memfillcolor(new, 0x000000FF);

	memimagedraw(new, m->clipr, m, m->clipr.min, nil, ZP, S);
	dw = byteaddr(new, ZP) - byteaddr(new, t);
	new->r = rectaddpt(new->r, t);
	new->zero += dw;
	if(writememimage(1, new) < 0)
		sysfatal("write error on output: %r");
	exits(nil);
}
