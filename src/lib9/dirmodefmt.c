#include <u.h>
#include <libc.h>

static char *modes[] =
{
	"---",
	"--x",
	"-w-",
	"-wx",
	"r--",
	"r-x",
	"rw-",
	"rwx",
};

static void
rwx(long m, char *s)
{
	strncpy(s, modes[m], 3);
}

int
dirmodefmt(Fmt *f)
{
	static char buf[16];
	ulong m;

	m = va_arg(f->args, ulong);

	if(m & DMDIR)
		buf[0]='d';
	else if(m & DMAPPEND)
		buf[0]='a';
	else if(m & DMAUTH)
		buf[0]='A';
	else if(m & DMDEVICE)
		buf[0] = 'D';
	else if(m & DMSYMLINK)
		buf[0] = 'L';
	else if(m & DMSOCKET)
		buf[0] = 'S';
	else if(m & DMNAMEDPIPE)
		buf[0] = 'P';
	else
		buf[0]='-';
	if(m & DMEXCL)
		buf[1]='l';
	else
		buf[1]='-';
	rwx((m>>6)&7, buf+2);
	rwx((m>>3)&7, buf+5);
	rwx((m>>0)&7, buf+8);
	buf[11] = 0;
	return fmtstrcpy(f, buf);
}
