#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <keyboard.h>

void
closekeyboard(Keyboardctl *kc)
{
	Rune r;

	if(kc == nil)
		return;

/*	postnote(PNPROC, kc->pid, "kill"); */

	do; while(nbrecv(kc->c, &r) > 0);
	chanfree(kc->c);
	free(kc);
}

static
void
_ioproc(void *arg)
{
	Rune r;
	Keyboardctl *kc;

	kc = arg;
	threadsetname("kbdproc");
	for(;;){
		if(_displayrdkbd(display, &r) < 0)
			threadexits("read error");
		send(kc->c, &r);
	}
}

Keyboardctl*
initkeyboard(char *file)
{
	Keyboardctl *kc;

	kc = mallocz(sizeof(Keyboardctl), 1);
	if(kc == nil)
		return nil;
	USED(file);
	kc->c = chancreate(sizeof(Rune), 20);
	chansetname(kc->c, "kbdc");
	proccreate(_ioproc, kc, 32*1024);
	return kc;
}
