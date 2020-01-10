#include <u.h>
#include <libc.h>
#include <draw.h>
#include <mouse.h>
#include <cursor.h>
#include <drawfcall.h>

void
usage(void)
{
	fprint(2, "usage: devdraw (don't run  directly)\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	int n;
	uchar buf[1024*1024];
	Wsysmsg m;

	ARGBEGIN{
	case 'D':
		break;
	default:
		usage();
	}ARGEND

	if(argc != 0)
		usage();

	while((n = readwsysmsg(0, buf, sizeof buf)) > 0){
		convM2W(buf, n, &m);
		m.type = Rerror;
		m.error = "no window system present";
		n = convW2M(&m, buf, sizeof buf);
		write(1, buf, n);
	}
	exits(0);
}
