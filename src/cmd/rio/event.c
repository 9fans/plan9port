/* Copyright (c) 1994-1996 David Hogan, see README for licence details */
#include <stdio.h>
#include <stdlib.h>
#include <X11/X.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include "dat.h"
#include "fns.h"
#include "patchlevel.h"

void
mainloop(int shape_event)
{
	XEvent ev;

	for(;;){
		getevent(&ev);

#ifdef	DEBUG_EV
		if(debug){
			ShowEvent(&ev);
			printf("\n");
		}
#endif
		switch (ev.type){
		default:
#ifdef	SHAPE
			if(shape && ev.type == shape_event)
				shapenotify((XShapeEvent *)&ev);
			else
#endif
				fprintf(stderr, "rio: unknown ev.type %d\n", ev.type);
			break;
		case KeyPress:
			keypress(&ev.xkey);
			break;
		case KeyRelease:
			keyrelease(&ev.xkey);
			break;
		case ButtonPress:
			button(&ev.xbutton);
			break;
		case ButtonRelease:
			break;
		case MapRequest:
			mapreq(&ev.xmaprequest);
			break;
		case ConfigureRequest:
			configurereq(&ev.xconfigurerequest);
			break;
		case CirculateRequest:
			circulatereq(&ev.xcirculaterequest);
			break;
		case UnmapNotify:
			unmap(&ev.xunmap);
			break;
		case CreateNotify:
			newwindow(&ev.xcreatewindow);
			break;
		case DestroyNotify:
			destroy(ev.xdestroywindow.window);
			break;
		case ClientMessage:
			clientmesg(&ev.xclient);
			break;
		case ColormapNotify:
			cmap(&ev.xcolormap);
			break;
		case PropertyNotify:
			property(&ev.xproperty);
			break;
		case SelectionClear:
			fprintf(stderr, "rio: SelectionClear (this should not happen)\n");
			break;
		case SelectionNotify:
			fprintf(stderr, "rio: SelectionNotify (this should not happen)\n");
			break;
		case SelectionRequest:
			fprintf(stderr, "rio: SelectionRequest (this should not happen)\n");
			break;
		case EnterNotify:
			enter(&ev.xcrossing);
			break;
		case LeaveNotify:
			leave(&ev.xcrossing);
			break;
		case ReparentNotify:
			reparent(&ev.xreparent);
			break;
		case FocusIn:
			focusin(&ev.xfocus);
			break;
		case MotionNotify:
			motionnotify(&ev.xmotion);
			break;
		case Expose:
		case NoExpose:
		case FocusOut:
		case ConfigureNotify:
		case MapNotify:
		case MappingNotify:
		case GraphicsExpose:
			/* not interested */
			trace("ignore", 0, &ev);
			break;
		}
	}
}


void
configurereq(XConfigureRequestEvent *e)
{
	XWindowChanges wc;
	Client *c;

	/* we don't set curtime as nothing here uses it */
	c = getclient(e->window, 0);
	trace("configurereq", c, e);

	e->value_mask &= ~CWSibling;

	if(c){
		if(e->value_mask & CWX)
			c->x = e->x;
		if(e->value_mask & CWY)
			c->y = e->y;
		if(e->value_mask & CWWidth)
			c->dx = e->width;
		if(e->value_mask & CWHeight)
			c->dy = e->height;
		if(e->value_mask & CWBorderWidth)
			c->border = e->border_width;

		if(c->dx >= c->screen->width && c->dy >= c->screen->height)
			c->border = 0;
		else
			c->border = BORDER;

		if(e->value_mask & CWStackMode){
			if(e->detail == Above)
				top(c);
			else
				e->value_mask &= ~CWStackMode;
		}
		e->value_mask |= CWX|CWY|CWHeight|CWWidth;

		if(c->parent != c->screen->root && c->window == e->window){
			wc.x = c->x - c->border;
			wc.y = c->y - c->border;
			wc.width = c->dx+c->border+c->border;
			wc.height = c->dy+c->border+c->border;
			wc.border_width = 1;
			wc.sibling = None;
			wc.stack_mode = e->detail;
			XConfigureWindow(dpy, c->parent, e->value_mask, &wc);

			if(e->value_mask & CWStackMode){
				top(c);
				active(c);
			}
		}
	}

	if(c && c->parent != c->screen->root){
		wc.x = c->border;
		wc.y = c->border;
	}else {
		wc.x = c->x;
		wc.y = c->y;
	}
	wc.width = c->dx;
	wc.height = c->dy;
	wc.border_width = 0;
	wc.sibling = None;
	wc.stack_mode = Above;
	e->value_mask &= ~CWStackMode;
	e->value_mask |= CWBorderWidth;
	XConfigureWindow(dpy, c->window, e->value_mask, &wc);
}

