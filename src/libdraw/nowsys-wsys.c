#include <u.h>
#include <libc.h>
#include <draw.h>

static int
bad(void)
{
	sysfatal("compiled with no window system support");
	return 0;
}

void
drawtopwindow(void)
{
	bad();
}

void
drawresizewindow(Rectangle r)
{
	USED(r);
	
	bad();
}
