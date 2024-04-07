#define _GNU_SOURCE
#include <u.h>
#include <libc.h>
#include "wayland-inc.h"
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>
#include <thread.h>
#include <mp.h>
#include <libsec.h>
#include "devdraw.h"

#include "wayland-screen.h"
#include "wayland-shm.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <errno.h>

Globals procState = {0};
const char *dataMimeType = "text/plain;charset=utf-8";
static const int _border = 4;

static struct wl_data_device_listener data_device_listener;
extern const struct wl_keyboard_listener wl_keyboard_listener; // See wayland-keyboard.c
extern void
internal_setcursor(Client *c, Cursor *cur, Cursor2 *cur2);

static void
rpc_resizeimg(Client *c);
static void
rpc_resizewindow(Client *c, Rectangle r);
static void
rpc_setcursor(Client *c, Cursor *cur, Cursor2 *cur2);
static void
rpc_setlabel(Client *c, char *label);
static void
rpc_setmouse(Client *c, Point p);
static void
rpc_topwin(Client *c);
static void
rpc_bouncemouse(Client *c, Mouse m);
static void
rpc_flush(Client *c, Rectangle r);

static ClientImpl wlImpl = {
	.rpc_resizeimg = rpc_resizeimg,
	.rpc_resizewindow = rpc_resizewindow,
	.rpc_setcursor = rpc_setcursor,
	.rpc_setlabel = rpc_setlabel,
	.rpc_setmouse = rpc_setmouse,
	.rpc_topwin = rpc_topwin,
	.rpc_bouncemouse = rpc_bouncemouse,
	.rpc_flush = rpc_flush,
};

static void
setupdecoration(window *w);
static window *
setupwindow(Client *c, Globals *g);
static void
updatedecoration(window *w, Rectangle r);

// This setup allows the gfx thread to not need to spawn ioprocs to handle the
// snarf/clipboard interaction.
//
// Without this, the event loop deadlocks trying to read a pipe it's also
// writing.
char *selfMimeType;
static void
setselfmimetype()
{
	uchar id[12];

	fmtinstall('[', encodefmt);
	genrandom(id, 12);
	selfMimeType = smprint("application/x-plan9port-devdraw-%.*[", sizeof(id), id);
};

// Gfx_main is the only gfx_* function that's allowed to dispatch events.
void
gfx_main(void)
{
	Globals *g;

	// Take this, it's dangerous to go alone:
	// - https://wayland.app
	// - https://wayland-book.com
	g = &procState;
	wl_list_init(&g->output_list);
	setselfmimetype();
	// Set up display and registry first thing.
	g->wl_display = wl_display_connect(NULL);
	if (g->wl_display == NULL) {
		werrstr("unable to open display");
		goto err;
	}
	g->wl_registry = wl_display_get_registry(g->wl_display);
	if (g->wl_registry == NULL) {
		werrstr("unable to get registry");
		goto err;
	}
	wl_registry_add_listener(g->wl_registry, &registry_listener, g);
	// Roundtrip blocks until all pending requests are handled.
	if (wl_display_roundtrip(g->wl_display) == -1) {
		werrstr("unable to roundtrip");
		goto err;
	}
	// Set up the data source & sink:
	g->data_device = wl_data_device_manager_get_data_device(g->data_device_manager, g->wl_seat);
	wl_data_device_add_listener(g->data_device, &data_device_listener, g);
	g->snarf_fd = allocate_shm_file(4096);
	if (g->snarf_fd < 0)
		goto err;
	g->snarf_buf = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, g->snarf_fd, 0);
	memset(g->snarf_buf, 0x00, 4096);

	gfx_started();
	while (wl_display_dispatch(g->wl_display)) {
	}
err:
	sysfatal("%r");
}

static void
sync_cb(void *opaque, struct wl_callback *cb, uint serial)
{
	int *done;

	done = opaque;
	*done = 1;
	wl_callback_destroy(cb);
}

