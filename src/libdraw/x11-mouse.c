#include "x11-inc.h"
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <memdraw.h>
#include "x11-memdraw.h"

void
moveto(Mousectl *m, Point pt)
{
	xmoveto(pt);
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
	int one;
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
	for(;;){
		XNextEvent(_x.mousecon, &xevent);
		switch(xevent.type){
		case Expose:
			xexpose(&xevent, _x.mousecon);
			continue;
		case ConfigureNotify:
			if(xconfigure(&xevent, _x.mousecon))
				nbsend(mc->resizec, &one);
			continue;
		case SelectionRequest:
			xselect(&xevent, _x.mousecon);
			continue;
		case ButtonPress:
		case ButtonRelease:
		case MotionNotify:
			if(xtoplan9mouse(&xevent, &m) < 0)
				continue;
			send(mc->c, &m);
			/*
			 * mc->Mouse is updated after send so it doesn't have wrong value if we block during send.
			 * This means that programs should receive into mc->Mouse (see readmouse() above) if
			 * they want full synchrony.
			 */
			mc->m = m;
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
	mc->resizec = chancreate(sizeof(int), 2);
	proccreate(_ioproc, mc, 16384);
	return mc;
}

void
setcursor(Mousectl *mc, Cursor *c)
{
	xsetcursor(c);
}

