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
	/*
	 * Should not be using kbdcon since we're not running
	 * in the kbdproc, but this is necessary to make the keyboard
	 * take focus if the window is hidden when drawtopwindow
	 * is called.  Let's hope that XSetInputFocus is only a write 
	 * on the fd, and so it's okay to do even though the kbdproc
	 * is reading at the same time.
	 */
	XSetInputFocus(_x.kbdcon, _x.drawable, RevertToPointerRoot,
		CurrentTime);
	XFlush(_x.kbdcon);
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
