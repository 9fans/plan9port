#include "x11-inc.h"

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <cursor.h>
#include <event.h>

#include <memdraw.h>
#include "x11-memdraw.h"

ulong
event(Event *e)
{
	return eread(~0UL, e);
}

static void
eflush(void)
{
	/* avoid generating a message if there's nothing to show. */
	/* this test isn't perfect, though; could do flushimage(display, 0) then call extract */
	/* also: make sure we don't interfere if we're multiprocessing the display */
	if(display->locking){
		/* if locking is being done by program, this means it can't depend on automatic flush in emouse() etc. */
		if(canqlock(&display->qlock)){
			if(display->bufp > display->buf)
				flushimage(display, 1);
			unlockdisplay(display);
		}
	}else
		if(display->bufp > display->buf)
			flushimage(display, 1);
}

ulong
eread(ulong keys, Event *e)
{
	ulong xmask;
	XEvent xevent;

	xmask = ExposureMask;

	eflush();

	if(keys&Emouse)
		xmask |= MouseMask|StructureNotifyMask;
	if(keys&Ekeyboard)
		xmask |= KeyPressMask;

	XSelectInput(_x.display, _x.drawable, xmask);
again:
	XWindowEvent(_x.display, _x.drawable, xmask, &xevent);

	switch(xevent.type){
	case Expose:
		xexpose(&xevent, _x.display);
		goto again;
	case ConfigureNotify:
		if(xconfigure(&xevent, _x.display))
			eresized(1);
		goto again;
	case ButtonPress:
	case ButtonRelease:
	case MotionNotify:
		if(xtoplan9mouse(&xevent, &e->mouse) < 0)
			goto again;
		return Emouse;
	case KeyPress:
		e->kbdc = xtoplan9kbd(&xevent);
		if(e->kbdc == -1)
			goto again;
		return Ekeyboard;
	default:
		return 0;
	}
}

void
einit(ulong keys)
{
	keys &= ~(Emouse|Ekeyboard);
	if(keys){
		fprint(2, "unknown keys in einit\n");
		abort();
	}
}

int
ekbd(void)
{
	Event e;

	eread(Ekeyboard, &e);
	return e.kbdc;
}

Mouse
emouse(void)
{
	Event e;

	eread(Emouse, &e);
	return e.mouse;
}

int
ecanread(ulong keys)
{
	int can;

	can = 0;
	if(keys&Emouse)
		can |= ecanmouse();
	if(keys&Ekeyboard)
		can |= ecankbd();
	return can;
}

int
ecanmouse(void)
{
	XEvent xe;
	Mouse m;

	eflush();
again:
	if(XCheckWindowEvent(_x.display, _x.drawable, MouseMask, &xe)){
		if(xtoplan9mouse(&xe, &m) < 0)
			goto again;
		XPutBackEvent(_x.display, &xe);
		return 1;
	}
	return 0;
}

int
ecankbd(void)
{
	XEvent xe;

	eflush();
again:
	if(XCheckWindowEvent(_x.display, _x.drawable, KeyPressMask, &xe)){
		if(xtoplan9kbd(&xe) == -1)
			goto again;
		XPutBackEvent(_x.display, &xe);
		return 1;
	}
	return 0;
}

void
emoveto(Point p)
{
	xmoveto(p);
}

void
esetcursor(Cursor *c)
{
	xsetcursor(c);
}