void
mapreq(XMapRequestEvent *e)
{
	Client *c;
	int i;

	curtime = CurrentTime;
	c = getclient(e->window, 0);
	trace("mapreq", c, e);

	if(c == 0 || c->window != e->window){
		/* workaround for stupid NCDware */
		fprintf(stderr, "rio: bad mapreq c %p w %x, rescanning\n",
			(void*)c, (int)e->window);
		for(i = 0; i < num_screens; i++)
			scanwins(&screens[i]);
		c = getclient(e->window, 0);
		if(c == 0 || c->window != e->window){
			fprintf(stderr, "rio: window not found after rescan\n");
			return;
		}
	}

	switch (c->state){
	case WithdrawnState:
		if(c->parent == c->screen->root){
			if(!manage(c, 0))
				return;
			break;
		}
		XReparentWindow(dpy, c->window, c->parent, BORDER-1, BORDER-1);
		XAddToSaveSet(dpy, c->window);
		/* fall through... */
	case NormalState:
		XMapWindow(dpy, c->window);
		XMapRaised(dpy, c->parent);
		top(c);
		setstate(c, NormalState);
		if(c->trans != None && current && c->trans == current->window)
				active(c);
		break;
	case IconicState:
		unhidec(c, 1);
		break;
	}
}

void
unmap(XUnmapEvent *e)
{
	Client *c;

	curtime = CurrentTime;
	c = getclient(e->window, 0);
	if(c){
		switch (c->state){
		case IconicState:
			if(e->send_event){
				unhidec(c, 0);
				withdraw(c);
			}
			break;
		case NormalState:
			if(c == current)
				nofocus();
			if(!c->reparenting)
				withdraw(c);
			break;
		}
		c->reparenting = 0;
	}
}

void
circulatereq(XCirculateRequestEvent *e)
{
	fprintf(stderr, "It must be the warlock Krill!\n");  /* ☺ */
}

void
newwindow(XCreateWindowEvent *e)
{
	Client *c;
	ScreenInfo *s;

	/* we don't set curtime as nothing here uses it */
	if(e->override_redirect)
		return;
	c = getclient(e->window, 1);
	if(c && c->window == e->window && (s = getscreen(e->parent))){
		c->x = e->x;
		c->y = e->y;
		c->dx = e->width;
		c->dy = e->height;
		c->border = e->border_width;
		c->screen = s;
		if(c->parent == None)
			c->parent = c->screen->root;
	}
}

void
destroy(Window w)
{
	int i;
	Client *c;

	curtime = CurrentTime;
	c = getclient(w, 0);
	if(c == 0)
		return;

	if(numvirtuals > 1)
		for(i=0; i<numvirtuals; i++)
			if(currents[i] == c)
				currents[i] = 0;

	rmclient(c);

	/* flush any errors generated by the window's sudden demise */
	ignore_badwindow = 1;
	XSync(dpy, False);
	ignore_badwindow = 0;
}

