#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <cursor.h>
#include <keyboard.h>
#include <frame.h>
#include "flayer.h"
#include "samterm.h"

int	protodebug;
int	cursorfd;
int	plumbfd = -1;
int	input;
int	got;
int	block;
int	kbdc;
int	resized;
uchar	*hostp;
uchar	*hoststop;
uchar	*plumbbase;
uchar	*plumbp;
uchar	*plumbstop;
Channel	*plumbc;
Channel	*hostc;
Mousectl	*mousectl;
Mouse	*mousep;
Keyboardctl *keyboardctl;
void	panic(char*);

void
initio(void)
{
	threadsetname("main");
	if(protodebug) print("mouse\n");
	mousectl = initmouse(nil, display->image);
	if(mousectl == nil){
		fprint(2, "samterm: mouse init failed: %r\n");
		threadexitsall("mouse");
	}
	mousep = &mousectl->m;
	if(protodebug) print("kbd\n");
	keyboardctl = initkeyboard(nil);
	if(keyboardctl == nil){
		fprint(2, "samterm: keyboard init failed: %r\n");
		threadexitsall("kbd");
	}
	if(protodebug) print("hoststart\n");
	hoststart();
	if(protodebug) print("plumbstart\n");
	if(plumbstart() < 0){
		if(protodebug) print("extstart\n");
		extstart();
	}
	if(protodebug) print("initio done\n");
}

void
getmouse(void)
{
	if(readmouse(mousectl) < 0)
		panic("mouse");
}

void
mouseunblock(void)
{
	got &= ~(1<<RMouse);
}

void
kbdblock(void)
{		/* ca suffit */
	block = (1<<RKeyboard)|(1<<RPlumb);
}

int
button(int but)
{
	getmouse();
	return mousep->buttons&(1<<(but-1));
}

void
externload(int i)
{
	drawtopwindow();
	plumbbase = malloc(plumbbuf[i].n);
	if(plumbbase == 0)
		return;
	memmove(plumbbase, plumbbuf[i].data, plumbbuf[i].n);
	plumbp = plumbbase;
	plumbstop = plumbbase + plumbbuf[i].n;
	got |= 1<<RPlumb;
}

int
waitforio(void)
{
	Alt alts[NRes+1];
	Rune r;
	int i;
	ulong type;

again:
	alts[RPlumb].c = plumbc;
	alts[RPlumb].v = &i;
	alts[RPlumb].op = CHANRCV;
	if((block & (1<<RPlumb)) || plumbc == nil)
		alts[RPlumb].op = CHANNOP;

	alts[RHost].c = hostc;
	alts[RHost].v = &i;
	alts[RHost].op = CHANRCV;
	if(block & (1<<RHost))
		alts[RHost].op = CHANNOP;

	alts[RKeyboard].c = keyboardctl->c;
	alts[RKeyboard].v = &r;
	alts[RKeyboard].op = CHANRCV;
	if(block & (1<<RKeyboard))
		alts[RKeyboard].op = CHANNOP;

	alts[RMouse].c = mousectl->c;
	alts[RMouse].v = &mousectl->m;
	alts[RMouse].op = CHANRCV;
	if(block & (1<<RMouse))
		alts[RMouse].op = CHANNOP;

	alts[RResize].c = mousectl->resizec;
	alts[RResize].v = nil;
	alts[RResize].op = CHANRCV;
	if(block & (1<<RResize))
		alts[RResize].op = CHANNOP;

	if(protodebug) print("waitforio %c%c%c%c%c\n",
		"h-"[alts[RHost].op == CHANNOP],
		"k-"[alts[RKeyboard].op == CHANNOP],
		"m-"[alts[RMouse].op == CHANNOP],
		"p-"[alts[RPlumb].op == CHANNOP],
		"R-"[alts[RResize].op == CHANNOP]);

	alts[NRes].op = CHANEND;

	if(got & ~block)
		return got & ~block;
	flushimage(display, 1);
	type = alt(alts);
	switch(type){
	case RHost:
		if(0) print("hostalt recv %d %d\n", i, hostbuf[i].n);
		hostp = hostbuf[i].data;
		hoststop = hostbuf[i].data + hostbuf[i].n;
		block = 0;
		break;
	case RPlumb:
		externload(i);
		break;
	case RKeyboard:
		kbdc = r;
		break;
	case RMouse:
		break;
	case RResize:
		resized = 1;
		/* do the resize in line if we've finished initializing and we're not in a blocking state */
		if(hasunlocked && block==0 && RESIZED())
			resize();
		goto again;
	}
	got |= 1<<type;
	return got;
}

int
rcvchar(void)
{
	int c;

	if(!(got & (1<<RHost)))
		return -1;
	c = *hostp++;
	if(hostp == hoststop)
		got &= ~(1<<RHost);
	return c;
}

char*
rcvstring(void)
{
	*hoststop = 0;
	got &= ~(1<<RHost);
	return (char*)hostp;
}

int
getch(void)
{
	int c;

	while((c = rcvchar()) == -1){
		block = ~(1<<RHost);
		waitforio();
		block = 0;
	}
	return c;
}

int
externchar(void)
{
	Rune r;

    loop:
	if(got & ((1<<RPlumb) & ~block)){
		plumbp += chartorune(&r, (char*)plumbp);
		if(plumbp >= plumbstop){
			got &= ~(1<<RPlumb);
			free(plumbbase);
		}
		if(r == 0)
			goto loop;
		return r;
	}
	return -1;
}

int kpeekc = -1;
int
ecankbd(void)
{
	Rune r;

	if(kpeekc >= 0)
		return 1;
	if(nbrecv(keyboardctl->c, &r) > 0){
		kpeekc = r;
		return 1;
	}
	return 0;
}

int
ekbd(void)
{
	int c;
	Rune r;

	if(kpeekc >= 0){
		c = kpeekc;
		kpeekc = -1;
		return c;
	}
	if(recv(keyboardctl->c, &r) < 0){
		fprint(2, "samterm: keybard recv error: %r\n");
		panic("kbd");
	}
	return r;
}

int
kbdchar(void)
{
	int c, i;

	c = externchar();
	if(c > 0)
		return c;
	if(got & (1<<RKeyboard)){
		c = kbdc;
		kbdc = -1;
		got &= ~(1<<RKeyboard);
		return c;
	}
	while(plumbc!=nil && nbrecv(plumbc, &i)>0){
		externload(i);
		c = externchar();
		if(c > 0)
			return c;
	}
	if(!ecankbd())
		return -1;
	return ekbd();
}

int
qpeekc(void)
{
	return kbdc;
}

int
RESIZED(void)
{
	if(resized){
		if(getwindow(display, Refnone) < 0)
			panic("can't reattach to window");
		resized = 0;
		return 1;
	}
	return 0;
}
