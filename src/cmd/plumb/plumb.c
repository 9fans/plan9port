#include <u.h>
#include <libc.h>
#include <plumb.h>

char *plumbfile = nil;
Plumbmsg m;

void
usage(void)
{
	fprint(2, "usage:  plumb [-p plumbfile] [-a 'attr=value ...'] [-s src] [-d dst] [-t type] [-w wdir] -i | data1\n");
	exits("usage");
}

void
gather(void)
{
	char buf[8192];
	int n;

	m.ndata = 0;
	m.data = nil;
	while((n = read(0, buf, sizeof buf)) > 0){
		m.data = realloc(m.data, m.ndata+n);
		if(m.data == nil){
			fprint(2, "plumb: alloc failed: %r\n");
			exits("alloc");
		}
		memmove(m.data+m.ndata, buf, n);
		m.ndata += n;
	}
	if(n < 0){
		fprint(2, "plumb: i/o error on input: %r\n");
		exits("read");
	}
}

void
main(int argc, char *argv[])
{
	char buf[1024], *p;
	int fd, i, input;

	input = 0;
	m.src = "plumb";
	m.dst = nil;
	m.wdir = getwd(buf, sizeof buf);
	m.type = "text";
	m.attr = nil;
	ARGBEGIN{
	case 'a':
		p = ARGF();
		if(p == nil)
			usage();
		m.attr = plumbaddattr(m.attr, plumbunpackattr(p));
		break;
	case 'd':
		m.dst = ARGF();
		if(m.dst == nil)
			usage();
		break;
	case 'i':
		input++;
		break;
	case 't':
	case 'k':	/* for backwards compatibility */
		m.type = ARGF();
		if(m.type == nil)
			usage();
		break;
	case 'p':
		plumbfile = ARGF();
		if(plumbfile == nil)
			usage();
		break;
	case 's':
		m.src = ARGF();
		if(m.src == nil)
			usage();
		break;
	case 'w':
		m.wdir = ARGF();
		if(m.wdir == nil)
			usage();
		break;
	}ARGEND

	if((input && argc>0) || (!input && argc<1))
		usage();
	if(plumbfile != nil)
		fd = open(plumbfile, OWRITE);
	else
		fd = plumbopen("send", OWRITE);
	if(fd < 0){
		fprint(2, "plumb: can't open plumb file: %r\n");
		exits("open");
	}
	if(input){
		gather();
		if(plumblookup(m.attr, "action") == nil)
			m.attr = plumbaddattr(m.attr, plumbunpackattr("action=showdata"));
		if(plumbsend(fd, &m) < 0){
			fprint(2, "plumb: can't send message: %r\n");
			exits("error");
		}
		exits(nil);
	}
	for(i=0; i<argc; i++){
		if(input == 0){
			m.data = argv[i];
			m.ndata = -1;
		}
		if(plumbsend(fd, &m) < 0){
			fprint(2, "plumb: can't send message: %r\n");
			exits("error");
		}
	}
	exits(nil);
}
