#include "x11-inc.h"
#include <u.h>
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

