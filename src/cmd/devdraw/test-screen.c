/*
 * In-memory devdraw backend, selected by DEVDRAW_TEST_MEMORY=1.
 *
 * Instead of talking to a host window system, the "screen" is just a
 * Memimage in this process's memory. No host routines are used. The draw
 * RPC service and (optional) scripted-input reader run exactly as they do
 * with the real backend; only the host display is replaced.
 *
 * If DEVDRAW_TEST_INPUT is not set, attempts to read the keyboard or mouse
 * simply block forever (no events are ever delivered), as if the user is
 * not touching the keyboard or mouse.
 */

#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <mouse.h>
#include <cursor.h>
#include <keyboard.h>
#include <drawfcall.h>
#include "devdraw.h"

static void	t_rpc_resizeimg(Client*);
static void	t_rpc_resizewindow(Client*, Rectangle);
static void	t_rpc_setcursor(Client*, Cursor*, Cursor2*);
static void	t_rpc_setlabel(Client*, char*);
static void	t_rpc_setmouse(Client*, Point);
static void	t_rpc_topwin(Client*);
static void	t_rpc_bouncemouse(Client*, Mouse);
static void	t_rpc_flush(Client*, Rectangle);

static ClientImpl testimpl = {
	t_rpc_resizeimg,
	t_rpc_resizewindow,
	t_rpc_setcursor,
	t_rpc_setlabel,
	t_rpc_setmouse,
	t_rpc_topwin,
	t_rpc_bouncemouse,
	t_rpc_flush,
};

static uint
msec(void)
{
	return nsec()/1000000;
}

// t_allocscreen allocates a fresh in-memory window image of the given size.
static Memimage*
t_allocscreen(Rectangle r)
{
	Memimage *m;

	m = _allocmemimage(r, XRGB32);
	if(m == nil)
		sysfatal("test backend: allocmemimage: %r");
	_memfillcolor(m, 0xFFFFFFFF);	// blank white window until the client paints
	return m;
}

// t_rpc_attach creates the in-memory window and attaches it to client c.
static Memimage*
t_rpc_attach(Client *c, char *label, char *winsize)
{
	Rectangle wr;
	int havemin;

	USED(label);
	c->impl = &testimpl;

	if(winsize == nil || winsize[0] == '\0' || parsewinsize(winsize, &wr, &havemin) < 0)
		wr = Rect(0, 0, 1024, 768);
	wr = Rect(0, 0, Dx(wr), Dy(wr));	// the in-memory window lives at the origin
	if(Dx(wr) <= 0 || Dy(wr) <= 0)
		wr = Rect(0, 0, 1024, 768);

	c->mouserect = wr;
	c->displaydpi = 100;

	// Park the mouse just inside the bottom-right corner so the cursor does
	// not obscure text drawn at the top-left (where frames start). The default
	// arrow is 16x16 with hotspot offset {-1,-1}, so its composited pixels span
	// [xy-1, xy+15); we want that exclusive bottom-right (xy+15) 10px up and
	// 10px left of the window's bottom-right corner, i.e. xy = corner - 25.
	c->mouse.last.xy = Pt(Dx(wr) - 25, Dy(wr) - 25);

	return t_allocscreen(wr);
}

void
t_gfx_main(void)
{
	gfx_started();
	// The host backend's gfx_main runs the platform event loop and never
	// returns. We have no display to service, so just block forever while
	// the RPC service proc (and optional input proc) do the work.
	for(;;)
		sleep(1000);
}

void
t_rpc_shutdown(void)
{
	// serveproc calls threadexitsall(nil) after this returns.
}

// t_rpc_resizeimg reallocates the window image at its current size and
// reinstalls it. It is the response to Cmd-R (retina toggle), which changes
// only the reported dpi, not the pixel size.
static void
t_rpc_resizeimg(Client *c)
{
	Rectangle r;

	r = c->screenimage->r;
	c->mouserect = r;
	gfx_replacescreenimage(c, t_allocscreen(r));
}

// t_rpc_resizewindow resizes the window to r's size. It serves both a client
// Tresize request and a scripted "resize" input command (a simulated user
// resize); either way the window image is reallocated at the new size.
static void
t_rpc_resizewindow(Client *c, Rectangle r)
{
	Rectangle nr;

	nr = Rect(0, 0, Dx(r), Dy(r));
	if(Dx(nr) <= 0 || Dy(nr) <= 0)
		return;
	c->mouserect = nr;
	gfx_replacescreenimage(c, t_allocscreen(nr));
}

// t_rpc_setmouse moves the cursor: record the move as a mouse event so the
// next mouse read reflects it (the current button state is preserved).
static void
t_rpc_setmouse(Client *c, Point p)
{
	gfx_mousetrack(c, p.x, p.y, c->mouse.last.buttons, msec());
}

// The cursor shape is recorded in the Client by srv.c for output compositing;
// there is no host cursor to update.
static void
t_rpc_setcursor(Client *c, Cursor *cur, Cursor2 *cur2)
{
	USED(c); USED(cur); USED(cur2);
}

static void
t_rpc_setlabel(Client *c, char *label)
{
	USED(c); USED(label);
}

static void
t_rpc_topwin(Client *c)
{
	USED(c);
}

static void
t_rpc_bouncemouse(Client *c, Mouse m)
{
	USED(c); USED(m);
}

static void
t_rpc_flush(Client *c, Rectangle r)
{
	// Frames are written on the 'v' (visible flush) opcode, not on the
	// dirty-region batch flushes that addflush emits as the draw buffer fills,
	// so the frame set matches the logical flushes in the draw log. See the
	// 'v' case in draw_datawrite (devdraw.c).
	USED(c);
	USED(r);
}

// The in-memory backend does not use any host graphics state during
// memimagedraw, so the draw lock is a no-op.
static void
t_drawlock(void)
{
}

static void
t_drawunlock(void)
{
}

// Snarf (clipboard) is kept in a plain in-memory buffer.
static char *snarf;

static char*
t_rpc_getsnarf(void)
{
	if(snarf == nil)
		return nil;
	return strdup(snarf);
}

static void
t_rpc_putsnarf(char *s)
{
	free(snarf);
	snarf = nil;
	if(s != nil)
		snarf = strdup(s);
}

Gfx testgfx = {
	t_gfx_main,
	t_rpc_shutdown,
	t_rpc_attach,
	t_rpc_getsnarf,
	t_rpc_putsnarf,
	t_drawlock,
	t_drawunlock,
};
