#include <u.h>
#include "x11-inc.h"
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include "x11-memdraw.h"

void
_xtopwindow(void)
{
	XMapRaised(_x.display, _x.drawable);
	XSetInputFocus(_x.display, _x.drawable, RevertToPointerRoot,
		CurrentTime);
	XFlush(_x.display);
}

void
_xresizewindow(Rectangle r)
{
	XWindowChanges e;
	int value_mask;

	memset(&e, 0, sizeof e);
	value_mask = CWX|CWY|CWWidth|CWHeight;
	e.width = Dx(r);
	e.height = Dy(r);
	XConfigureWindow(_x.display, _x.drawable, value_mask, &e);
	XFlush(_x.display);
}

void
_xmovewindow(Rectangle r)
{
	XWindowChanges e;
	int value_mask;

	memset(&e, 0, sizeof e);
	value_mask = CWX|CWY|CWWidth|CWHeight;
	e.x = r.min.x;
	e.y = r.min.y;
	e.width = Dx(r);
	e.height = Dy(r);
	XConfigureWindow(_x.display, _x.drawable, value_mask, &e);
	XFlush(_x.display);
}

