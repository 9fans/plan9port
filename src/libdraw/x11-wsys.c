#include <u.h>
#include "x11-inc.h"
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include "x11-memdraw.h"

void
drawtopwindow(void)
{
	XMapRaised(_x.display, _x.drawable);
	XFlush(_x.display);
	XSetInputFocus(_x.display, _x.drawable, RevertToPointerRoot,
		CurrentTime);
	XFlush(_x.display);
}

void
drawresizewindow(Rectangle r)
{
//	XConfigureRequestEvent e;
	XWindowChanges e;
	int value_mask;

	memset(&e, 0, sizeof e);
	value_mask = CWWidth|CWHeight;
//	e.x = r.min.x;
//	e.y = r.min.y;
	e.width = Dx(r);
	e.height = Dy(r);
	XConfigureWindow(_x.display, _x.drawable, value_mask, &e);
}
