#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <mouse.h>
#include <cursor.h>
#include "devdraw.h"
#include "bigarrow.h"
#include "wayland-inc.h"

// Big enough for a Cursor2:
const int curBufSz = 32 * 32 * sizeof(uint32);

static void enter(void *opaque, struct wl_pointer *p, uint32_t serial, struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y);
static void leave(void *opaque, struct wl_pointer *p, uint32_t serial, struct wl_surface *surface);
static void motion(void *opaque, struct wl_pointer *p, uint32_t time, wl_fixed_t x, wl_fixed_t y);
static void button(void *opaque, struct wl_pointer *p, uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
static void frame(void *opaque, struct wl_pointer *p);
static void axis(void *opaque, struct wl_pointer *p, uint32 time, uint32 axis, wl_fixed_t val);
static void axis_discrete(void *opaque, struct wl_pointer *p, uint32 axis, int32 discrete);
static void axis_source(void *opaque, struct wl_pointer *p, uint32 src);
static void axis_stop(void *opaque, struct wl_pointer *p, uint32 time, uint32 axis);

const struct wl_pointer_listener wl_pointer_listener = {
	.enter = enter,
	.leave = leave,
	.motion = motion,
	.button = button,
	.frame = frame,
	.axis = axis,
	.axis_discrete = axis_discrete,
	.axis_source = axis_source,
	.axis_stop = axis_stop,
};

static void
cursor_buffer_release(void *opaque, struct wl_buffer *buf)
{
	wl_buffer_destroy(buf);
};

static const struct wl_buffer_listener cursor_buffer_listener = {
	.release = cursor_buffer_release,
};

void
internal_setcursor(Client *c, Cursor *cur, Cursor2 *cur2)
{
	struct wl_buffer *curBuf;
	window *w;
	int off;
	int curSz;

	w = (void *)c->view;
	if (cur == nil && cur2 == nil) {
		cur2 = &bigarrow2;
		cur = &bigarrow;
	}
	off = 0;
	memset((void *)w->cursor.buf, 0x00, curBufSz);
	if (w->scale > 1) {
		curSz = 32;
		for (int y = 0; y < curSz; y++) { // y line, same in mask and px
			int r = y * sizeof(uint32);
			uint32 clrrow = cur2->clr[r + 0] << 24 | cur2->clr[r + 1] << 16 | cur2->clr[r + 2] << 8 | cur2->clr[r + 3];
			uint32 setrow = cur2->set[r + 0] << 24 | cur2->set[r + 1] << 16 | cur2->set[r + 2] << 8 | cur2->set[r + 3];
			for (int x = 0; x < curSz; x++) { // for each bit / pixel
				uint32 t = 1 << (curSz - x);
				int clr = (t & clrrow), set = (t & setrow);
				if (clr != 0)
					w->cursor.buf[off] = 0xFFFFFFFF;
				if (set != 0)
					w->cursor.buf[off] = 0xFF000000;
				off++;
			}
		}
	} else {
		curSz = 16;
		for (int y = 0; y < curSz; y++) { // y line, same in mask and px
			int r = y * sizeof(uint16);
			uint16 clrrow = cur->clr[r + 0] << 8 | cur->clr[r + 1];
			uint16 setrow = cur->set[r + 0] << 8 | cur->set[r + 1];
			for (int x = 0; x < curSz; x++) { // for each bit / pixel
				uint16 t = 1 << (curSz - x);
				int clr = (t & clrrow), set = (t & setrow);
				if (clr != 0)
					w->cursor.buf[off] = 0xFFFFFFFF;
				if (set != 0)
					w->cursor.buf[off] = 0xFF000000;
				off++;
			}
			off += 16;
		}
	}
	curBuf = wl_shm_pool_create_buffer(w->cursor.pool, 0, curSz, curSz, 32 * sizeof(uint32), WL_SHM_FORMAT_ARGB8888);
	wl_buffer_add_listener(curBuf, &cursor_buffer_listener, nil);
	w->cursor.x = -cur->offset.x;
	w->cursor.y = -cur->offset.y;
	wl_surface_attach(w->cursor.surface, curBuf, 0, 0);
	wl_surface_set_buffer_scale(w->cursor.surface, (w->scale > 1) ? 2 : 1);
	wl_surface_commit(w->cursor.surface);
	wl_pointer_set_cursor(w->wl_pointer, w->m.S, w->cursor.surface, w->cursor.x, w->cursor.y);
}

static void
enter(void *opaque, struct wl_pointer *p, uint32_t serial, struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y)
{
	Client *c;
	window *w;
	struct wl_cursor *cur;
	struct wl_cursor_image *img;
	struct wl_buffer *buf;
	struct wl_surface *t;
	int i, dx, dy;
	char *name;

	c = opaque;
	w = (void *)c->view;
	if (w->m.S > serial)
		return;
	for (i = 0; i < 5; i++) {
		if (i == 4)
			t = w->wl_surface;
		else
			t = w->decoration.edge[i].surface;
		if (surface == t) {
			w->m.in = i;
			goto Found;
		}
	}
	fprint(2, "martian surface\n");
	return;

Found:
	w->m.S = serial;
	w->m.X = wl_fixed_to_int(x);
	w->m.Y = wl_fixed_to_int(y);
	dx = Dx(c->screenimage->r);
	dy = Dy(c->screenimage->r);
	switch (w->m.in) {
	case 0:
		if (w->m.X < 50)
			name = "nw-resize";
		else if ((dx - w->m.X) < 50)
			name = "ne-resize";
		else
			name = "n-resize";
		break;
	case 1:
		if (w->m.X < 50)
			name = "sw-resize";
		else if ((dx - w->m.X) < 50)
			name = "se-resize";
		else
			name = "s-resize";
		break;
	case 2:
		if (w->m.Y < 50)
			name = "nw-resize";
		else if ((dy - w->m.Y) < 50)
			name = "sw-resize";
		else
			name = "w-resize";
		break;
	case 3:
		if (w->m.Y < 50)
			name = "ne-resize";
		else if ((dy - w->m.Y) < 50)
			name = "se-resize";
		else
			name = "e-resize";
		break;
	case 4:
		internal_setcursor(c, nil, nil);
		goto Done;
	}
Again:
	cur = wl_cursor_theme_get_cursor(w->global->wl_cursor_theme, name);
	if (cur == nil) {
		name = "left_ptr";
		goto Again;
	}
	img = cur->images[0];
	w->cursor.x = img->hotspot_x;
	w->cursor.y = img->hotspot_y;
	buf = wl_cursor_image_get_buffer(img);
	wl_surface_attach(w->cursor.surface, buf, 0, 0);
	wl_surface_commit(w->cursor.surface);
	wl_pointer_set_cursor(p, w->m.S, w->cursor.surface, w->cursor.x, w->cursor.y);
Done:
	wl_display_flush(w->global->wl_display);
}

static void
leave(void *opaque, struct wl_pointer *p, uint32_t serial, struct wl_surface *surface)
{
	Client *c = opaque;
	window *w = (void *)c->view;
	w->m.in = -1;
	w->m.S = serial;
	w->m.B = 0;
}

static void
motion(void *opaque, struct wl_pointer *p, uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
	Client *c;
	window *w;

	c = opaque;
	w = (void *)c->view;
	w->m.T = time;
	w->m.X = wl_fixed_to_int(x);
	w->m.Y = wl_fixed_to_int(y);
}

// Linux's pointer button events:
#define BTN_MOUSE 0x110
#define BTN_LEFT 0x110
#define BTN_RIGHT 0x111
#define BTN_MIDDLE 0x112
#define BTN_SIDE 0x113
#define BTN_EXTRA 0x114
#define BTN_FORWARD 0x115
#define BTN_BACK 0x116
#define BTN_TASK 0x117

static void
button(void *opaque, struct wl_pointer *p, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
	Client *c;
	window *w;
	uint32 resize;
	int b;

	c = opaque;
	w = (void *)c->view;
	if (w->m.S > serial) // If we have a martian event, somehow.
		return;

	if (w->m.in < 0 || w->m.in > 4)
		// martian button press.
		return;
	if (w->m.in == 4)
		goto Record;

	resize = 0;
	switch (button) {
	case BTN_MIDDLE:
		xdg_toplevel_move(w->xdg_toplevel, w->global->wl_seat, serial);
		break;
	case BTN_LEFT:
		switch (w->m.in) {
		case 0:
			resize |= XDG_TOPLEVEL_RESIZE_EDGE_TOP;
			break;
		case 1:
			resize |= XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
			break;
		case 2:
			resize |= XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
			break;
		case 3:
			resize |= XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
			break;
		}
		if (w->m.X < 50)
			resize |= XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
		else if ((w->cursize.x - w->m.X) < 50)
			resize |= XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
		if (w->m.Y < 50)
			resize |= XDG_TOPLEVEL_RESIZE_EDGE_TOP;
		else if ((w->cursize.y - w->m.Y) < 50)
			resize |= XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
		xdg_toplevel_resize(w->xdg_toplevel, w->global->wl_seat, serial, resize);
		break;
	case BTN_RIGHT:
		if (w->m.in == 0)
			xdg_toplevel_show_window_menu(w->xdg_toplevel, w->global->wl_seat, serial, w->m.X, w->m.Y);
	}
	return;

Record:
	w->m.T = time;
	switch (button) {
	case BTN_LEFT:
		b = 1;
		break;
	case BTN_MIDDLE:
		b = 2;
		break;
	case BTN_RIGHT:
		b = 4;
		break;
	default:
		fprint(2, "unhandled button event: %x\n", button);
		return;
	};
	switch (state) {
	case WL_POINTER_BUTTON_STATE_PRESSED:
		w->m.B |= b;
		break;
	case WL_POINTER_BUTTON_STATE_RELEASED:
		w->m.B ^= b;
		break;
	default:
		fprint(2, "martian button state: %x\n", state);
	}
}

// The stored mouse coordinates flit between the surfaces, depending on where
// the pointer is. Making sure to only call gfx_mousetrack when the pointer is
// in the application's surface means it should always see its own coordinates.

static void
frame(void *opaque, struct wl_pointer *p)
{
	Client *c;
	window *w;

	c = opaque;
	w = (void *)c->view;
	if (w->m.in)
		gfx_mousetrack(c, w->m.X, w->m.Y, w->m.B, w->m.T);
}

static void
axis(void *opaque, struct wl_pointer *p, uint32 time, uint32 axis, wl_fixed_t val)
{
	Client *c;
	window *w;

	c = opaque;
	w = (void *)c->view;
	if (!w->m.in)
		return;
	w->m.T = time;
	switch (axis) {
	case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
		return;
	case WL_POINTER_AXIS_VERTICAL_SCROLL:
		break;
	default:
		sysfatal("martian axis: %x", axis);
	}
	gfx_mousetrack(c, w->m.X, w->m.Y, w->m.B, w->m.T);
}

static void
axis_discrete(void *opaque, struct wl_pointer *p, uint32 axis, int32 discrete)
{
	Client *c;
	window *w;
	int b;

	c = opaque;
	w = (void *)c->view;
	if (!w->m.in)
		return;
	switch (axis) {
	case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
		return;
	case WL_POINTER_AXIS_VERTICAL_SCROLL:
		break;
	default:
		sysfatal("martian axis: %x", axis);
	}
	b = w->m.B;
	if (discrete > 0)
		b |= 16;
	else {
		discrete *= -1;
		b |= 8;
	}
	for (int i = 0; i < discrete; i++) {
		gfx_mousetrack(c, w->m.X, w->m.Y, b, w->m.T);
		gfx_mousetrack(c, w->m.X, w->m.Y, w->m.B, w->m.T);
	}
}

static void
axis_source(void *opaque, struct wl_pointer *p, uint32 src)
{
}

static void
axis_stop(void *opaque, struct wl_pointer *p, uint32 time, uint32 axis)
{
}

