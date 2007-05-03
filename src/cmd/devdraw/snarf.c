#include <u.h>
#include <sys/select.h>
#include <errno.h>
#include "x11-inc.h"
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>
#include <drawfcall.h>
#include "x11-memdraw.h"
#include "devdraw.h"

#undef time

#define MouseMask (\
	ButtonPressMask|\
	ButtonReleaseMask|\
	PointerMotionMask|\
	Button1MotionMask|\
	Button2MotionMask|\
	Button3MotionMask)

#define Mask MouseMask|ExposureMask|StructureNotifyMask|KeyPressMask|EnterWindowMask|LeaveWindowMask

void runxevent(XEvent*);

void
usage(void)
{
	fprint(2, "usage: snarf [-a] [-o | text]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	int apple;
	int out;

	apple = 0;
	out = 0;

	ARGBEGIN{
	case 'a':
		apple = 1;
		break;
	case 'o':
		out = 1;
		break;
	default:
		usage();
	}ARGEND

	if(out && argc != 0)
		usage();
	if(!out && argc != 1)
		usage();

	_x.fd = -1;

	memimageinit();
	_xattach("snarf", "20x20");

	XSelectInput(_x.display, _x.drawable, Mask);
	XFlush(_x.display);

	if(out){
		char *s;
		if(apple)
			s = _applegetsnarf();
		else
			s = _xgetsnarf();
		write(1, s, strlen(s));
		write(1, "\n", 1);
		exits(0);
	}else{
		_xputsnarf(argv[0]);
		for(;;){
			XEvent event;
			XNextEvent(_x.display, &event);
			runxevent(&event);
		}
	}
}

/*
 * Handle an incoming X event.
 */
void
runxevent(XEvent *xev)
{
	switch(xev->type){
	case Expose:
		_xexpose(xev);
		break;
	
	case DestroyNotify:
		if(_xdestroy(xev))
			exits(0);
		break;

	case SelectionRequest:
		_xselect(xev);
		break;
	}
}