void
clientmesg(XClientMessageEvent *e)
{
	Client *c;

	curtime = CurrentTime;
	if(e->message_type == exit_rio){
		cleanup();
		exit(0);
	}
	if(e->message_type == restart_rio){
		fprintf(stderr, "*** rio restarting ***\n");
		cleanup();
		execvp(myargv[0], myargv);
		perror("rio: exec failed");
		exit(1);
	}
	if(e->message_type == wm_protocols)
		return;
	if(e->message_type == wm_change_state){
		c = getclient(e->window, 0);
		if(e->format == 32 && e->data.l[0] == IconicState && c != 0){
			if(normal(c))
				hide(c);
		}
		else
			fprintf(stderr, "rio: WM_CHANGE_STATE: format %d data %d w 0x%x\n",
				(int)e->format, (int)e->data.l[0], (int)e->window);
		return;
	}
	if(e->message_type == wm_state){
//		c = getclient(e->window, 0);
//		if(e->format == 32 && e->data.l[1] == wm_state_fullscreen){
//		}else
		fprintf(stderr, "rio: WM_STATE: format %d data %d %d w 0x%x\n",
			(int)e->format, (int)e->data.l[0], (int)e->data.l[1],
			(int)e->window);
		return;
	}
	fprintf(stderr, "rio: strange ClientMessage, type 0x%x window 0x%x\n",
		(int)e->message_type, (int)e->window);
}

void
cmap(XColormapEvent *e)
{
	Client *c;
	int i;

	/* we don't set curtime as nothing here uses it */
	if(e->new){
		c = getclient(e->window, 0);
		if(c){
			c->cmap = e->colormap;
			if(c == current)
				cmapfocus(c);
		}
		else
			for(c = clients; c; c = c->next){
				for(i = 0; i < c->ncmapwins; i++)
					if(c->cmapwins[i] == e->window){
						c->wmcmaps[i] = e->colormap;
						if(c == current)
							cmapfocus(c);
						return;
					}
			}
	}
}

void
property(XPropertyEvent *e)
{
	Atom a;
	int delete;
	Client *c;
	long msize;

	/* we don't set curtime as nothing here uses it */
	a = e->atom;
	delete = (e->state == PropertyDelete);
	c = getclient(e->window, 0);
	if(c == 0)
		return;

	switch (a){
	case XA_WM_ICON_NAME:
		if(c->iconname != 0)
			XFree((char*) c->iconname);
		c->iconname = delete ? 0 : getprop(c->window, a);
		setlabel(c);
		renamec(c, c->label);
		return;
	case XA_WM_NAME:
		if(c->name != 0)
			XFree((char*) c->name);
		c->name = delete ? 0 : getprop(c->window, a);
		setlabel(c);
		renamec(c, c->label);
		return;
	case XA_WM_TRANSIENT_FOR:
		gettrans(c);
		return;
	case XA_WM_HINTS:
	case XA_WM_SIZE_HINTS:
	case XA_WM_ZOOM_HINTS:
		/* placeholders to not forget.  ignore for now.  -Axel */
		return;
	case XA_WM_NORMAL_HINTS:
		if(XGetWMNormalHints(dpy, c->window, &c->size, &msize) == 0 || c->size.flags == 0)
			c->size.flags = PSize;	/* not specified - punt */
		return;
	}
	if(a == _rio_hold_mode){
		c->hold = getiprop(c->window, _rio_hold_mode);
		if(c == current)
			draw_border(c, 1);
	}
	else if(a == wm_colormaps){
		getcmaps(c);
		if(c == current)
			cmapfocus(c);
	}
}

void
reparent(XReparentEvent *e)
{
	Client *c;
	XWindowAttributes attr;
	ScreenInfo *s;

	/* we don't set curtime as nothing here uses it */
	if(!getscreen(e->event) || e->override_redirect)
		return;
	if((s = getscreen(e->parent)) != 0){
		c = getclient(e->window, 1);
		if(c != 0 && (c->dx == 0 || c->dy == 0)){
			/* flush any errors */
			ignore_badwindow = 1;
			XGetWindowAttributes(dpy, c->window, &attr);
			XSync(dpy, False);
			ignore_badwindow = 0;

			c->x = attr.x;
			c->y = attr.y;
			c->dx = attr.width;
			c->dy = attr.height;
			c->border = attr.border_width;
			c->screen = s;
			if(c->parent == None)
				c->parent = c->screen->root;
		}
	}
	else {
		c = getclient(e->window, 0);
		if(c != 0 && (c->parent == c->screen->root || withdrawn(c)))
			rmclient(c);
	}
}

