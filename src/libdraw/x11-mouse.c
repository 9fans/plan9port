#include <u.h>
#include "x11-inc.h"
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <memdraw.h>
#include "x11-memdraw.h"

int _windowhasfocus = 1;
int _wantfocuschanges;

void
moveto(Mousectl *m, Point pt)
{
	_xmoveto(pt);
}

void
closemouse(Mousectl *mc)
{
	if(mc == nil)
		return;

/*	postnote(PNPROC, mc->pid, "kill");
*/
	do; while(nbrecv(mc->c, &mc->m) > 0);
	close(mc->mfd);
	close(mc->cfd);
	free(mc->file);
	chanfree(mc->c);
	chanfree(mc->resizec);
	free(mc);
}

int
readmouse(Mousectl *mc)
{
	if(mc->display)
		flushimage(mc->display, 1);
	if(recv(mc->c, &mc->m) < 0){
		fprint(2, "readmouse: %r\n");
		return -1;
	}
	return 0;
}

static
void
_ioproc(void *arg)
{
	int fd, one, buttons;
	Atom a;
	ulong mask;
	Mouse m;
	Mousectl *mc;
	XEvent xevent;

	one = 1;
	mc = arg;
	threadsetname("mouseproc");
	memset(&m, 0, sizeof m);
	mc->pid = getpid();
	mask = MouseMask|ExposureMask|StructureNotifyMask;
	XSelectInput(_x.mousecon, _x.drawable, mask);
	fd = XConnectionNumber(_x.mousecon);
	buttons = 0;
	for(;;){
		XNextEvent(_x.mousecon, &xevent);
		switch(xevent.type){
		case Expose:
			_xexpose(&xevent, _x.mousecon);
			continue;
		case DestroyNotify:
			if(_xdestroy(&xevent, _x.mousecon)){
				/* drain it before sending */
				/* apps that care can notice we sent a 0 */
				/* otherwise we'll have getwindow send SIGHUP */
				nbrecv(mc->resizec, 0);
				nbrecv(mc->resizec, 0);
				send(mc->resizec, 0);
			}
			continue;
		case ConfigureNotify:
			if(_xconfigure(&xevent, _x.mousecon))
				nbsend(mc->resizec, &one);
			continue;
		case SelectionRequest:
			_xselect(&xevent, _x.mousecon);
			continue;
		case ButtonPress:
		case ButtonRelease:
		case MotionNotify:
			/* If the motion notifications are backing up, skip over some. */
			if(0 && xevent.type == MotionNotify){
				while(XCheckWindowEvent(_x.mousecon, _x.drawable, MouseMask, &xevent)){
					if(xevent.type != MotionNotify)
						break;
				}
			}
			m.buttons = buttons;
			if(_xtoplan9mouse(_x.mousecon, &xevent, &m) < 0)
				continue;
			buttons = m.buttons;
			send(mc->c, &m);
			/*
			 * mc->Mouse is updated after send so it doesn't have wrong value if we block during send.
			 * This means that programs should receive into mc->Mouse (see readmouse() above) if
			 * they want full synchrony.
			 */
			mc->m = m;
			break;
		case ClientMessage:
			if(xevent.xclient.message_type == _x.wmprotos){
				a = xevent.xclient.data.l[0];
				if(_wantfocuschanges && a == _x.takefocus){
					_windowhasfocus = 1;
					_x.newscreenr = _x.screenr;
					nbsend(mc->resizec, &one);
				}else if(_wantfocuschanges && a == _x.losefocus){
					_windowhasfocus = 0;
					_x.newscreenr = _x.screenr;
					nbsend(mc->resizec, &one);
				}
			}
			break;
		}
	}
}

Mousectl*
initmouse(char *file, Image *i)
{
	Mousectl *mc;

	mc = mallocz(sizeof(Mousectl), 1);
	if(i)
		mc->display = i->display;
	mc->c = chancreate(sizeof(Mouse), 0);
	chansetname(mc->c, "mousec");
	mc->resizec = chancreate(sizeof(int), 2);
	chansetname(mc->resizec, "resizec");
	proccreate(_ioproc, mc, 256*1024);
	return mc;
}

void
setcursor(Mousectl *mc, Cursor *c)
{
	_xsetcursor(c);
}

/*
 * Send the mouse event back to the window manager.
 * So that 9term can tell rio to pop up its button3 menu.
 * Note that we're using _x.mousecon in a few places,
 * so we have to be sure that the mouse proc isn't using it
 * when we call!  This is all a bit wonky and should be 
 * avoided unless you know what you're doing.
 */
void
bouncemouse(Mouse *m)
{
	XButtonEvent e;
	XWindow dw;

	e.type = ButtonPress;
	e.state = 0;
	e.button = 0;
	if(m->buttons&1)
		e.button = 1;
	else if(m->buttons&2)
		e.button = 2;
	else if(m->buttons&4)
		e.button = 3;
	e.same_screen = 1;
	XTranslateCoordinates(_x.display, _x.drawable,
		DefaultRootWindow(_x.display),
		m->xy.x, m->xy.y, &e.x_root, &e.y_root, &dw);
	e.root = DefaultRootWindow(_x.mousecon);
	e.window = e.root;
	e.subwindow = None;
	e.x = e.x_root;
	e.y = e.y_root;
#undef time
	e.time = CurrentTime;
	XUngrabPointer(_x.mousecon, m->msec);
	XSendEvent(_x.mousecon, e.root, True, ButtonPressMask, (XEvent*)&e);
	XFlush(_x.mousecon);
}
