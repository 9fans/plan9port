#define _POSIX_C_SOURCE 200809L
#include <sys/mman.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <linux/input-event-codes.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
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
#include "bigarrow.h"
#include "wl-inc.h"

static QLock wllk;

static Wlwin *snarfwin;

static int clientruning;

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
	wl->client->view = wl;
	wl->client->impl = &wlimpl;
	wl->dx = 1024;
	wl->dy = 1024;
	wl->monx = 1920;
	wl->mony = 1080;
	return wl;
}

void
wlflush(Wlwin *wl)
{
	qlock(&wllk);
	if(wl->dirty == 1)
		memcpy(wl->shm_data, wl->screen->data->bdata, wl->dx*wl->dy*4);

	wl_surface_attach(wl->surface, wl->screenbuffer, 0, 0);
	wl_surface_damage(wl->surface, 0, 0, wl->dx, wl->dy);
	wl_surface_commit(wl->surface);
	wl->dirty = 0;
	qunlock(&wllk);
}

void
wlresize(Wlwin *wl, int x, int y)
{
	Rectangle r;

	qlock(&wllk);
	wl->dx = x;
	wl->dy = y;

	wlallocbuffer(wl);
	r = Rect(0, 0, wl->dx, wl->dy);
	wl->client->mouserect = r;
	wl->screen = allocmemimage(r, XRGB32);
	wl->dirty = 1;
	qunlock(&wllk);
	gfx_replacescreenimage(wl->client, wl->screen);
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

	wl = newwlwin(client);
	snarfwin = wl;
	wl->display = wl_display_connect(NULL);
	if(wl->display == nil)
		sysfatal("could not connect to display");

	wlsetcb(wl);
	wlsettitle(wl, label);
	wlresize(wl, wl->dx, wl->dy);
	wldrawcursor(wl, &bigarrow);
	wlflush(wl);
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
	wl->dirty = 1;
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
}

void
rpc_setcursor(Client *client, Cursor *c, Cursor2 *c2)
{
	if(c == nil)
		c = &bigarrow;
	qlock(&wllk);
	wldrawcursor(client->view, c);
	qunlock(&wllk);
}

void
rpc_bouncemouse(Client *c, Mouse m)
{
}

void
rpc_setlabel(Client *c, char *s)
{
	wlsettitle(c->view, s);
}

void
rpc_topwin(Client *c)
{
}

void
rpc_setmouse(Client *c, Point p)
{
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
