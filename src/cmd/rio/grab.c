/* Copyright (c) 1994-1996 David Hogan, see README for licence details */
#include <stdio.h>
#include <X11/X.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "dat.h"
#include "fns.h"

int
nobuttons(XButtonEvent *e)	/* Einstuerzende */
{
	int state;

	state = (e->state & AllButtonMask);
	return (e->type == ButtonRelease) && (state & (state - 1)) == 0;
}

int
grab(Window w, Window constrain, int mask, Cursor curs, int t)
{
	int status;

	if (t == 0)
		t = timestamp();
	status = XGrabPointer(dpy, w, False, mask,
		GrabModeAsync, GrabModeAsync, constrain, curs, t);
	return status;
}

void
ungrab(XButtonEvent *e)
{
	XEvent ev;

	if (!nobuttons(e))
		for (;;) {
			XMaskEvent(dpy, ButtonMask | ButtonMotionMask, &ev);
			if (ev.type == MotionNotify)
				continue;
			e = &ev.xbutton;
			if (nobuttons(e))
				break;
		}
	XUngrabPointer(dpy, e->time);
	curtime = e->time;
}

static void
drawstring(Display *dpy, ScreenInfo *s, Menu *m, int wide, int high, int i, int selected)
{
	int tx, ty;

	tx = (wide - XTextWidth(font, m->item[i], strlen(m->item[i])))/2;
	ty = i*high + font->ascent + 1;
	XFillRectangle(dpy, s->menuwin, selected ? s->gcmenubgs : s->gcmenubg, 0, i*high, wide, high);
	XDrawString(dpy, s->menuwin, selected ? s->gcmenufgs : s->gcmenufg, tx, ty, m->item[i], strlen(m->item[i]));
}

int
menuhit(XButtonEvent *e, Menu *m)
{
	XEvent ev;
	int i, n, cur, old, wide, high, status, drawn, warp;
	int x, y, dx, dy, xmax, ymax;
	ScreenInfo *s;

	if (font == 0)
		return -1;
	s = getscreen(e->root);
	if (s == 0 || e->window == s->menuwin)	   /* ugly event mangling */
		return -1;

	dx = 0;
	for (n = 0; m->item[n]; n++) {
		wide = XTextWidth(font, m->item[n], strlen(m->item[n])) + 4;
		if (wide > dx)
			dx = wide;
	}
	wide = dx;
	cur = m->lasthit;
	if (cur >= n)
		cur = n - 1;

	high = font->ascent + font->descent + 1;
	dy = n*high;
	x = e->x - wide/2;
	y = e->y - cur*high - high/2;
	warp = 0;
	xmax = DisplayWidth(dpy, s->num);
	ymax = DisplayHeight(dpy, s->num);
	if (x < 0) {
		e->x -= x;
		x = 0;
		warp++;
	}
	if (x+wide >= xmax) {
		e->x -= x+wide-xmax;
		x = xmax-wide;
		warp++;
	}
	if (y < 0) {
		e->y -= y;
		y = 0;
		warp++;
	}
	if (y+dy >= ymax) {
		e->y -= y+dy-ymax;
		y = ymax-dy;
		warp++;
	}
	if (warp)
		setmouse(e->x, e->y, s);
	XMoveResizeWindow(dpy, s->menuwin, x, y, dx, dy);
	XSelectInput(dpy, s->menuwin, MenuMask);
	XMapRaised(dpy, s->menuwin);
	status = grab(s->menuwin, None, MenuGrabMask, None, e->time);
	if (status != GrabSuccess) {
		/* graberror("menuhit", status); */
		XUnmapWindow(dpy, s->menuwin);
		return -1;
	}
	drawn = 0;
	for (;;) {
		XMaskEvent(dpy, MenuMask, &ev);
		switch (ev.type) {
		default:
			fprintf(stderr, "9wm: menuhit: unknown ev.type %d\n", ev.type);
			break;
		case ButtonPress:
			break;
		case ButtonRelease:
			if (ev.xbutton.button != e->button)
				break;
			x = ev.xbutton.x;
			y = ev.xbutton.y;
			i = y/high;
			if (cur >= 0 && y >= cur*high-3 && y < (cur+1)*high+3)
				i = cur;
			if (x < 0 || x > wide || y < -3)
				i = -1;
			else if (i < 0 || i >= n)
				i = -1;
			else
				m->lasthit = i;
			if (!nobuttons(&ev.xbutton))
				i = -1;
			ungrab(&ev.xbutton);
			XUnmapWindow(dpy, s->menuwin);
			return i;
		case MotionNotify:
			if (!drawn)
				break;
			x = ev.xbutton.x;
			y = ev.xbutton.y;
			old = cur;
			cur = y/high;
			if (old >= 0 && y >= old*high-3 && y < (old+1)*high+3)
				cur = old;
			if (x < 0 || x > wide || y < -3)
				cur = -1;
			else if (cur < 0 || cur >= n)
				cur = -1;
			if (cur == old)
				break;
			if (old >= 0 && old < n)
				drawstring(dpy, s, m, wide, high, old, 0);
			if (cur >= 0 && cur < n)
				drawstring(dpy, s, m, wide, high, cur, 1);
			break;
		case Expose:
			XClearWindow(dpy, s->menuwin);
			for (i = 0; i < n; i++)
				drawstring(dpy, s, m, wide, high, i, cur==i);
			drawn = 1;
		}
	}
}