static const struct wl_callback_listener sync_listener = {
	.done = sync_cb,
};

// syncpoint waits until all queued graphics events are handled.
//
// It's like wl_display_roundtrip, but flushes events instead of dispatching
// them. Calling on the graphics thread is likely to deadlock.
static void
syncpoint(struct wl_display *d)
{
	struct wl_callback *cb;
	int done, ret;

	done = 0;
	ret = 0;
	cb = wl_display_sync(d);
	wl_callback_add_listener(cb, &sync_listener, &done);
	while (done == 0 && ret >= 0)
		ret = wl_display_flush(d);
	if (ret == -1 && done == 0)
		wl_callback_destroy(cb);
}

static int
pixfmtPriority(uint32 f)
{
	static const uint32 prio[] = {
		WL_SHM_FORMAT_XRGB8888,
	};
	const int N = sizeof(prio) / (sizeof(prio[0]));
	for (int i = 0; i < N; i++)
		if (prio[i] == f)
			return i;
	return -1;
}

static void
shm_format(void *opaque, struct wl_shm *wl_shm, uint32 format)
{
	Globals *g;
	int p;

	if ((p = pixfmtPriority(format)) < 0)
		return;
	g = opaque;
	if (p < pixfmtPriority(g->pixfmt.wl))
		return;
	g->pixfmt.wl = format;
	g->pixfmt.p9 = format; // TODO write conversion func and actually use negotiated values.
}

static const struct wl_shm_listener wl_shm_formats = {
	.format = shm_format,
};

static void
registry_global(void *opaque, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version)
{
	Globals *g;

	g = opaque;
	if (strcmp(interface, wl_shm_interface.name) == 0) {
		g->wl_shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, 1);
		wl_shm_add_listener(g->wl_shm, &wl_shm_formats, g);
		// Load small cursors by default.
		g->wl_cursor_theme = wl_cursor_theme_load(NULL, 16, g->wl_shm);
	} else if (strcmp(interface, wl_compositor_interface.name) == 0) {
		g->wl_compositor = wl_registry_bind(wl_registry, name, &wl_compositor_interface, 4);
	} else if (strcmp(interface, wl_subcompositor_interface.name) == 0) {
		g->wl_subcompositor = wl_registry_bind(wl_registry, name, &wl_subcompositor_interface, 1);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		struct output_elem *e;
		e = malloc(sizeof(struct output_elem));
		e->o = wl_registry_bind(wl_registry, name, &wl_output_interface, 2);
		wl_output_add_listener(e->o, &output_listener, e);
		wl_list_insert(&g->output_list, &e->link);
	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		g->xdg_wm_base = wl_registry_bind(wl_registry, name, &xdg_wm_base_interface, 3);
		xdg_wm_base_add_listener(g->xdg_wm_base, &xdg_wm_base_listener, nil);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		g->wl_seat = wl_registry_bind(wl_registry, name, &wl_seat_interface, 5);
		wl_seat_add_listener(g->wl_seat, &wl_seat_listener, g);
	} else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0) {
		g->zxdg_decoration_manager_v1 = wl_registry_bind(wl_registry, name, &zxdg_decoration_manager_v1_interface, 1);
	} else if (strcmp(interface, zwp_pointer_constraints_v1_interface.name) == 0) {
		g->pointer_constraints = wl_registry_bind(wl_registry, name, &zwp_pointer_constraints_v1_interface, 1);
	} else if (strcmp(interface, wl_data_device_manager_interface.name) == 0) {
		g->data_device_manager = wl_registry_bind(wl_registry, name, &wl_data_device_manager_interface, 3);
	}
}

static void
registry_global_remove(void *opaque, struct wl_registry *wl_registry, uint32_t name)
{
	fprint(2, "should remove something, not going to\n");
}

static void
xdg_wm_base_ping(void *opaque, struct xdg_wm_base *base, uint32_t serial)
{
	xdg_wm_base_pong(base, serial);
}

