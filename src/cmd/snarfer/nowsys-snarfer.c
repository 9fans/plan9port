#include <u.h>
#include <libc.h>

void
main(void)
{
	fprint(2, "no window system\n");
	exits("nowsys");
}