Client *
selectwin(int release, int *shift, ScreenInfo *s)
{
	XEvent ev;
	XButtonEvent *e;
	int status;
	Window w;
	Client *c;

	status = grab(s->root, s->root, ButtonMask, s->target, 0);
	if (status != GrabSuccess) {
		graberror("selectwin", status); /* */
		return 0;
	}
	w = None;
	for (;;) {
		XMaskEvent(dpy, ButtonMask, &ev);
		e = &ev.xbutton;
		switch (ev.type) {
		case ButtonPress:
			if (e->button != Button3) {
				ungrab(e);
				return 0;
			}
			w = e->subwindow;
			if (!release) {
				c = getclient(w, 0);
				if (c == 0)
					ungrab(e);
				if (shift != 0)
					*shift = (e->state&ShiftMask) != 0;
				return c;
			}
			break;
		case ButtonRelease:
			ungrab(e);
			if (e->button != Button3 || e->subwindow != w)
				return 0;
			if (shift != 0)
				*shift = (e->state&ShiftMask) != 0;
			return getclient(w, 0);
		}
	}
}

void
sweepcalc(Client *c, int x, int y)
{
	int dx, dy, sx, sy;

	dx = x - c->x;
	dy = y - c->y;
	sx = sy = 1;
	x += dx;
	if (dx < 0) {
		dx = -dx;
		sx = -1;
	}
	y += dy;
	if (dy < 0) {
		dy = -dy;
		sy = -1;
	}

	dx -= 2*BORDER;
	dy -= 2*BORDER;

	if (!c->is9term) {
		if (dx < c->min_dx)
			dx = c->min_dx;
		if (dy < c->min_dy)
			dy = c->min_dy;
	}

	if (c->size.flags & PResizeInc) {
		dx = c->min_dx + (dx-c->min_dx)/c->size.width_inc*c->size.width_inc;
		dy = c->min_dy + (dy-c->min_dy)/c->size.height_inc*c->size.height_inc;
	}

	if (c->size.flags & PMaxSize) {
		if (dx > c->size.max_width)
			dx = c->size.max_width;
		if (dy > c->size.max_height)
			dy = c->size.max_height;
	}
	c->dx = sx*(dx + 2*BORDER);
	c->dy = sy*(dy + 2*BORDER);
}

void
dragcalc(Client *c, int x, int y)
{
	c->x += x;
	c->y += y;
}

static void
xcopy(int fwd, Display *dpy, Drawable src, Drawable dst, GC gc, int x, int y, int dx, int dy, int x1, int y1)
{
	if(fwd)
		XCopyArea(dpy, src, dst, gc, x, y, dx, dy, x1, y1);
	else
		XCopyArea(dpy, dst, src, gc, x1, y1, dx, dy, x, y);
}

void
drawbound(Client *c, int drawing)
{
	int x, y, dx, dy;
	ScreenInfo *s;
	
	s = c->screen;
	x = c->x;
	y = c->y;
	dx = c->dx;
	dy = c->dy;
	if (dx < 0) {
		x += dx;
		dx = -dx;
	}
	if (dy < 0) {
		y += dy;
		dy = -dy;
	}
	if (dx <= 2 || dy <= 2)
		return;

	if(solidsweep){
		if(drawing == -1){
			XUnmapWindow(dpy, s->sweepwin);
			return;
		}
		
		x += BORDER;
		y += BORDER;
		dx -= 2*BORDER;
		dy -= 2*BORDER;

		if(drawing){
			XMoveResizeWindow(dpy, s->sweepwin, x, y, dx, dy);
			XSelectInput(dpy, s->sweepwin, MenuMask);
			XMapRaised(dpy, s->sweepwin);
		}
		return;
	}

	if(drawing == -1)
		return;

	xcopy(drawing, dpy, s->root, s->bkup[0], s->gccopy, x, y, dx, BORDER, 0, 0);
	xcopy(drawing, dpy, s->root, s->bkup[0], s->gccopy, x, y+dy-BORDER, dx, BORDER, dx, 0);
	xcopy(drawing, dpy, s->root, s->bkup[1], s->gccopy, x, y, BORDER, dy, 0, 0);
	xcopy(drawing, dpy, s->root, s->bkup[1], s->gccopy, x+dx-BORDER, y, BORDER, dy, 0, dy);

	if(drawing){
		XFillRectangle(dpy, s->root, s->gcred, x, y, dx, BORDER);
		XFillRectangle(dpy, s->root, s->gcred, x, y+dy-BORDER, dx, BORDER);
		XFillRectangle(dpy, s->root, s->gcred, x, y, BORDER, dy);
		XFillRectangle(dpy, s->root, s->gcred, x+dx-BORDER, y, BORDER, dy);
	}
}

