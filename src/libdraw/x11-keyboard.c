#include <u.h>
#include "x11-inc.h"
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <memdraw.h>
#include <keyboard.h>
#include "x11-memdraw.h"

void
closekeyboard(Keyboardctl *kc)
{
	if(kc == nil)
		return;

/*	postnote(PNPROC, kc->pid, "kill");
*/

#ifdef BUG
	/* Drain the channel */
	while(?kc->c)
		<-kc->c;
#endif

	close(kc->ctlfd);
	close(kc->consfd);
	free(kc->file);
	free(kc->c);
	free(kc);
}

static
void
_ioproc(void *arg)
{
	int i;
	int fd;
	Keyboardctl *kc;
	Rune r;
	XEvent xevent;

	kc = arg;
	threadsetname("kbdproc");
	kc->pid = getpid();
	fd = XConnectionNumber(_x.kbdcon);
	XSelectInput(_x.kbdcon, _x.drawable, KeyPressMask);
	for(;;){
		while(XCheckWindowEvent(_x.kbdcon, _x.drawable, KeyPressMask, &xevent) == False)
			threadfdwait(fd, 'r');
		switch(xevent.type){
		case KeyPress:
			i = _xtoplan9kbd(&xevent);
			if(i == -1)
				continue;
			r = i;
			send(kc->c, &r);
			while((i=_xtoplan9kbd(nil)) >= 0){
				r = i;
				send(kc->c, &r);
			}
			break;
		}
	}
}

Keyboardctl*
initkeyboard(char *file)
{
	Keyboardctl *kc;

	threadfdwaitsetup();
	kc = mallocz(sizeof(Keyboardctl), 1);
	if(kc == nil)
		return nil;
	kc->c = chancreate(sizeof(Rune), 20);
	threadcreate(_ioproc, kc, 4096);
	return kc;
}

