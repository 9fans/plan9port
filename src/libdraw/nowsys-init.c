#include <u.h>
#include <libc.h>
#include <draw.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>

char *winsize;

static int
bad(void)
{
	sysfatal("compiled with no window system support"):
	return 0;
}

Display*
_initdisplay(void (*error)(Display*, char*), char *label)
{
	USED(error);
	USED(label);
	
	bad();
	return nil;
}

int
getwindow(Display *d, int ref)
{
	USED(d);
	USED(ref);
	return bad();
}

int
drawsetlabel(char *label)
{
	USED(label);
	return bad();
}

void
_flushmemscreen(Rectangle r)
{
	bad();
}