void
misleep(int msec)
{
	struct timeval t;

	t.tv_sec = msec/1000;
	t.tv_usec = (msec%1000)*1000;
	select(0, 0, 0, 0, &t);
}

int
sweepdrag(Client *c, XButtonEvent *e0, void (*recalc)(Client*, int, int))
{
	XEvent ev;
	int idle;
	int cx, cy, rx, ry;
	int ox, oy, odx, ody;
	XButtonEvent *e;

	ox = c->x;
	oy = c->y;
	odx = c->dx;
	ody = c->dy;
	c->x -= BORDER;
	c->y -= BORDER;
	c->dx += 2*BORDER;
	c->dy += 2*BORDER;
	if (e0)
		getmouse(&c->x, &c->y, c->screen);
	else
		getmouse(&cx, &cy, c->screen);
	XGrabServer(dpy);
	drawbound(c, 1);
	idle = 0;
	for (;;) {
		if (XCheckMaskEvent(dpy, ButtonMask, &ev) == 0) {
			getmouse(&rx, &ry, c->screen);
			if (rx != cx || ry != cy || ++idle > 300) {
				drawbound(c, 0);
				if (rx == cx && ry == cy) {
					XUngrabServer(dpy);
					XFlush(dpy);
					misleep(500);
					XGrabServer(dpy);
					idle = 0;
				}
				if(e0)
					recalc(c, rx, ry);
				else
					recalc(c, rx-cx, ry-cy);
				cx = rx;
				cy = ry;
				drawbound(c, 1);
				XFlush(dpy);
			}
			misleep(50);
			continue;
		}
		e = &ev.xbutton;
		switch (ev.type) {
		case ButtonPress:
		case ButtonRelease:
			drawbound(c, 0);
			ungrab(e);
			XUngrabServer(dpy);
			if (e->button != Button3 && c->init)
				goto bad;
			if (c->dx < 0) {
				c->x += c->dx;
				c->dx = -c->dx;
			}
			if (c->dy < 0) {
				c->y += c->dy;
				c->dy = -c->dy;
			}
			c->x += BORDER;
			c->y += BORDER;
			c->dx -= 2*BORDER;
			c->dy -= 2*BORDER;
			if (c->dx < 4 || c->dy < 4 || c->dx < c->min_dx || c->dy < c->min_dy)
				goto bad;
			return 1;
		}
	}
bad:
	c->x = ox;
	c->y = oy;
	c->dx = odx;
	c->dy = ody;
	drawbound(c, -1);
	return 0;
}

int
sweep(Client *c)
{
	XEvent ev;
	int status;
	XButtonEvent *e;
	ScreenInfo *s;

	s = c->screen;
	status = grab(s->root, s->root, ButtonMask, s->sweep0, 0);
	if (status != GrabSuccess) {
		graberror("sweep", status); /* */
		return 0;
	}

	XMaskEvent(dpy, ButtonMask, &ev);
	e = &ev.xbutton;
	if (e->button != Button3) {
		ungrab(e);
		return 0;
	}
	XChangeActivePointerGrab(dpy, ButtonMask, s->boxcurs, e->time);
	return sweepdrag(c, e, sweepcalc);
}

int
drag(Client *c)
{
	int status;
	ScreenInfo *s;

	s = c->screen;
	status = grab(s->root, s->root, ButtonMask, s->boxcurs, 0);
	if (status != GrabSuccess) {
		graberror("drag", status); /* */
		return 0;
	}
	return sweepdrag(c, 0, dragcalc);
}

void
getmouse(int *x, int *y, ScreenInfo *s)
{
	Window dw1, dw2;
	int t1, t2;
	unsigned int t3;

	XQueryPointer(dpy, s->root, &dw1, &dw2, x, y, &t1, &t2, &t3);
}

void
setmouse(int x, int y, ScreenInfo *s)
{
	XWarpPointer(dpy, None, s->root, None, None, None, None, x, y);
}