static void
xdg_surface_configure(void *opaque, struct xdg_surface *s, uint32_t serial)
{
	Client *c;
	window *w;
	Rectangle r;
	Memimage *new;

	c = opaque;
	w = (void *)c->view;
	if (w->wantsize.x == 0 && w->wantsize.y == 0)
		goto Done;
	if (w->cursize.x == w->wantsize.x && w->cursize.y == w->wantsize.y)
		goto Done;

	r = Rect(0, 0, w->wantsize.x, w->wantsize.y);
	new = allocmemimage(r, strtochan("x8r8g8b8"));
	if (new == nil)
		sysfatal("%r");
	c->mouserect = r;
	gfx_replacescreenimage(c, new);
	updatedecoration(w, r);
	xdg_surface_set_window_geometry(w->xdg_surface, 0, 0, Dx(r), Dy(r));
	w->cursize.x = w->wantsize.x;
	w->cursize.y = w->wantsize.y;
Done:
	xdg_surface_ack_configure(w->xdg_surface, serial);
}

static void
xdg_toplevel_configure_bounds(void *opaque, struct xdg_toplevel *t, int32_t wd, int32_t ht)
{
	Client *c;
	window *w;

	c = opaque;
	w = (void *)c->view;
	w->bounds.x = wd;
	w->bounds.y = ht;
}

static void
xdg_toplevel_configure(void *opaque, struct xdg_toplevel *t, int32_t wd, int32_t ht, struct wl_array *states)
{
	Client *c;
	window *w;
	enum xdg_toplevel_state *s;

	c = opaque;
	w = (void *)c->view;
	wl_array_for_each(s, states)
	{
		// TODO: use this to toggle drawing decorations
		switch (*s) {
		case XDG_TOPLEVEL_STATE_ACTIVATED:
			break;
		case XDG_TOPLEVEL_STATE_FULLSCREEN:
		case XDG_TOPLEVEL_STATE_MAXIMIZED:
			w->bounds.x = wd;
			w->bounds.y = ht;
		case XDG_TOPLEVEL_STATE_RESIZING:
			// w->resizing = 1;
			w->wantsize.x = wd;
			w->wantsize.y = ht;
			break;
		case XDG_TOPLEVEL_STATE_TILED_TOP:
			break;
		case XDG_TOPLEVEL_STATE_TILED_BOTTOM:
			break;
		case XDG_TOPLEVEL_STATE_TILED_LEFT:
			break;
		case XDG_TOPLEVEL_STATE_TILED_RIGHT:
			break;
		}
	};
}

static void
xdg_toplevel_close(void *opaque, struct xdg_toplevel *t)
{
	Globals *g;

	g = opaque;
	wl_display_disconnect(g->wl_display);
	threadexitsall(nil);
}

static void
wl_buffer_release(void *opaque, struct wl_buffer *wl_buffer)
{
	/* Sent by the compositor when it's no longer using this buffer */
	wl_buffer_destroy(wl_buffer);
}

static void
wl_seat_capabilities(void *opaque, struct wl_seat *wl_seat, uint32_t caps)
{
	Globals *g;

	g = opaque;
	g->seat_capabilities = caps;
}

static void
wl_seat_name(void *opaque, struct wl_seat *wl_seat, const char *name)
{
}

