/*
 * Pop-up menus.
 */

/* Copyright (c) 1994-1996 David Hogan, see README for licence details */
#define _SVID_SOURCE 1	/* putenv in glibc */
#define _DEFAULT_SOURCE 1
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "dat.h"
#include "fns.h"

Client	*hiddenc[MAXHIDDEN];

int	numhidden;

int virt;
int reversehide = 1;

Client * currents[NUMVIRTUALS] =
{
	NULL, NULL, NULL, NULL
};

char	*b2items[NUMVIRTUALS+1] =
{
	"One",
	"Two",
	"Three",
	"Four",
	"Five",
	"Six",
	"Seven",
	"Eight",
	"Nine",
	"Ten",
	"Eleven",
	"Twelve",
	0
};

Menu b2menu =
{
	b2items
};

char	*b3items[B3FIXED+MAXHIDDEN+1] =
{
	"New",
	"Reshape",
	"Move",
	"Delete",
	"Hide",
	0
};

enum
{
	New,
	Reshape,
	Move,
	Delete,
	Hide
};

Menu	b3menu =
{
	b3items
};

Menu	egg =
{
	version
};

void
button(XButtonEvent *e)
{
	int n, shift;
	Client *c;
	Window dw;
	ScreenInfo *s;

	curtime = e->time;
	s = getscreen(e->root);
	if(s == 0)
		return;
	c = getclient(e->window, 0);
	if(c){
		if(debug) fprintf(stderr, "but: e x=%d y=%d c x=%d y=%d dx=%d dy=%d BORDR %d\n",
				e->x, e->y, c->x, c->y, c->dx, c->dy, BORDER);
		if(borderorient(c, e->x, e->y) != BorderUnknown){
			switch (e->button){
			case Button1:
			case Button2:
				reshape(c, e->button, pull, e);
				return;
			case Button3:
				move(c, Button3);
				return;
			default:
				return;
			}
		}
		e->x += c->x - BORDER;
		e->y += c->y - BORDER;
	} else if(e->window != e->root){
		if(debug) fprintf(stderr, "but no client: e x=%d y=%d\n",
				e->x, e->y);
		XTranslateCoordinates(dpy, e->window, s->root, e->x, e->y,
				&e->x, &e->y, &dw);
	}
	switch (e->button){
	case Button1:
		if(c){
			XMapRaised(dpy, c->parent);
			top(c);
			active(c);
		}
		return;
	case Button2:
		if(c){
			XMapRaised(dpy, c->parent);
			active(c);
			XAllowEvents (dpy, ReplayPointer, curtime);
		} else if((e->state&(ShiftMask|ControlMask))==(ShiftMask|ControlMask)){
			menuhit(e, &egg);
		} else if(numvirtuals > 1 && (n = menuhit(e, &b2menu)) > -1)
				button2(n);
		return;
	case Button3:
		break;
	case Button4:
		/* scroll up changes to previous virtual screen */
		if(!c && e->type == ButtonPress)
			if(numvirtuals > 1 && virt > 0)
				switch_to(virt - 1);
		return;
	case Button5:
		/* scroll down changes to next virtual screen */
		if(!c && e->type == ButtonPress)
			if(numvirtuals > 1 && virt < numvirtuals - 1)
				switch_to(virt + 1);
		return;
	default:
		return;
	}

	if(current && current->screen == s)
		cmapnofocus(s);
	switch (n = menuhit(e, &b3menu)){
	case New:
		spawn(s);
		break;
	case Reshape:
		reshape(selectwin(1, 0, s), Button3, sweep, 0);
		break;
	case Move:
		move(selectwin(0, 0, s), Button3);
		break;
	case Delete:
		shift = 0;
		c = selectwin(1, &shift, s);
		delete(c, shift);
		break;
	case Hide:
		hide(selectwin(1, 0, s));
		break;
	default:	/* unhide window */
		unhide(n - B3FIXED, 1);
		break;
	case -1:	/* nothing */
		break;
	}
	if(current && current->screen == s)
		cmapfocus(current);
}

void
spawn(ScreenInfo *s)
{
	/*
	 * ugly dance to cause sweeping for terminals.
	 * the very next window created will require sweeping.
	 * hope it's created by the program we're about to
	 * exec!
	 */
	isNew = 1;
	/*
	 * ugly dance to avoid leaving zombies. Could use SIGCHLD,
	 * but it's not very portable.
	 */
	if(fork() == 0){
		if(fork() == 0){
			close(ConnectionNumber(dpy));
			if(s->display[0] != '\0')
				putenv(s->display);
			signal(SIGINT, SIG_DFL);
			signal(SIGTERM, SIG_DFL);
			signal(SIGHUP, SIG_DFL);
			if(termprog != NULL){
				execl(shell, shell, "-c", termprog, (char*)0);
				fprintf(stderr, "rio: exec %s", shell);
				perror(" failed");
			}
			execlp("9term", "9term", scrolling ? "-ws" : "-w", (char*)0);
			execlp("xterm", "xterm", "-ut", (char*)0);
			perror("rio: exec 9term/xterm failed");
			exit(1);
		}
		exit(0);
	}
	wait((int *) 0);
}

