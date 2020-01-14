#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>

void
moveto(Mousectl *mc, Point pt)
{
	_displaymoveto(mc->display, pt);
	mc->m.xy = pt;
}

void
closemouse(Mousectl *mc)
{
	if(mc == nil)
		return;

/*	postnote(PNPROC, mc->pid, "kill"); */

	do; while(nbrecv(mc->c, &mc->m) > 0);
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
	int one, resized;
	Mouse m;
	Mousectl *mc;

	mc = arg;
	threadsetname("mouseproc");
	memset(&m, 0, sizeof m);
	one = 1;
	resized = 0;
	for(;;){
		if(_displayrdmouse(mc->display, &m, &resized) < 0) {
			if(postnote(PNPROC, getpid(), "hangup") < 0)
				fprint(2, "postnote: %r\n");
			sleep(10*1000);
			threadexitsall("mouse read error");
		}
		if(resized)
			send(mc->resizec, &one);
		send(mc->c, &m);
		/*
		 * mc->m is updated after send so it doesn't have wrong value if we block during send.
		 * This means that programs should receive into mc->Mouse (see readmouse() above) if
		 * they want full synchrony.
		 */
		mc->m = m;
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
	proccreate(_ioproc, mc, 32*1024);
	return mc;
}

void
setcursor(Mousectl *mc, Cursor *c)
{
	_displaycursor(mc->display, c, nil);
}

void
setcursor2(Mousectl *mc, Cursor *c, Cursor2 *c2)
{
	_displaycursor(mc->display, c, c2);
}
