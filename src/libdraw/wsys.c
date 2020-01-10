#include <u.h>
#include <libc.h>
#include <draw.h>

int _wantfocuschanges;

void
drawtopwindow(void)
{
	_displaytop(display);
}

int
drawsetlabel(char *label)
{
	return _displaylabel(display, label);
}

void
bouncemouse(Mouse *m)
{
	_displaybouncemouse(display, m);
}

void
drawresizewindow(Rectangle r)
{
	_displayresize(display, r);
}