void
reshape(Client *c, int but, int (*fn)(Client*, int, XButtonEvent *), XButtonEvent *e)
{
	int odx, ody;

	if(c == 0)
		return;
	odx = c->dx;
	ody = c->dy;
	if(fn(c, but, e) == 0)
		return;
	active(c);
	top(c);
	XRaiseWindow(dpy, c->parent);
	XMoveResizeWindow(dpy, c->parent, c->x-BORDER, c->y-BORDER,
					c->dx+2*BORDER, c->dy+2*BORDER);
	if(c->dx == odx && c->dy == ody)
		sendconfig(c);
	else
		XMoveResizeWindow(dpy, c->window, BORDER, BORDER, c->dx, c->dy);
}

void
move(Client *c, int but)
{
	if(c == 0)
		return;
	if(drag(c, but) == 0)
		return;
	active(c);
	top(c);
	XRaiseWindow(dpy, c->parent);
	XMoveWindow(dpy, c->parent, c->x-BORDER, c->y-BORDER);
	sendconfig(c);
}

void
delete(Client *c, int shift)
{
	if(c == 0)
		return;
	if((c->proto & Pdelete) && !shift)
		sendcmessage(c->window, wm_protocols, wm_delete, 0, 0);
	else
		XKillClient(dpy, c->window);		/* let event clean up */
}

void
hide(Client *c)
{
	if(c == 0 || numhidden == MAXHIDDEN)
		return;
	if(hidden(c)){
		fprintf(stderr, "rio: already hidden: %s\n", c->label);
		return;
	}
	XUnmapWindow(dpy, c->parent);
	XUnmapWindow(dpy, c->window);
	setstate(c, IconicState);
	if(c == current)
		nofocus();
	if(reversehide){
		memmove(hiddenc+1, hiddenc, numhidden*sizeof hiddenc[0]);
		memmove(b3items+B3FIXED+1, b3items+B3FIXED, numhidden*sizeof b3items[0]);
		hiddenc[0] = c;
		b3items[B3FIXED] = c->label;
	}else{
		hiddenc[numhidden] = c;
		b3items[B3FIXED+numhidden] = c->label;
	}
	numhidden++;
	b3items[B3FIXED+numhidden] = 0;
}

void
unhide(int n, int map)
{
	Client *c;
	int i;

	if(n >= numhidden){
		fprintf(stderr, "rio: unhide: n %d numhidden %d\n", n, numhidden);
		return;
	}
	c = hiddenc[n];
	if(!hidden(c)){
		fprintf(stderr, "rio: unhide: not hidden: %s(0x%x)\n",
			c->label, (int)c->window);
		return;
	}
	c->virt = virt;

	if(map){
		XMapWindow(dpy, c->window);
		XMapRaised(dpy, c->parent);
		setstate(c, NormalState);
		active(c);
		top(c);
	}

	numhidden--;
	for(i = n; i < numhidden; i++){
		hiddenc[i] = hiddenc[i+1];
		b3items[B3FIXED+i] = b3items[B3FIXED+i+1];
	}
	b3items[B3FIXED+numhidden] = 0;
}

void
unhidec(Client *c, int map)
{
	int i;

	for(i = 0; i < numhidden; i++)
		if(c == hiddenc[i]){
			unhide(i, map);
			return;
		}
	fprintf(stderr, "rio: unhidec: not hidden: %s(0x%x)\n",
		c->label, (int)c->window);
}

void
renamec(Client *c, char *name)
{
	int i;

	if(name == 0)
		name = "???";
	c->label = name;
	if(!hidden(c))
		return;
	for(i = 0; i < numhidden; i++)
		if(c == hiddenc[i]){
			b3items[B3FIXED+i] = name;
			return;
		}
}

void
button2(int n)
{
	switch_to(n);
	if(current)
		cmapfocus(current);
}

void
switch_to_c(int n, Client *c)
{
	if(c == 0)
		return;

	if(c->next)
		switch_to_c(n, c->next);

	if(c->parent == DefaultRootWindow(dpy))
		return;

	if(c->virt != virt && c->state == NormalState){
		XUnmapWindow(dpy, c->parent);
		XUnmapWindow(dpy, c->window);
		setstate(c, IconicState);
		if(c == current)
			nofocus();
	} else if(c->virt == virt && c->state == IconicState){
		int i;

		for(i = 0; i < numhidden; i++)
			if(c == hiddenc[i])
				break;

		if(i == numhidden){
			XMapWindow(dpy, c->window);
			XMapWindow(dpy, c->parent);
			setstate(c, NormalState);
			if(currents[virt] == c)
				active(c);
		}
	}
}

void
switch_to(int n)
{
	if(n == virt)
		return;
	currents[virt] = current;
	virt = n;

	/* redundant when called from a menu switch
	 * but needed for scroll-button switches
	 */
	b2menu.lasthit = n;

	switch_to_c(n, clients);
	current = currents[virt];
}

void
initb2menu(int n)
{
	b2items[n] = 0;
}