static void
updatedecoration(window *w, Rectangle r)
{
	Globals *g;
	struct wl_buffer *buf;
	struct edge *e;
	int wd, ht, skipv, skiph, scale, stride, ppb;

	g = w->global;
	wd = Dx(r);
	ht = Dy(r);
	scale = w->scale;
	skipv = (w->bounds.x != 0 && (wd + (2 * _border)) > w->bounds.x);
	skiph = (w->bounds.y != 0 && (ht + (2 * _border)) > w->bounds.y);
	ppb = _border * scale;
	stride = ppb * sizeof(uint32);
	for (int i = 0; i < 4; i++) {
		e = &w->decoration.edge[i];
		if ((i < 2 && skiph) || (i > 1 && skipv)) {
			if (e->subsurface != nil) {
				wl_subsurface_destroy(e->subsurface);
				e->subsurface = nil;
			}
			continue;
		}
		if (e->subsurface == nil) {
			e->subsurface = wl_subcompositor_get_subsurface(g->wl_subcompositor, e->surface, w->wl_surface);
			wl_subsurface_place_below(e->subsurface, w->wl_surface);
			switch (i) {
			case 0:
				wl_subsurface_set_position(e->subsurface, 0, -_border);
				break;
			case 2:
				wl_subsurface_set_position(e->subsurface, -_border, -_border);
				break;
			}
		}
		// It'd be better if this could have a 0 stride, but that seems
		// disallowed.
		//
		// As is, we keep one pool of pixels for the borders, slice them up as
		// vertical rectangles, and then ask the compositor to rotate if
		// needed.
		switch (i) {
		case 1:
			wl_subsurface_set_position(e->subsurface, 0, ht);
		case 0:
			buf = wl_shm_pool_create_buffer(w->decoration.pool, 0, ppb, (wd * scale), stride, WL_SHM_FORMAT_XRGB8888);
			wl_surface_set_buffer_transform(e->surface, WL_OUTPUT_TRANSFORM_90);
			break;
		case 3:
			wl_subsurface_set_position(e->subsurface, wd, -_border);
		case 2:
			buf = wl_shm_pool_create_buffer(w->decoration.pool, 0, ppb, (ht * scale) + (2 * ppb), stride, WL_SHM_FORMAT_XRGB8888);
			break;
		}
		wl_buffer_add_listener(buf, &wl_buffer_listener, nil);
		wl_surface_set_buffer_scale(e->surface, scale);
		wl_surface_attach(e->surface, buf, 0, 0);
		wl_surface_commit(e->surface);
	}
}

// All the devdraw hooks start here:

// Resizewindow resizes the window decoration (which should already be set up
// via setupdecoration). The draw thread should have already modified the
// memimage, so it will be drawn correctly on the next flush call.
static void
rpc_resizewindow(Client *c, Rectangle r)
{
	window *w;

	w = (void *)c->view;
	updatedecoration(w, r);
	xdg_surface_set_window_geometry(w->xdg_surface, 0, 0, Dx(r), Dy(r));
	wl_display_flush(w->global->wl_display);
}

static void
rpc_resizeimg(Client *c)
{
	window *w;
	Rectangle r;
	Memimage *m, *cur;
	Memdata *md;

	w = (void *)c->view;
	r = Rect(0, 0, w->wantsize.x, w->wantsize.y);
	cur = c->screenimage;
	if (rectinrect(r, cur->r)) {
		md = c->screenimage->data;
		m = allocmemimaged(r, strtochan("x8r8g8b8"), md, cur->X);
	} else {
		m = allocmemimage(r, strtochan("x8r8g8b8"));
	}
	c->mouserect = r;
	gfx_replacescreenimage(c, m);
}

static void
data_offer_offer(void *opaque, struct wl_data_offer *offer, const char *kind)
{
	Globals *g;

	g = opaque;
	if (strcmp(kind, selfMimeType) == 0) {
		wl_data_offer_destroy(offer);
		g->snarf_offer = nil;
	} else if (strcmp(kind, dataMimeType) == 0)
		g->snarf_offer = offer;
	// TODO Look into how some applications turn drag-n-drop into file names.
}

static const struct wl_data_offer_listener data_offer_listener = {
	.offer = data_offer_offer,
};

