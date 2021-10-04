#define _POSIX_C_SOURCE 200809L
#include <sys/mman.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <linux/input-event-codes.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>
#include <xkbcommon/xkbcommon.h>
#include "xdg-shell-protocol.h"

#include <u.h>
#include <errno.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>
#include <thread.h>
#include "devdraw.h"
#include "wl-inc.h"

static QLock wllk;

static Wlwin *snarfwin;

static clientruning;

static void rpc_resizeimg(Client*);
static void rpc_resizewindow(Client*, Rectangle);
static void rpc_setcursor(Client*, Cursor*, Cursor2*);
static void rpc_setlabel(Client*, char*);
static void rpc_setmouse(Client*, Point);
static void rpc_topwin(Client*);
static void rpc_bouncemouse(Client*, Mouse);
static void rpc_flush(Client*, Rectangle);

static ClientImpl wlimpl = {
	rpc_resizeimg,
	rpc_resizewindow,
	rpc_setcursor,
	rpc_setlabel,
	rpc_setmouse,
	rpc_topwin,
	rpc_bouncemouse,
	rpc_flush,
};

static Wlwin*
newwlwin(Client *c)
{
	Wlwin *wl;

	wl = mallocz(sizeof *wl, 1);
	if(wl == nil)
		sysfatal("malloc Wlwin");
	wl->client = c;
	wl->dx = 1024;
	wl->dy = 1024;
	return wl;
}

void
dispatchproc(void *a)
{
	Wlwin *wl;
	wl = a;
	for(;clientruning == 1 && wl->runing == 1;){
		wl_display_dispatch(wl->display);
	}
}

static Memimage*
wlattach(Client *client, char *label, char *winsize)
{
	Rectangle r;
	Wlwin *wl;

	wl = newwlwin(nil);
	snarfwin = wl;
	wl->client = client;
	wl->client->view = wl;
	wl->client->impl = &wlimpl;
	wl->display = wl_display_connect(NULL);
	if(wl->display == nil)
		sysfatal("could not connect to display");

	wlsetcb(wl);
	r = Rect(0, 0, wl->dx, wl->dy);
	wl->screen = allocmemimage(r, XRGB32);
	wl->runing = 1;
	proccreate(dispatchproc, wl, 8192);
	return wl->screen;
}

void
rpc_gfxdrawlock(void)
{
	qlock(&wllk);
}

void
rpc_gfxdrawunlock(void)
{
	qunlock(&wllk);
}

void
gfx_main(void)
{
	clientruning = 1;
	gfx_started();
}

Memimage*
rpc_attach(Client *client, char *label, char *winsize)
{
	return wlattach(client, label, winsize);
}

void
rpc_flush(Client *client, Rectangle r)
{
	Wlwin *wl;

	wl = (Wlwin*)client->view;
	qlock(&wllk);
	memcpy(wl->shm_data, wl->screen->data->bdata, wl->dx*wl->dy*4);
	wl_surface_attach(wl->surface, wl->buffer, 0, 0);
	wl_surface_damage(wl->surface, 0, 0, wl->dx, wl->dy);
	wl_surface_commit(wl->surface);
	qunlock(&wllk);
}

void
rpc_resizeimg(Client *client)
{
	Rectangle r;
	Wlwin *wl;

	wl = (Wlwin*)client->view;
	qlock(&wllk);
	r = Rect(0, 0, wl->dx, wl->dy);
	wl->screen = allocmemimage(r, XRGB32);
	qunlock(&wllk);
	gfx_replacescreenimage(client, wl->screen);
}

void
rpc_resizewindow(Client *client, Rectangle r)
{
	fprint(2, "resize window stub\n");
}

void
rpc_setcursor(Client *client, Cursor *c, Cursor2 *c2)
{
	fprint(2, "Set cursor stub");
}

void
rpc_bouncemouse(Client *c, Mouse m)
{
	fprint(2, "bounce mouse stub\n");
}

void
rpc_setlabel(Client *c, char*)
{
	fprint(2, "set label stub\n");
}

void
rpc_topwin(Client *c)
{
	fprint(2, "top win stub\n");
}

void
rpc_setmouse(Client *c, Point p)
{
	fprint(2, "set mouse stub\n");
}

void
rpc_shutdown(void)
{
	clientruning = 0;
}

char*
rpc_getsnarf(void)
{
	return wlgetsnarf(snarfwin);
}

void
rpc_putsnarf(char *data)
{
	wlsetsnarf(snarfwin, data);
}
