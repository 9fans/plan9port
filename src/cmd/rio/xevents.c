/*
 * Original code posted to comp.sources.x (see printevent.c).
 * Modifications by Russ Cox <rsc@swtch.com>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <X11/Intrinsic.h>
#include "printevent.h"

int
main(int argc, char **argv)
{
	int screen;
	Display *dpy;
	Window window;
	XEvent event;

	if (!(dpy = XOpenDisplay(""))) {
		printf("Failed to open display...\n");
		exit(1);
	}

	screen = DefaultScreen(dpy);

	window = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 100, 100,
		300, 200, 2, BlackPixel(dpy, screen), WhitePixel(dpy, screen));

	XSelectInput(dpy, window, KeyPressMask | KeyReleaseMask | ButtonPressMask |
		ButtonReleaseMask | EnterWindowMask | LeaveWindowMask |
		PointerMotionMask | PointerMotionHintMask | Button1MotionMask |
		Button2MotionMask | Button3MotionMask | Button4MotionMask |
		Button5MotionMask | ButtonMotionMask | KeymapStateMask |
		ExposureMask | VisibilityChangeMask | StructureNotifyMask |
		SubstructureNotifyMask | SubstructureRedirectMask | FocusChangeMask |
		PropertyChangeMask | ColormapChangeMask | OwnerGrabButtonMask);

	XMapWindow(dpy, window);

	for(;;){
		XNextEvent(dpy, &event);
		printevent(&event);
	}
}