static void
data_device_selection(void *opaque, struct wl_data_device *dev, struct wl_data_offer *offer)
{
	Globals *g;
	struct stat fs;
	int fds[2];
	long off;
	int ct;

	g = opaque;
	if (offer == nil) {
		// This means the clipboard is empty. Make sure to clear the offer we
		// have stored, if any.
		if (g->snarf_offer != nil)
			wl_data_offer_destroy(g->snarf_offer);
		g->snarf_offer = nil;
		return;
	}

	// Need a "normal" pipe.
#undef pipe
	pipe(fds);
#define pipe p9pipe
	fstat(g->snarf_fd, &fs);
	wl_data_offer_receive(g->snarf_offer, dataMimeType, fds[1]);
	close(fds[1]);
	wl_display_flush(g->wl_display);

	off = 0;
	// Arbitrary limit on incoming paste data. Should maybe be INT_MAX?
	ct = splice(fds[0], NULL, g->snarf_fd, &off, 32 * 1024 * 1024, 0);
	if (ct < 0)
		fprint(2, "oops: %r\n");
	close(fds[0]);
	ct++;
	if (ct > fs.st_size) {
		munmap(g->snarf_buf, fs.st_size);
		g->snarf_buf = mmap(NULL, ct, PROT_READ | PROT_WRITE, MAP_SHARED, g->snarf_fd, 0);
	}
	g->snarf_buf[--ct] = 0x00;
	wl_data_offer_destroy(g->snarf_offer);
	g->snarf_offer = nil;
}
static void
data_device_data_offer(void *opaque, struct wl_data_device *dev, struct wl_data_offer *offer)
{
	wl_data_offer_add_listener(offer, &data_offer_listener, opaque);
}

static struct wl_data_device_listener data_device_listener = {
	.selection = data_device_selection,
	.data_offer = data_device_data_offer,
};

static void
getsnarf_cb(void *opaque, struct wl_callback *cb, uint serial)
{
	char **out;

	out = opaque;
	*out = strdup(procState.snarf_buf);
}

static const struct wl_callback_listener getsnarf_listener = {
	.done = getsnarf_cb,
};

char *
rpc_getsnarf(void)
{
	char *out = NULL;
	int ret;
	struct wl_display *d = procState.wl_display;
	struct wl_callback *cb = wl_display_sync(d);
	wl_callback_add_listener(cb, &getsnarf_listener, &out);
	while (out == NULL && ret >= 0)
		ret = wl_display_flush(d);
	wl_callback_destroy(cb);
	return out;
}

static void
data_source_send(void *opaque, struct wl_data_source *src, const char *kind, int fd)
{
	Globals *g;
	long off;
	size_t len;

	if (strcmp(kind, dataMimeType) != 0)
		return;
	g = opaque;
	off = 0;
	len = strlen(g->snarf_buf); // Sending text, not C strings.
	if (sendfile(fd, g->snarf_fd, &off, len) < 0)
		fprint(2, "sendfile error: %r\n");
	close(fd);
}
static void
data_source_cancel(void *opaque, struct wl_data_source *src)
{
	wl_data_source_destroy(src);
}

static const struct wl_data_source_listener data_source_listener = {
#undef send
	.send = data_source_send,
#define send chansend
	.cancelled = data_source_cancel,
};

struct putsnarf_cmd {
	int done;
	int len;
	char *data;
};

static void
putsnarf_cb(void *opaque, struct wl_callback *cb, uint serial)
{
	// This gets hairy because it pulls the window out of the default client,
	// but the function signature just doesn't allow for passing it in cleanly.
	Globals *g;
	window *w;
	struct putsnarf_cmd *c;
	struct wl_data_source *src;
	struct stat fs;
	int need;

	g = &procState;
	c = opaque;
	if (fstat(g->snarf_fd, &fs) < 0) {
		fprint(2, "unable to stat snarf fd: %r\n");
		goto Done;
	}
	need = c->len + 1;
	if (need > fs.st_size) {
		// Need to grow the file and update the mapping.
		int np = need / 4096;
		if ((need % 4096) != 0)
			np++;
		int sz = np * 4096;
		ftruncate(g->snarf_fd, sz);
		munmap(g->snarf_buf, fs.st_size);
		g->snarf_buf = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, g->snarf_fd, 0);
		if (g->snarf_buf == NULL)
			sysfatal("unable to remap snarf buf: %r\n");
	}
	strncpy(g->snarf_buf, c->data, need);
	w = (window *)client0->view;
	src = wl_data_device_manager_create_data_source(g->data_device_manager);
	wl_data_source_add_listener(src, &data_source_listener, &procState);
	wl_data_source_offer(src, dataMimeType);
	wl_data_source_offer(src, selfMimeType);
	wl_data_device_set_selection(procState.data_device, src, w->kb.S);

