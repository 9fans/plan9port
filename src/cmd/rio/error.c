/* Copyright (c) 1994-1996 David Hogan, see README for licence details */
#include <stdio.h>
#include <stdlib.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <X11/extensions/shape.h>
#include "dat.h"
#include "fns.h"

int 	ignore_badwindow;

void
fatal(char *s)
{
	fprintf(stderr, "rio: ");
	perror(s);
	fprintf(stderr, "\n");
	exit(1);
}

int
handler(Display *d, XErrorEvent *e)
{
	char msg[80], req[80], number[80];

	if(initting && (e->request_code == X_ChangeWindowAttributes) && (e->error_code == BadAccess)){
		fprintf(stderr, "rio: it looks like there's already a window manager running;  rio not started\n");
		exit(1);
	}

	if(ignore_badwindow && (e->error_code == BadWindow || e->error_code == BadColor))
		return 0;

	XGetErrorText(d, e->error_code, msg, sizeof(msg));
	sprintf(number, "%d", e->request_code);
	XGetErrorDatabaseText(d, "XRequest", number, "", req, sizeof(req));
	if(req[0] == '\0')
		sprintf(req, "<request-code-%d>", (int)e->request_code);

	fprintf(stderr, "rio: %s(0x%x): %s\n", req, (int)e->resourceid, msg);

	if(initting){
		fprintf(stderr, "rio: failure during initialisation; aborting\n");
		exit(1);
	}
	return 0;
}

void
graberror(char *f, int err)
{
#ifdef	DEBUG	/* sick of "bug" reports; grab errors "just happen" */
	char *s;

	switch (err){
	case GrabNotViewable:
		s = "not viewable";
		break;
	case AlreadyGrabbed:
		s = "already grabbed";
		break;
	case GrabFrozen:
		s = "grab frozen";
		break;
	case GrabInvalidTime:
		s = "invalid time";
		break;
	case GrabSuccess:
		return;
	default:
		fprintf(stderr, "rio: %s: grab error: %d\n", f, err);
		return;
	}
	fprintf(stderr, "rio: %s: grab error: %s\n", f, s);
#endif
}

#ifdef	DEBUG_EV
#include "showevent/ShowEvent.c"
#endif

#ifdef	DEBUG

void
dotrace(char *s, Client *c, XEvent *e)
{
	if(debug == 0)
		return;
setbuf(stdout, 0);
	fprintf(stderr, "rio: %s: c=%p", s, (void*)c);
	if(c)
		fprintf(stderr, " x %d y %d dx %d dy %d w 0x%x parent 0x%x", c->x, c->y, c->dx, c->dy, (int)c->window, (int)c->parent);
#ifdef	DEBUG_EV
	if(e){
		fprintf(stderr, "\n\t");
		ShowEvent(e);
	}
#endif
	fprintf(stderr, "\n");
}
#endif
