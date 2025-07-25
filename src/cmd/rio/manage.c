/*
 * Window management.
 */

/* Copyright (c) 1994-1996 David Hogan, see README for licence details */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <X11/X.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include "dat.h"
#include "fns.h"

int isNew;

int
manage(Client *c, int mapped)
{
	int fixsize, dohide, doreshape, state;
	long msize;
	XClassHint class;
	XWMHints *hints;
	XSetWindowAttributes attrs;

	trace("manage", c, 0);
	XSelectInput(dpy, c->window, ColormapChangeMask | EnterWindowMask | PropertyChangeMask | FocusChangeMask | KeyPressMask);

	/* Get loads of hints */

	if(XGetClassHint(dpy, c->window, &class) != 0){	/* ``Success'' */
		c->instance = class.res_name;
		c->class = class.res_class;
		c->is9term = 0;
		if(isNew){
			c->is9term = strstr(c->class, "term") || strstr(c->class, "Term");
			isNew = 0;
		}
	}
	else {
		c->instance = 0;
		c->class = 0;
		c->is9term = 0;
	}
	c->iconname = getprop(c->window, XA_WM_ICON_NAME);
	c->name = getprop(c->window, XA_WM_NAME);
	setlabel(c);

	hints = XGetWMHints(dpy, c->window);
	if(XGetWMNormalHints(dpy, c->window, &c->size, &msize) == 0 || c->size.flags == 0)
		c->size.flags = PSize;		/* not specified - punt */

	getcmaps(c);
	getproto(c);
	gettrans(c);
	if(c->is9term)
		c->hold = getiprop(c->window, _rio_hold_mode);

	/* Figure out what to do with the window from hints */

	if(!getstate(c->window, &state))
		state = hints ? hints->initial_state : NormalState;
	dohide = (state == IconicState);

	fixsize = 0;
	if((c->size.flags & (USSize|PSize)))
		fixsize = 1;
	if((c->size.flags & (PMinSize|PMaxSize)) == (PMinSize|PMaxSize) && c->size.min_width == c->size.max_width && c->size.min_height == c->size.max_height)
		fixsize = 1;
	doreshape = !mapped;
	if(fixsize){
		if(c->size.flags & USPosition)
			doreshape = 0;
		if(dohide && (c->size.flags & PPosition))
			doreshape = 0;
		if(c->trans != None)
			doreshape = 0;
	}
	if(c->is9term)
		fixsize = 0;
	if(c->size.flags & PBaseSize){
		c->min_dx = c->size.base_width;
		c->min_dy = c->size.base_height;
	}
	else if(c->size.flags & PMinSize){
		c->min_dx = c->size.min_width;
		c->min_dy = c->size.min_height;
	}
	else if(c->is9term){
		c->min_dx = 100;
		c->min_dy = 50;
	}
	else
		c->min_dx = c->min_dy = 0;

	if(hints)
		XFree(hints);

	/* Now do it!!! */

	if(doreshape){
		if(0) fprintf(stderr, "in doreshape is9term=%d fixsize=%d, x=%d, y=%d, min_dx=%d, min_dy=%d, dx=%d, dy=%d\n",
				c->is9term, fixsize, c->x, c->y, c->min_dx, c->min_dy, c->dx, c->dy);
		if(current && current->screen == c->screen)
			cmapnofocus(c->screen);
		if(!c->is9term && c->x==0 && c->y==0){
			static int nwin;

			c->x = 20*nwin+BORDER;
			c->y = 20*nwin+BORDER;
			nwin++;
			nwin %= 10;
		}

		if(c->is9term && !(fixsize ? drag(c, Button3) : sweep(c, Button3, 0))){
			ScreenInfo *screen = c->screen;
			XKillClient(dpy, c->window);
			rmclient(c);
			if(current && current->screen == screen)
				cmapfocus(current);
			return 0;
		}
	}

	attrs.border_pixel =  c->screen->black;
	attrs.background_pixel =  c->screen->white;
	attrs.colormap = c->screen->def_cmap;
	c->parent = XCreateWindow(dpy, c->screen->root,
			c->x - BORDER, c->y - BORDER,
			c->dx + 2*BORDER, c->dy + 2*BORDER,
			0,
			c->screen->depth,
			CopyFromParent,
			c->screen->vis,
			CWBackPixel | CWBorderPixel | CWColormap,
			&attrs);

	XSelectInput(dpy, c->parent, SubstructureRedirectMask | SubstructureNotifyMask|ButtonPressMask| PointerMotionMask|LeaveWindowMask|KeyPressMask);
	if(mapped)
		c->reparenting = 1;
	if(doreshape && !fixsize)
		XResizeWindow(dpy, c->window, c->dx, c->dy);
	XSetWindowBorderWidth(dpy, c->window, 0);

	/*
	  * To have something more than only a big white or black border
	  * XXX should replace this by a pattern in the white or black
	  * such that we can see the border also if all our
	  * windows are black and/or white
	  * (black (or white)  border around black (or white) window
	  *  is not very helpful.
	  */
	if(c->screen->depth <= 8){
		XSetWindowBorderWidth(dpy, c->parent, 1);
	}

	XReparentWindow(dpy, c->window, c->parent, BORDER, BORDER);
#ifdef	SHAPE
	if(shape){
		XShapeSelectInput(dpy, c->window, ShapeNotifyMask);
		ignore_badwindow = 1;		/* magic */
		setshape(c);
		ignore_badwindow = 0;
	}
#endif
	XAddToSaveSet(dpy, c->window);
	if(dohide)
		hide(c);
	else {
		XMapWindow(dpy, c->window);
		XMapWindow(dpy, c->parent);
		XUnmapWindow(dpy, c->screen->sweepwin);
		if(nostalgia || doreshape)
			active(c);
		else if(c->trans != None && current && current->window == c->trans)
			active(c);
		else
			setactive(c, 0);
		setstate(c, NormalState);
	}
	if(current && (current != c))
		cmapfocus(current);
	c->init = 1;

	/*
	 * If we swept the window, let's send a resize event to the
	 * guy who just got resized.  It's not clear whether the apps
	 * should notice their new size via other means.  Try as I might,
	 * I can't find a way to have them notice during initdraw, so
	 * I solve the problem this way instead.		-rsc
	 */
	if(c->is9term)
		sendconfig(c);
	return 1;
}