Done:
	c->done = 1;
}

static const struct wl_callback_listener putsnarf_listener = {
	.done = putsnarf_cb,
};

void
rpc_putsnarf(char *data)
{
	struct putsnarf_cmd c = {
		.done = 0,
		.len = strlen(data),
		.data = data,
	};
	int ret;
	struct wl_display *d = procState.wl_display;
	struct wl_callback *cb = wl_display_sync(d);
	wl_callback_add_listener(cb, &putsnarf_listener, &c);
	while (c.done == 0 && ret >= 0)
		ret = wl_display_flush(d);
	wl_callback_destroy(cb);
	return;
}

static void
shutdown_cb(void *opaque, struct wl_callback *cb, uint serial)
{
	xdg_toplevel_close(opaque, nil);
}

static const struct wl_callback_listener shutdown_listener = {
	.done = shutdown_cb,
};

void
rpc_shutdown(void)
{
	Globals *g;
	struct wl_callback *cb;

	g = &procState;
	cb = wl_display_sync(g->wl_display);
	wl_callback_add_listener(cb, &shutdown_listener, g);
}

void
rpc_gfxdrawlock(void)
{
}
void
rpc_gfxdrawunlock(void)
{
}

static void
rpc_setlabel(Client *c, char *label)
{
	const window *w;
	char id[64];

	w = c->view;
	snprint(id, 64, "plan9port.%s", label);
	xdg_toplevel_set_title(w->xdg_toplevel, label);
}

static void
flush_release(void *opaque, struct wl_buffer *buf)
{
	int *done;

	done = opaque;
	wl_buffer_destroy(buf);
	*done = 1;
}

const struct wl_buffer_listener flush_listener = {
	.release = flush_release,
};

