/* Copyright (c) 2005 Russ Cox, see README for licence details */
#include <stdio.h>
#include <stdlib.h>
#include <X11/X.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/extensions/shape.h>
#include "dat.h"
#include "fns.h"
#include "patchlevel.h"

enum
{
	GrabAltTab,
	GrabAltAny
};

/*static int tabcode = 0x17; */
/*static int altcode = 0x40; */
/*static int pgupcode = 0x63; */
/*static int pgdowncode = 0x69; */

static void alttab(int shift);

void
keysetup(void)
{
	int i;
	int tabcode = XKeysymToKeycode(dpy, XK_Tab);

	for(i=0; i<num_screens; i++){
		XGrabKey(dpy, tabcode, Mod1Mask, screens[i].root, 0, GrabModeSync, GrabModeAsync);
		XGrabKey(dpy, tabcode, Mod1Mask|ShiftMask, screens[i].root, 0, GrabModeSync, GrabModeAsync);
	/*	XGrabKey(dpy, pgupcode, Mod1Mask, screens[i].root, 0, GrabModeSync, GrabModeAsync); */
	/*	XGrabKey(dpy, pgdowncode, Mod1Mask, screens[i].root, 0, GrabModeSync, GrabModeAsync); */
	/*	XGrabKey(dpy, altcode, 0, screens[i].root, 0, GrabModeSync, GrabModeAsync); */
	}
}

void
keypress(XKeyEvent *e)
{
	/*
	 * process key press here
	 */
	int tabcode = XKeysymToKeycode(dpy, XK_Tab);
	if(e->keycode == tabcode && (e->state&Mod1Mask) == (1<<3))
		alttab(e->state&ShiftMask);
	XAllowEvents(dpy, SyncKeyboard, e->time);
}

void
keyrelease(XKeyEvent *e)
{
	XAllowEvents(dpy, SyncKeyboard, e->time);
}

static void
alttab(int shift)
{
	shuffle(shift);
/*	fprintf(stderr, "%sTab\n", shift ? "Back" : ""); */
}