void
scanwins(ScreenInfo *s)
{
	unsigned int i, nwins;
	Client *c;
	Window dw1, dw2, *wins;
	XWindowAttributes attr;

	XQueryTree(dpy, s->root, &dw1, &dw2, &wins, &nwins);
	for(i = 0; i < nwins; i++){
		XGetWindowAttributes(dpy, wins[i], &attr);
		if(attr.override_redirect || wins[i] == s->menuwin)
			continue;
		c = getclient(wins[i], 1);
		if(c != 0 && c->window == wins[i] && !c->init){
			c->x = attr.x;
			c->y = attr.y;
			c->dx = attr.width;
			c->dy = attr.height;
			c->border = attr.border_width;
			c->screen = s;
			c->parent = s->root;
			if(attr.map_state == IsViewable)
				manage(c, 1);
		}
	}
	XFree((void *) wins);	/* cast is to shut stoopid compiler up */
}

void
gettrans(Client *c)
{
	Window trans;

	trans = None;
	if(XGetTransientForHint(dpy, c->window, &trans) != 0)
		c->trans = trans;
	else
		c->trans = None;
}

void
withdraw(Client *c)
{
	XUnmapWindow(dpy, c->parent);
	XReparentWindow(dpy, c->window, c->screen->root, c->x, c->y);
	XRemoveFromSaveSet(dpy, c->window);
	setstate(c, WithdrawnState);

	/* flush any errors */
	ignore_badwindow = 1;
	XSync(dpy, False);
	ignore_badwindow = 0;
}

static void
installcmap(ScreenInfo *s, Colormap cmap)
{
	if(cmap == None)
		XInstallColormap(dpy, s->def_cmap);
	else
		XInstallColormap(dpy, cmap);
}

void
cmapfocus(Client *c)
{
	int i, found;
	Client *cc;

	if(c == 0)
		return;
	else if(c->ncmapwins != 0){
		found = 0;
		for(i = c->ncmapwins-1; i >= 0; i--){
			installcmap(c->screen, c->wmcmaps[i]);
			if(c->cmapwins[i] == c->window)
				found++;
		}
		if(!found)
			installcmap(c->screen, c->cmap);
	}
	else if(c->trans != None && (cc = getclient(c->trans, 0)) != 0 && cc->ncmapwins != 0)
		cmapfocus(cc);
	else
		installcmap(c->screen, c->cmap);
}

void
cmapnofocus(ScreenInfo *s)
{
	installcmap(s, None);
}

