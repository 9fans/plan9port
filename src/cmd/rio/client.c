/* Copyright (c) 1994-1996 David Hogan, see README for licence details */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "dat.h"
#include "fns.h"

Client	*clients;
Client	*current;

void
setactive(Client *c, int on)
{
/*	dbg("setactive client %x %d", c->window, c->on); */

	if(c->parent == c->screen->root)
		return;

	if(on){
		XUngrabButton(dpy, AnyButton, AnyModifier, c->parent);
		XSetInputFocus(dpy, c->window, RevertToPointerRoot, timestamp());
		if(c->proto & Ptakefocus)
			sendcmessage(c->window, wm_protocols, wm_take_focus, 0, 1);
		cmapfocus(c);
	}else{
		if(c->proto & Plosefocus)
			sendcmessage(c->window, wm_protocols, wm_lose_focus, 0, 1);
		XGrabButton(dpy, AnyButton, AnyModifier, c->parent, False,
			ButtonMask, GrabModeAsync, GrabModeSync, None, None);
	}
	draw_border(c, on);
}

void
draw_border(Client *c, int active)
{
	unsigned long pixel;

	if(active){
		if(c->hold)
			pixel = c->screen->activeholdborder;
		else
			pixel = c->screen->activeborder;
	}else{
		if(c->hold)
			pixel = c->screen->inactiveholdborder;
		else
			pixel = c->screen->inactiveborder;
	}

	if(debug) fprintf(stderr, "draw_border %p pixel %ld active %d hold %d\n", (void*)c, pixel, active, c->hold);
	XSetWindowBackground(dpy, c->parent, pixel);
	XClearWindow(dpy, c->parent);
}

void
active(Client *c)
{
	Client *cc;

	if(c == 0){
		fprintf(stderr, "rio: active(c==0)\n");
		return;
	}
	if(c == current)
		return;
	if(current){
		setactive(current, 0);
		if(current->screen != c->screen)
			cmapnofocus(current->screen);
	}
	setactive(c, 1);
	for(cc = clients; cc; cc = cc->next)
		if(cc->revert == c)
			cc->revert = c->revert;
	c->revert = current;
	while(c->revert && !normal(c->revert))
		c->revert = c->revert->revert;
	current = c;
#ifdef	DEBUG
	if(debug)
		dump_revert();
#endif
}

void
nofocus(void)
{
	static Window w = 0;
	int mask;
	XSetWindowAttributes attr;
	Client *c;

	if(current){
		setactive(current, 0);
		for(c = current->revert; c; c = c->revert)
			if(normal(c)){
				active(c);
				return;
			}
		cmapnofocus(current->screen);
		/* if no candidates to revert to, fall through */
	}
	current = 0;
	if(w == 0){
		mask = CWOverrideRedirect/*|CWColormap*/;
		attr.override_redirect = 1;
		/* attr.colormap = screens[0].def_cmap;*/
		w = XCreateWindow(dpy, screens[0].root, 0, 0, 1, 1, 0,
			0 /*screens[0].depth*/, InputOnly, 	screens[0].vis, mask, &attr);
		XMapWindow(dpy, w);
	}
	XSetInputFocus(dpy, w, RevertToPointerRoot, timestamp());
}

void
top(Client *c)
{
	Client **l, *cc;

	l = &clients;
	for(cc = *l; cc; cc = *l){
		if(cc == c){
			*l = c->next;
			c->next = clients;
			clients = c;
			return;
		}
		l = &cc->next;
	}
	fprintf(stderr, "rio: %p not on client list in top()\n", (void*)c);
}

Client *
getclient(Window w, int create)
{
	Client *c;

	if(w == 0 || getscreen(w))
		return 0;

	for(c = clients; c; c = c->next)
		if(c->window == w || c->parent == w)
			return c;

	if(!create)
		return 0;

	c = (Client *)malloc(sizeof(Client));
	memset(c, 0, sizeof(Client));
	c->window = w;
	/* c->parent will be set by the caller */
	c->parent = None;
	c->reparenting = 0;
	c->state = WithdrawnState;
	c->init = 0;
	c->cmap = None;
	c->label = c->class = 0;
	c->revert = 0;
	c->is9term = 0;
	c->hold = 0;
	c->ncmapwins = 0;
	c->cmapwins = 0;
	c->wmcmaps = 0;
	c->next = clients;
	c->virt = virt;
	clients = c;
	return c;
}

void
rmclient(Client *c)
{
	Client *cc;

	for(cc = current; cc && cc->revert; cc = cc->revert)
		if(cc->revert == c)
			cc->revert = cc->revert->revert;

	if(c == clients)
		clients = c->next;
	for(cc = clients; cc && cc->next; cc = cc->next)
		if(cc->next == c)
			cc->next = cc->next->next;

	if(hidden(c))
		unhidec(c, 0);

	if(c->parent != c->screen->root)
		XDestroyWindow(dpy, c->parent);

	c->parent = c->window = None;		/* paranoia */
	if(current == c){
		current = c->revert;
		if(current == 0)
			nofocus();
		else {
			if(current->screen != c->screen)
				cmapnofocus(c->screen);
			setactive(current, 1);
		}
	}
	if(c->ncmapwins != 0){
		XFree((char *)c->cmapwins);
		free((char *)c->wmcmaps);
	}
	if(c->iconname != 0)
		XFree((char*) c->iconname);
	if(c->name != 0)
		XFree((char*) c->name);
	if(c->instance != 0)
		XFree((char*) c->instance);
	if(c->class != 0)
		XFree((char*) c->class);
	memset(c, 0, sizeof(Client));		/* paranoia */
	free(c);
}

#ifdef	DEBUG
void
dump_revert(void)
{
	Client *c;
	int i;

	i = 0;
	for(c = current; c; c = c->revert){
		fprintf(stderr, "%s(%x:%d)", c->label ? c->label : "?", (int)c->window, c->state);
		if(i++ > 100)
			break;
		if(c->revert)
			fprintf(stderr, " -> ");
	}
	if(current == 0)
		fprintf(stderr, "empty");
	fprintf(stderr, "\n");
}

void
dump_clients(void)
{
	Client *c;

	for(c = clients; c; c = c->next)
		fprintf(stderr, "w 0x%x parent 0x%x @ (%d, %d)\n", (int)c->window, (int)c->parent, c->x, c->y);
}
#endif

void
shuffle(int up)
{
	Client **l, *c;

	if(clients == 0 || clients->next == 0)
		return;
	if(!up){
		c = 0;
		/*for(c=clients; c->next; c=c->next) */
		/*	; */
		for(l=&clients; (*l)->next; l=&(*l)->next)
			if ((*l)->state == 1)
				c = *l;
		if (c == 0)
			return;
		XMapRaised(dpy, c->parent);
		top(c);
		active(c);
	}else{
		c = clients;
		for(l=&clients; *l; l=&(*l)->next)
			;
		clients = c->next;
		*l = c;
		c->next = 0;
		XLowerWindow(dpy, c->window);
	}
/*	XMapRaised(dpy, clients->parent); */
/*	top(clients);	 */
/*	active(clients); */
}