static void
rpc_flush(Client *c, Rectangle r)
{
	window *w;
	Memimage *i;
	shmTab *t;
	struct wl_shm_pool *pool;
	struct wl_buffer *buf;
	ptrdiff_t off;
	int x, y, scale, done, ret;

	w = (void *)c->view;
	if (w->resizing == 1)
		return;
	done = 0;
	scale = (w->scale > 1) ? 2 : 1;
	i = c->screenimage;
	t = i->X;
	pool = wl_shm_create_pool(w->global->wl_shm, t->fd, t->sz);
	off = (void *)(i->data->bdata + i->zero) - (void *)i->data->base;
	x = Dx(i->r) * scale;
	y = Dy(i->r) * scale;
	assert(y > 0 && x > 0);
	buf = wl_shm_pool_create_buffer(pool, off, x, y, i->width * scale * sizeof(uint32), WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	wl_buffer_add_listener(buf, &flush_listener, &done);
	wl_surface_attach(w->wl_surface, buf, 0, 0);
	wl_surface_set_buffer_scale(w->wl_surface, scale);
	wl_surface_damage_buffer(w->wl_surface, r.min.x * scale, r.min.y * scale, Dx(r) * scale, Dy(r) * scale);
	// We get border updates "for free" because they're synced with the parent
	// surface.
	wl_surface_commit(w->wl_surface);
	while (done == 0 && ret >= 0)
		ret = wl_display_flush(w->global->wl_display);
}

void
rpc_setcursor(Client *c, Cursor *cur, Cursor2 *cur2)
{
	window *w;

	w = (void *)c->view;
	internal_setcursor(c, cur, cur2);
	syncpoint(w->global->wl_display);
}

// Rpc_setmouse warps the pointer by converting into the surface space,
// asking the compositor to lock the mouse, hinting the correct position,
// and then unlocking the mouse.
static void
rpc_setmouse(Client *c, Point p)
{
	Globals *g;
	window *w;
	struct zwp_locked_pointer_v1 *l;

	w = (void *)c->view;
	g = w->global;
	if (g->pointer_constraints == nil) {
		fprint(2, "pointer warping unsupported\n");
		return;
	}

	l = zwp_pointer_constraints_v1_lock_pointer(g->pointer_constraints, w->wl_surface, w->wl_pointer, NULL, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT);
	zwp_locked_pointer_v1_set_cursor_position_hint(l, wl_fixed_from_int(p.x), wl_fixed_from_int(p.y));
	w->m.X = p.x;
	w->m.Y = p.y;
	// wl_surface_commit(w->wl_surface);
	syncpoint(g->wl_display);
	zwp_locked_pointer_v1_destroy(l);
}

static void
rpc_topwin(Client *c)
{
	// I think this is impossible in the Wayland world.
}

static void
rpc_bouncemouse(Client *c, Mouse m)
{
	// Unsure what bouncemouse should do.
}

static void
setscale(Client *c, struct wl_list *list)
{
	struct output_elem *e;
	window *w;
	int scale, dpi;

	scale = 1;
	dpi = 100;
	w = (void *)c->view;
	wl_list_for_each(e, list, link)
	{
		if (e->on != 1)
			continue;
		if (e->scale > scale)
			scale = e->scale;
		if (e->dpi > dpi)
			dpi = e->dpi;
	}
	c->displaydpi = dpi;
	w->scale = scale;
}

static void
surface_enter(void *opaque, struct wl_surface *s, struct wl_output *o)
{
	Client *c;
	Globals *g;
	window *w;
	struct output_elem *e;

	c = opaque;
	w = (void *)c->view;
	g = w->global;

	wl_list_for_each(e, &g->output_list, link)
	{
		if (e->o == o)
			e->on = 1;
	}
	setscale(c, &g->output_list);
	// wl_output_add_listener(o, &output_listener, opaque);
}

static void
surface_leave(void *opaque, struct wl_surface *s, struct wl_output *o)
{
	Client *c;
	Globals *g;
	window *w;
	struct output_elem *e;

	c = opaque;
	w = (void *)c->view;
	g = w->global;

	wl_list_for_each(e, &g->output_list, link)
	{
		if (e->o == o)
			e->on = 0;
	}
	setscale(c, &g->output_list);
}

static const struct wl_surface_listener wl_surface_listener = {
	.enter = surface_enter,
	.leave = surface_leave,
};

// Guesses the parent's arg0 by looking at the cmdline file in proc.
//
// Pgid might be a better pid to look at, but requires looking at whether rc(1)
// uses setpgid the same way bash(1) et.al. do.
static char *
guessappid(void)
{
	int ppid;
	char buf[64];
	int fd;

	ppid = getppid();
	snprint(buf, 64, "/proc/%d/cmdline", ppid);
	fd = open(buf, OREAD);
	if (fd == -1)
		return nil;
	read(fd, buf, 64);
	close(fd);
	buf[63] = 0x00;
	return strdup(buf);
}

static window *
setupwindow(Client *c, Globals *g)
{
	window *w;
	char *app_id;
	struct zxdg_toplevel_decoration_v1 *d;
	int fd;

	w = mallocz(sizeof(window), 1);
	// Populate everything:
	c->view = w;
	w->global = g;
	w->kb.context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (w->kb.context == nil) {
		free(w);
		werrstr("unable to allocate new xkb_context");
		return nil;
	}
	app_id = guessappid();
	w->wl_surface = wl_compositor_create_surface(g->wl_compositor);
	w->xdg_surface = xdg_wm_base_get_xdg_surface(g->xdg_wm_base, w->wl_surface);
	w->xdg_toplevel = xdg_surface_get_toplevel(w->xdg_surface);
	for (int i = 0; i < 4; i++) {
		w->decoration.edge[i].surface = wl_compositor_create_surface(g->wl_compositor);
	}
	w->wl_pointer = wl_seat_get_pointer(g->wl_seat);
	w->wl_keyboard = wl_seat_get_keyboard(g->wl_seat);
	w->cursor.surface = wl_compositor_create_surface(g->wl_compositor);
	fd = allocate_shm_file(curBufSz);
	w->cursor.pool = wl_shm_create_pool(w->global->wl_shm, fd, curBufSz);
	w->cursor.buf = mmap(NULL, curBufSz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);

	wl_surface_add_listener(w->wl_surface, &wl_surface_listener, c);
	xdg_surface_add_listener(w->xdg_surface, &xdg_surface_listener, c);
	xdg_toplevel_add_listener(w->xdg_toplevel, &xdg_toplevel_listener, c);
	xdg_toplevel_set_title(w->xdg_toplevel, "devdraw");
	if (app_id == nil)
		xdg_toplevel_set_app_id(w->xdg_toplevel, "devdraw");
	else {
		fprint(2, "app_id: %s\n", app_id);
		xdg_toplevel_set_app_id(w->xdg_toplevel, app_id);
		free(app_id);
	}

	// Signal we're drawing our own decoration (for better or worse).
	if (g->zxdg_decoration_manager_v1 != NULL) {
		d = zxdg_decoration_manager_v1_get_toplevel_decoration(g->zxdg_decoration_manager_v1, w->xdg_toplevel);
		zxdg_toplevel_decoration_v1_set_mode(d, ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
		zxdg_toplevel_decoration_v1_destroy(d);
	}
	setscale(c, &g->output_list);
	setupdecoration(w);
	return w;
};

static void
setupdecoration(window *w)
{
	int sz, fd, x, y;
	uint32 *map;

	// Allocate backing buffers big enough for the whole screen and then just
	// don't worry about it.
	x = w->global->bounds.x;
	if (x == 0)
		x = 1920;
	y = w->global->bounds.y;
	if (y == 0)
		y = 1080;

	sz = ((x > y) ? x : y + (2 * _border)) * _border * sizeof(uint32);
	fd = allocate_shm_file(sz);
	map = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	w->decoration.pool = wl_shm_create_pool(w->global->wl_shm, fd, sz);
	close(fd);
	for (int i = 0; i * sizeof(uint32) < sz; i++)
		map[i] = 0x55AAAA;
};

// Rpc_attach creates a new window and associates it with the passed
// *client.
Memimage *
rpc_attach(Client *client, char *label, char *winsize)
{
	Globals *g;
	window *w;
	Rectangle r;
	int havemin;
	Memimage *i;

	r = Rect(0, 0, 800, 600);
	if (winsize && winsize[0])
		if (parsewinsize(winsize, &r, &havemin) < 0)
			sysfatal("%r");
	g = &procState;
	w = setupwindow(client, g);
	if (w == nil)
		sysfatal("unable to allocate new window");
	wl_pointer_add_listener(w->wl_pointer, &wl_pointer_listener, client);
	wl_keyboard_add_listener(w->wl_keyboard, &wl_keyboard_listener, client);
	syncpoint(w->global->wl_display);
	if (w->cursize.x != 0 && w->cursize.y != 0)
		// May have had bounds suggested by callbacks run during the syncpoint.
		r = Rect(0, 0, w->cursize.x, w->cursize.y);

	r.max = mulpt(r.max, (w->scale > 1) ? 2 : 1);
	i = allocmemimage(r, strtochan("x8r8g8b8"));
	// Set up the client object:
	client->impl = &wlImpl;
	client->mouserect = r;
	if (label && label[0])
		rpc_setlabel(client, label);
	rpc_resizewindow(client, r);     // updates decorations
	rpc_setcursor(client, nil, nil); // Cursor has an implicit flush
	return i;
};