void
getcmaps(Client *c)
{
	int n, i;
	Window *cw;
	XWindowAttributes attr;

	if(!c->init){
		ignore_badwindow = 1;
		XGetWindowAttributes(dpy, c->window, &attr);
		c->cmap = attr.colormap;
		XSync(dpy, False);
		ignore_badwindow = 0;
	}

	n = _getprop(c->window, wm_colormaps, XA_WINDOW, 100L, (void*)&cw);
	if(c->ncmapwins != 0){
		XFree((char *)c->cmapwins);
		free((char *)c->wmcmaps);
	}
	if(n <= 0){
		c->ncmapwins = 0;
		return;
	}

	c->ncmapwins = n;
	c->cmapwins = cw;

	c->wmcmaps = (Colormap*)malloc(n*sizeof(Colormap));
	if (!c->wmcmaps){
		fprintf(stderr, "rio: Failed to allocate memory\n");
		exit(1);
	}

	for(i = 0; i < n; i++){
		if(cw[i] == c->window)
			c->wmcmaps[i] = c->cmap;
		else {
			/* flush any errors (e.g., caused by mozilla tabs) */
			ignore_badwindow = 1;
			XSelectInput(dpy, cw[i], ColormapChangeMask);
			XGetWindowAttributes(dpy, cw[i], &attr);
			c->wmcmaps[i] = attr.colormap;
			XSync(dpy, False);
			ignore_badwindow = 0;
		}
	}
}

void
setlabel(Client *c)
{
	char *label, *p;

	if(c->iconname != 0)
		label = c->iconname;
	else if(c->name != 0)
		label = c->name;
	else if(c->instance != 0)
		label = c->instance;
	else if(c->class != 0)
		label = c->class;
	else
		label = "no label";
	if((p = index(label, ':')) != 0)
		*p = '\0';
	c->label = label;
}

#ifdef	SHAPE
void
setshape(Client *c)
{
	int n, order;
	XRectangle *rect;

	/* don't try to add a border if the window is non-rectangular */
	rect = XShapeGetRectangles(dpy, c->window, ShapeBounding, &n, &order);
	if(n > 1)
		XShapeCombineShape(dpy, c->parent, ShapeBounding, BORDER, BORDER,
			c->window, ShapeBounding, ShapeSet);
	XFree((void*)rect);
}
#endif

int
_getprop(Window w, Atom a, Atom type, long len, unsigned char **p)
{
	Atom real_type;
	int format;
	unsigned long n, extra;
	int status;

	status = XGetWindowProperty(dpy, w, a, 0L, len, False, type, &real_type, &format, &n, &extra, p);
	if(status != Success || *p == 0)
		return -1;
	if(n == 0)
		XFree((void*) *p);
	/* could check real_type, format, extra here... */
	return n;
}

char *
getprop(Window w, Atom a)
{
	unsigned char *p;

	if(_getprop(w, a, XA_STRING, 100L, &p) <= 0)
		return 0;
	return (char *)p;
}

int
get1prop(Window w, Atom a, Atom type)
{
	char **p, *x;

	if(_getprop(w, a, type, 1L, (void*)&p) <= 0)
		return 0;
	x = *p;
	XFree((void*) p);
	return (int)(uintptr_t)x;
}

Window
getwprop(Window w, Atom a)
{
	return get1prop(w, a, XA_WINDOW);
}

int
getiprop(Window w, Atom a)
{
	return get1prop(w, a, XA_INTEGER);
}

void
setstate(Client *c, int state)
{
	long data[2];

	data[0] = (long) state;
	data[1] = (long) None;

	c->state = state;
	XChangeProperty(dpy, c->window, wm_state, wm_state, 32,
		PropModeReplace, (unsigned char *)data, 2);
}

int
getstate(Window w, int *state)
{
	long *p = 0;

	if(_getprop(w, wm_state, wm_state, 2L, (void*)&p) <= 0)
		return 0;

	*state = (int) *p;
	XFree((char *) p);
	return 1;
}

void
getproto(Client *c)
{
	Atom *p;
	int i;
	long n;
	Window w;

	w = c->window;
	c->proto = 0;
	if((n = _getprop(w, wm_protocols, XA_ATOM, 20L, (void*)&p)) <= 0)
		return;

	for(i = 0; i < n; i++)
		if(p[i] == wm_delete)
			c->proto |= Pdelete;
		else if(p[i] == wm_take_focus)
			c->proto |= Ptakefocus;
		else if(p[i] == wm_lose_focus)
			c->proto |= Plosefocus;

	XFree((char *) p);
}