#ifdef	SHAPE
void
shapenotify(XShapeEvent *e)
{
	Client *c;

	/* we don't set curtime as nothing here uses it */
	c = getclient(e->window, 0);
	if(c == 0)
		return;

	setshape(c);
}
#endif

void
enter(XCrossingEvent *e)
{
	Client *c;

	curtime = e->time;
	if(!ffm)
	if(e->mode != NotifyGrab || e->detail != NotifyNonlinearVirtual)
		return;
	c = getclient(e->window, 0);
	if(c != 0 && c != current){
		/* someone grabbed the pointer; make them current */
		if(!ffm)
			XMapRaised(dpy, c->parent);
		top(c);
		active(c);
	}
}

void
leave(XCrossingEvent *e)
{
	Client *c;

	c = getclient(e->window, 0);
	if(c)
		XUndefineCursor(dpy, c->parent);
/* 	XDefineCursor(dpy, c->parent, c->screen->arrow); */
}

void
focusin(XFocusChangeEvent *e)
{
	Client *c;

	curtime = CurrentTime;
	if(e->detail != NotifyNonlinearVirtual)
		return;
	c = getclient(e->window, 0);
	if(c != 0 && c->window == e->window && c != current){
		/* someone grabbed keyboard or seized focus; make them current */
		XMapRaised(dpy, c->parent);
		top(c);
		active(c);
	}
}

BorderOrient
borderorient(Client *c, int x, int y)
{
	if(x <= BORDER){
		if(y <= CORNER){
			if(debug) fprintf(stderr, "topleft\n");
			return BorderWNW;
		}
		if(y >= (c->dy + 2*BORDER) - CORNER){
			if(debug) fprintf(stderr, "botleft\n");
			return BorderWSW;
		}
		if(y > CORNER &&
			y < (c->dy + 2*BORDER) - CORNER){
			if(debug) fprintf(stderr, "left\n");
			return BorderW;
		}
	} else if(x <= CORNER){
		if(y <= BORDER){
			if(debug) fprintf(stderr, "topleft\n");
			return BorderNNW;
		}
		if  (y >= (c->dy + BORDER)){
			if(debug) fprintf(stderr, "botleft\n");
			return BorderSSW;
		}
	} else if(x >= (c->dx + BORDER)){
		if(y <= CORNER){
			if(debug) fprintf(stderr, "topright\n");
			return BorderENE;
		}
		if(y >= (c->dy + 2*BORDER) - CORNER){
			if(debug) fprintf(stderr, "botright\n");
			return BorderESE;
		}
		if(y > CORNER &&
			y < (c->dy + 2*BORDER) - CORNER){
			if(debug) fprintf(stderr, "right\n");
			return BorderE;
		}
	} else if(x >= (c->dx + 2*BORDER) - CORNER){
		if(y <= BORDER){
			if(debug) fprintf(stderr, "topright\n");
			return BorderNNE;
		}
		if  (y >= (c->dy + BORDER)){
			if(debug) fprintf(stderr, "botright\n");
			return BorderSSE;
		}
	} else if(x > CORNER &&
			x < (c->dx + 2*BORDER) - CORNER){
		if(y <= BORDER){
			if(debug) fprintf(stderr, "top\n");
			return BorderN;
		}
		if(y >= (c->dy + BORDER)){
			if(debug) fprintf(stderr, "bot\n");
			return BorderS;
		}
	}
	return BorderUnknown;
}

void
motionnotify(XMotionEvent *e)
{
	Client *c;
	BorderOrient bl;

	c = getclient(e->window, 0);
	if(c){
		bl = borderorient(c, e->x, e->y);
		if(bl == BorderUnknown)
			XUndefineCursor(dpy, c->parent);
		else
			XDefineCursor(dpy, c->parent, c->screen->bordcurs[bl]);
	}
}
