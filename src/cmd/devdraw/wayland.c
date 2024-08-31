#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>
#include <thread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "bigarrow.h"
#include "devdraw.h"
#include "wayland-pointer-constraints.h"
#include "wayland-xdg-decoration.h"
#include "wayland-xdg-shell.h"

// alt+click and ctl+click are mapped to mouse buttons
// to support single button mice.
#define ALT_BUTTON 1
#define CTL_BUTTON 2

struct WaylandBuffer {
	int w;
	int h;
	int size;
	char *data;
	struct wl_buffer* wl_buffer;
};
typedef struct WaylandBuffer WaylandBuffer;

struct WaylandClient {
	// The screen image written to by the client, and read by this driver.
	Memimage *memimage;

	// The current mouse coordinates and a bitmask of held buttons.
	int mouse_x;
	int mouse_y;
	int buttons;

	// Booleans indicating whether control or alt are currently held.
	int ctl;
	int alt;

	// State for key repeat for keyboard keys.
	int repeat_rune;
	int repeat_next_ms;

	// State for "key repeat" for the mouse scroll wheel.
	// This allows touchpad devices to have accelerated scrolling.
	int repeat_scroll_button;
	int repeat_scroll_count;
	int repeat_scroll_rate_ms;
	int repeat_scroll_next_ms;

	// The Wayland surface for this window
	// and its corresponding xdg objects.
	struct wl_surface *wl_surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;

	// A callback when wl_surface is ready for the next frame.
	// Currently this is not used for drawing,
	// but as a hack for implementing key repeat
	// without needing to implement our own timer thread/events.
	struct wl_callback *wl_callback;

	// The mouse pointer and the surface for the current cursor.
	struct wl_pointer *wl_pointer;
	struct wl_surface *wl_surface_cursor;

	// The keyboard and xkb state used
	// for mapping scan codes to key codes.
	struct wl_keyboard *wl_keyboard;
	struct xkb_context *xkb_context;
	struct xkb_keymap *xkb_keymap;
	struct xkb_state *xkb_state;
};
typedef struct WaylandClient WaylandClient;

static QLock wayland_lock;

// Required globals wayland objects.
static struct wl_display *wl_display;
static struct wl_output *wl_output;
static struct wl_registry *wl_registry;
static struct wl_shm *wl_shm;
static struct wl_compositor *wl_compositor;
static struct xdg_wm_base *xdg_wm_base;
static struct wl_seat *wl_seat;
static struct wl_data_device_manager *wl_data_device_manager;
static struct wl_data_device *wl_data_device;

static char *snarf;
uint32_t keyboard_enter_serial;

// Optional global wayland objects.
// Need to NULL check them before using.
static struct zxdg_decoration_manager_v1 *decoration_manager;
static struct zwp_pointer_constraints_v1 *pointer_constraints;

// The wl output scale factor reported by wl_output.
// We only set it if we get th event before entering the graphics loop.
// Once we enter the loop, we never change it to avoid the need
// to reason about which scale a buffer was created with.
int wl_output_scale_factor = 1;
int entered_gfx_loop = 0;

// The delay in ms which a key must be held to begin repeating..
int key_repeat_delay_ms = 500;
// The number of ms between repeats of a repeating key.
int key_repeat_ms = 100;
// The maximum number of ms between repeats of a scroll.
// This is scaled by the number of repeating scrolls pending.
int scroll_repeat_ms = 100;

// A pool of xrgb888 buffers used for drawing to the screen.
// When drawing, we give ownership of the buffer's memory to the compositor.
// The compositor notifies us asynchronously when it is done reading the buffer.
// In the case that we need to draw (rpc_flush) before the buffer is ready,
// we will need to allocate a whole new buffer to draw to.
// We use this pool to avoid the need to setup a new shared memory buffer
// each time this happens.
#define N_XRGB8888_BUFFERS 3
struct WaylandBuffer *xrgb8888_buffers[N_XRGB8888_BUFFERS];

int wayland_debug = 0;

#define DEBUG(...)					\
do {								\
	if (wayland_debug) {			\
		fprint(2, __VA_ARGS__);	\
	}							\
} while(0)

static void registry_global(void *data, struct wl_registry *wl_registry,
	uint32_t name, const char *interface, uint32_t version) {
	if (strcmp(interface, wl_output_interface.name) == 0) {
		wl_output = wl_registry_bind(wl_registry, name, &wl_output_interface, 2);

	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		wl_shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, 1);

	} else if (strcmp(interface, wl_compositor_interface.name) == 0) {
		wl_compositor = wl_registry_bind(wl_registry, name,
			&wl_compositor_interface, 4);

	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		xdg_wm_base = wl_registry_bind(wl_registry, name,
			&xdg_wm_base_interface, 1);

	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		wl_seat = wl_registry_bind(wl_registry, name, &wl_seat_interface, 1);

	} else if (strcmp(interface, wl_data_device_manager_interface.name) == 0) {
		wl_data_device_manager = wl_registry_bind(wl_registry, name, &wl_data_device_manager_interface, 2);

	} else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0) {
		decoration_manager = wl_registry_bind(wl_registry, name,
			&zxdg_decoration_manager_v1_interface, 1);

	} else if (strcmp(interface, zwp_pointer_constraints_v1_interface.name) == 0) {
		pointer_constraints = wl_registry_bind(wl_registry, name,
			&zwp_pointer_constraints_v1_interface, 1);
	}
}

static void registry_global_remove(void *data, struct wl_registry *wl_registry,
	uint32_t name) {}

static const struct wl_registry_listener wl_registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

void wl_output_geometry(void *data, struct wl_output *wl_output,
	int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
	int32_t subpixel, const char *make, const char *model, int32_t transform) {}

void wl_output_mode(void *data, struct wl_output *wl_output, uint32_t flags,
	int32_t width, int32_t height, int32_t refresh) {}

void wl_output_done(void *data, struct wl_output *wl_output) {}

void wl_output_scale(void *data, struct wl_output *wl_output, int32_t factor) {
	DEBUG("wl_output_scale(factor=%d)\n", factor);

	qlock(&wayland_lock);

	if (!entered_gfx_loop) {
		wl_output_scale_factor = factor;
	}

	qunlock(&wayland_lock);
}

static const struct wl_output_listener wl_output_listener = {
	.geometry = wl_output_geometry,
	.mode = wl_output_mode,
	.done = wl_output_done,
	.scale = wl_output_scale,
};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
	xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
	.ping = xdg_wm_base_ping,
};

void wl_data_device_listener_data_offer(void *data,
	struct wl_data_device *wl_data_device, struct wl_data_offer *id) {}

void wl_data_device_listener_data_enter(void *data,
	struct wl_data_device *wl_data_device, uint32_t serial,
	struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y,
	struct wl_data_offer *id) {}

void wl_data_device_listener_data_leave(void *data,
	struct wl_data_device *wl_data_device) {}

void wl_data_device_listener_data_motion(void *data,
	struct wl_data_device *wl_data_device, uint32_t time,
	wl_fixed_t x, wl_fixed_t y) {}

void wl_data_device_listener_data_drop(void *data,
	struct wl_data_device *wl_data_device) {}

void wl_data_device_listener_selection(void *data,
	struct wl_data_device *wl_data_device, struct wl_data_offer *id) {
	DEBUG("wl_data_device_listener_selection\n");

	if (id == NULL) {
		qlock(&wayland_lock);
		free(snarf);
		snarf = NULL;
		qunlock(&wayland_lock);
		DEBUG("wl_data_device_listener_selection: no data\n");
		return;
	}

	int fds[2];
	if (pipe(fds) < 0) {
		sysfatal("Failed to create pipe");
	}
	wl_data_offer_receive(id, "text/plain", fds[1]);
	close(fds[1]);
	wl_display_roundtrip(wl_display);

	qlock(&wayland_lock);

	int total = 0;
	snarf = NULL;
	for (; ;) {
		char buf[128];
		int n = read(fds[0], &buf, sizeof(buf));
		if (n < 0 && errno == EAGAIN) {
			continue;
		}
		if (n < 0) {
			sysfatal("Read failed");
		}
		if (n == 0) {
			break;
		}
		// +1 to ensure it's always at least null terminated.
		char *tmp = calloc(1, total + n + 1);
		if (snarf != NULL) {
			strncpy(tmp, snarf, total);
		}
		memcpy(tmp+total, buf, n);
		total += n;
		snarf = tmp;
	}

	DEBUG("wl_data_device_listener_selection: read [%s]\n", snarf);
	qunlock(&wayland_lock);
	close(fds[0]);
}

static const struct wl_data_device_listener wl_data_device_listener = {
	.data_offer = wl_data_device_listener_data_offer,
	.enter = wl_data_device_listener_data_enter,
	.leave = wl_data_device_listener_data_leave,
	.motion = wl_data_device_listener_data_motion,
	.drop = wl_data_device_listener_data_drop,
	.selection = wl_data_device_listener_selection,
};

void wl_data_source_target(void *data,
	struct wl_data_source *wl_data_source,
	const char *mime_type) {}

void wl_data_source_send(void *data,
	struct wl_data_source *wl_data_source,
	const char *mime_type, int32_t fd) {
	DEBUG("wl_data_source_send(mime_type=%s)\n", mime_type);

	if (strcmp(mime_type, "text/plain") != 0) {
		DEBUG("unknown mime type\n");
		close(fd);
		return;
	}

	qlock(&wayland_lock);

	int total = 0;
	if (snarf != NULL) {
		total = strlen(snarf);
	}
	DEBUG("wl_data_source_send: writing %d bytes\n", total);
	char *p = snarf;
	while (total > 0) {
		int n = write(fd, p, total);
		if (n < 0 && errno == EAGAIN) {
			continue;
		}
		if (n < 0) {
			sysfatal("Write error");
		}
		p += n;
		total -= n;
	}

	qunlock(&wayland_lock);
	close(fd);
}

void wl_data_source_cancelled(void *data, struct wl_data_source *wl_data_source) {}

static const struct wl_data_source_listener wl_data_source_listener = {
	.target = wl_data_source_target,
	.send = wl_data_source_send,
	.cancelled = wl_data_source_cancelled,
};

void delete_buffer(WaylandBuffer *b) {
	munmap(b->data, b->size);
	wl_buffer_destroy(b->wl_buffer);
	free(b);
}

static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer) {
	if (data == NULL) {
		wl_buffer_destroy(wl_buffer);
		return;
	}
	for (int i = 0; i < N_XRGB8888_BUFFERS; i++) {
		if (xrgb8888_buffers[i] == NULL) {
			xrgb8888_buffers[i] = (WaylandBuffer*) data;
			return;
		}
	}
	delete_buffer((WaylandBuffer*) data);
}

static const struct wl_buffer_listener wl_buffer_listener = {
	.release = wl_buffer_release,
};

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
	DEBUG("xdg_surface_configure\n");
	const Client* c = data;
	const WaylandClient *wl = c->view;
	qlock(&wayland_lock);

	xdg_surface_ack_configure(wl->xdg_surface, serial);

	qunlock(&wayland_lock);
	DEBUG("xdg_surface_configure: returned\n");
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_configure,
};

void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
	int32_t width, int32_t height, struct wl_array *states) {
	DEBUG("xdg_toplevel_configure(width=%d, height=%d)\n", width, height);
	if (width == 0 || height == 0) {
		return;
	}
	Client* c = data;
	WaylandClient *wl = (WaylandClient*) c->view;
	qlock(&wayland_lock);

	width *= wl_output_scale_factor;
	height *= wl_output_scale_factor;
	Rectangle r = Rect(0, 0, width, height);
	if (eqrect(r, wl->memimage->r)) {
		// The size didn't change, so nothing to do.
		qunlock(&wayland_lock);
		return;
	}

	// The size changed, so allocate a new Memimage and notify the client.
	wl->memimage = _allocmemimage(r, XRGB32);
	c->mouserect = r;

	qunlock(&wayland_lock);
	gfx_replacescreenimage(c, wl->memimage);
}

void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
	DEBUG("xdg_toplevel_close\n");
	threadexitsall(nil);
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_toplevel_configure,
	.close = xdg_toplevel_close,
};

static const struct wl_callback_listener wl_callback_listener;

// The callback is called per-frame.
// It is currently only used to implement key repeat.
static void wl_callback_done(void *data, struct wl_callback *wl_callback, uint32_t time) {
	Client* c = data;
	WaylandClient *wl = (WaylandClient*) c->view;
	qlock(&wayland_lock);

	// Request another callback for the next frame.
	// TODO: Only request a callback if we are still repeating a key.
	wl_callback_destroy(wl_callback);
	wl_callback = wl_surface_frame(wl->wl_surface);
	wl_callback_add_listener(wl_callback, &wl_callback_listener, c);
	wl_surface_commit(wl->wl_surface);

	int repeat_rune = 0;
	if (wl->repeat_rune && time >= wl->repeat_next_ms) {
		repeat_rune = wl->repeat_rune;
		wl->repeat_next_ms = time + key_repeat_ms;
	}

	int x = wl->mouse_x;
	int y = wl->mouse_y;
	int repeat_scroll_button = 0;
	if (wl->repeat_scroll_button && time >= wl->repeat_scroll_next_ms) {
		repeat_scroll_button = wl->repeat_scroll_button | wl->buttons;
		wl->repeat_scroll_count--;
		if (wl->repeat_scroll_count == 0) {
			wl->repeat_scroll_button = 0;
		}
		wl->repeat_scroll_next_ms = time + scroll_repeat_ms/wl->repeat_scroll_count;
	}

	qunlock(&wayland_lock);
	if (repeat_rune) {
		gfx_keystroke(c, repeat_rune);
	}
	if (repeat_scroll_button) {
		gfx_mousetrack(c, x, y, repeat_scroll_button, (uint) time);
	}
}

static const struct wl_callback_listener wl_callback_listener = {
	.done = wl_callback_done,
};

void wl_pointer_enter(void *data,struct wl_pointer *wl_pointer, uint32_t serial,
	struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	Client* c = data;
	WaylandClient *wl = (WaylandClient*) c->view;
	qlock(&wayland_lock);

	wl->mouse_x = wl_fixed_to_int(surface_x) * wl_output_scale_factor;
	wl->mouse_y = wl_fixed_to_int(surface_y) * wl_output_scale_factor;

	wl_pointer_set_cursor(wl->wl_pointer, serial, wl->wl_surface_cursor, 0, 0);

	qunlock(&wayland_lock);
	// We don't call gfx_mousetrack here, since we don't have the time.
}

void wl_pointer_leave(void *data, struct wl_pointer *wl_pointer,
	uint32_t serial, struct wl_surface *surface) {
	Client* c = data;
	WaylandClient *wl = (WaylandClient*) c->view;
	qlock(&wayland_lock);

	wl->buttons = 0;
	wl->repeat_scroll_button = 0;

	qunlock(&wayland_lock);
}

void wl_pointer_motion(void *data, struct wl_pointer *wl_pointer, uint32_t time,
	wl_fixed_t surface_x, wl_fixed_t surface_y){
	Client* c = data;
	WaylandClient *wl = (WaylandClient*) c->view;
	qlock(&wayland_lock);

	wl->repeat_scroll_button = 0;
	wl->repeat_scroll_count = 0;
	wl->mouse_x = wl_fixed_to_int(surface_x) * wl_output_scale_factor;
	wl->mouse_y = wl_fixed_to_int(surface_y) * wl_output_scale_factor;
	int x = wl->mouse_x;
	int y = wl->mouse_y;
	int b = wl->buttons;

	qunlock(&wayland_lock);
	gfx_mousetrack(c, x, y, b, (uint) time);
}

void wl_pointer_button(void *data, struct wl_pointer *wl_pointer, uint32_t serial,
	uint32_t time, uint32_t button, uint32_t state) {
	DEBUG("wl_pointer_button(button=%d)\n", (int) button);
	Client* c = data;
	WaylandClient *wl = (WaylandClient*) c->view;
	qlock(&wayland_lock);

	wl->repeat_scroll_button = 0;
	wl->repeat_scroll_count = 0;

	int mask = 0;
	switch (button) {
	case BTN_LEFT:
		mask = 1<<0;
		break;
	case BTN_MIDDLE:
		mask = 1<<1;
		break;
	case BTN_RIGHT:
		mask = 1<<2;
		break;
	case BTN_4:
		mask = 1<<3;
		break;
	case BTN_5:
		mask = 1<<4;
		break;
	default:
		DEBUG("wl_pointer_button: unknown button: %d\n", button);
		qunlock(&wayland_lock);
		return;
	}
	if (button == BTN_LEFT) {
		if (wl->ctl) {
			mask = 1 << CTL_BUTTON;
		} else if (wl->alt) {
			mask = 1 << ALT_BUTTON;
		}
	}
	DEBUG("wl_pointer_button: mask=%x\n", mask);

	switch (state) {
	case WL_POINTER_BUTTON_STATE_PRESSED:
		wl->buttons |= mask;
		break;
	case WL_POINTER_BUTTON_STATE_RELEASED:
		wl->buttons &= ~mask;
		break;
	default:
		fprint(2, "Unknown button state: %d\n", state);
	}
	int x = wl->mouse_x;
	int y = wl->mouse_y;
	int b = wl->buttons;

	qunlock(&wayland_lock);
	DEBUG("wl_pointer_button: gfx_trackmouse(x=%d, y=%d, b=%d)\n", x, y, b);
	gfx_mousetrack(c, x, y, b, (uint) time);
}

void wl_pointer_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time,
	uint32_t axis, wl_fixed_t value_fixed) {
	double value = wl_fixed_to_double(value_fixed);
	Client* c = data;
	WaylandClient *wl = (WaylandClient*) c->view;
	qlock(&wayland_lock);

	int x = wl->mouse_x;
	int y = wl->mouse_y;
	wl->repeat_scroll_button = 0;
	wl->repeat_scroll_count = 0;

	int b = 0;
	if (value < 0) {
		b |= 1 << 3;
	} else if (value > 0) {
		b |= 1 << 4;
	}
	int mag = fabs(value);
	if (mag > 1) {
		wl->repeat_scroll_button = b;
		wl->repeat_scroll_count = mag;
		wl->repeat_scroll_next_ms = time + scroll_repeat_ms/wl->repeat_scroll_count;
	}
	b |= wl->buttons;

	qunlock(&wayland_lock);
	gfx_mousetrack(c, x, y, b, (uint) time);
}


static const struct wl_pointer_listener pointer_listener = {
	.enter = wl_pointer_enter,
	.leave = wl_pointer_leave,
	.motion = wl_pointer_motion,
	.button = wl_pointer_button,
	.axis = wl_pointer_axis,
};

void wl_keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
	uint32_t format, int32_t fd, uint32_t size) {
	DEBUG("wl_keyboard_keymap\n");
	char *keymap = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	Client* c = data;
	WaylandClient *wl = (WaylandClient*) c->view;
	qlock(&wayland_lock);

	if (wl->xkb_keymap != NULL) {
		xkb_keymap_unref(wl->xkb_keymap);
	}
	wl->xkb_keymap = xkb_keymap_new_from_string(wl->xkb_context, keymap,
		XKB_KEYMAP_FORMAT_TEXT_V1,
		XKB_KEYMAP_COMPILE_NO_FLAGS);

	if (wl->xkb_state != NULL) {
		xkb_state_unref(wl->xkb_state);
	}
	wl->xkb_state = xkb_state_new(wl->xkb_keymap);

	qunlock(&wayland_lock);
    	munmap(keymap, size);
	close(fd);
}

void wl_keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
	uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
	DEBUG("wl_keyboard_enter\n");
	qlock(&wayland_lock);
	keyboard_enter_serial = serial;
	qunlock(&wayland_lock);
}

void wl_keyboard_leave(void *data, struct wl_keyboard *wl_keyboard,
	uint32_t serial, struct wl_surface *surface) {
	DEBUG("wl_keyboard_leave\n");
	Client* c = data;
	WaylandClient *wl = (WaylandClient*) c->view;
	qlock(&wayland_lock);

	wl->ctl = 0;
	wl->alt = 0;
	wl->repeat_rune = 0;

	qunlock(&wayland_lock);
}

void wl_keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
	uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
	Client* c = data;
	WaylandClient *wl = (WaylandClient*) c->view;
	qlock(&wayland_lock);

	wl->repeat_rune = 0;
	wl->repeat_scroll_button = 0;
	wl->repeat_scroll_count = 0;

	key += 8;	// Add 8 to translate Linux scan code to xkb code.
	uint32_t rune = xkb_state_key_get_utf32(wl->xkb_state, key);
	xkb_keysym_t keysym = xkb_state_key_get_one_sym(wl->xkb_state, key);

	if (wayland_debug) {
		char name[256];
		xkb_keysym_get_name(keysym, &name[0], 256);
		char *state_str = WL_KEYBOARD_KEY_STATE_PRESSED ? "down" : "up";
		DEBUG("wl_keyboard_key: keysym=%s, rune=0x%x, state=%s\n",
			name, rune, state_str);
	}

	switch (keysym) {
	case XKB_KEY_Return:
		rune = '\n';
		break;
	case XKB_KEY_Alt_L:
	case XKB_KEY_Alt_R:
		rune = Kalt;
		if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
			wl->alt = 1;
		} else {
			wl->alt = 0;
		}
		if (wl->buttons) {
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
				wl->buttons |= 1 << ALT_BUTTON;
			} else {
				wl->buttons &= ~(1 << ALT_BUTTON);
			}
			int x = wl->mouse_x;
			int y = wl->mouse_y;
			int b = wl->buttons;

			qunlock(&wayland_lock);
			gfx_mousetrack(c, x, y, b, (uint) time);
			return;
		}
		break;
	case XKB_KEY_Control_L:
	case XKB_KEY_Control_R:
		// For some reason, Kctl is not used;
		// it results in drawing a replacement character.
		// Common ctl combos still work.
		// For example ctl+w sends rune 0x17
		// which erases the previous word.
		rune = 0; // Kctl;
		if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
			wl->ctl = 1;
		} else {
			wl->ctl = 0;
		}
		if (wl->buttons) {
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
				wl->buttons |= 1 << CTL_BUTTON;
			} else {
				wl->buttons &= ~(1 << CTL_BUTTON);
			}
			int x = wl->mouse_x;
			int y = wl->mouse_y;
			int b = wl->buttons;

			qunlock(&wayland_lock);
			gfx_mousetrack(c, x, y, b, (uint) time);
			return;
		}
		break;
	case XKB_KEY_Delete:
		rune = Kdel;
		break;
	case XKB_KEY_Escape:
		rune = Kesc;
		break;
	case XKB_KEY_Home:
		rune = Khome;
		break;
	case XKB_KEY_End:
		rune = Kend;
		break;
	case XKB_KEY_Prior:
		rune = Kpgup;
		break;
	case XKB_KEY_Next:
		rune = Kpgdown;
		break;
	case XKB_KEY_Up:
		rune = Kup;
		break;
	case XKB_KEY_Down:
		rune = Kdown;
		break;
	case XKB_KEY_Left:
		rune = Kleft;
		break;
	case XKB_KEY_Right:
		rune = Kright;
		break;
	}

	qunlock(&wayland_lock);
	if (state == WL_KEYBOARD_KEY_STATE_PRESSED && rune != 0) {
		wl->repeat_rune = rune;
		wl->repeat_next_ms = time + key_repeat_delay_ms;
		gfx_keystroke(c, rune);
	}
}

void wl_keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard,
	uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
	uint32_t mods_locked, uint32_t group) {
	DEBUG("wl_keyboard_modifiers\n");
	Client* c = data;
	WaylandClient *wl = (WaylandClient*) c->view;
	qlock(&wayland_lock);

	xkb_state_update_mask(wl->xkb_state, mods_depressed,
		mods_latched, mods_locked, 0, 0, group);

	qunlock(&wayland_lock);
}

// TODO: Use this to set key repeat.
// Currently we don't bind the keyboard with the correct version to get this event.
void wl_keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
	int32_t rate, int32_t delay) {
	DEBUG("wl_keyboard_repeat_info(rate=%d, delay=%d)\n",
		(int) rate, (int) delay);
}

static const struct wl_keyboard_listener keyboard_listener = {
	.keymap = wl_keyboard_keymap,
	.enter = wl_keyboard_enter,
	.leave = wl_keyboard_leave,
	.key = wl_keyboard_key,
	.modifiers = wl_keyboard_modifiers,
	.repeat_info = wl_keyboard_repeat_info,
};

void	gfx_main(void) {
	DEBUG("gfx_main called\n");

	wl_display = wl_display_connect(NULL);
	wl_registry = wl_display_get_registry(wl_display);
	wl_registry_add_listener(wl_registry, &wl_registry_listener, NULL);
	wl_display_roundtrip(wl_display);

	// Ensure required globals were correctly bound.
	if (wl_display == NULL) {
		sysfatal("Unable to get Wayland display");
	}
	if (wl_registry == NULL) {
		sysfatal("Unable to get Wayland registry");
	}
	if (wl_output == NULL) {
		sysfatal("Unable to bind wl_output");
	}
	if (wl_shm == NULL) {
		sysfatal("Unable to bind wl_shm");
	}
	if (wl_compositor == NULL) {
		sysfatal("Unable to bind wl_compositor");
	}
	if (xdg_wm_base == NULL) {
		sysfatal("Unable to bind xdg_wm_base");
	}
	if (wl_seat == NULL) {
		sysfatal("Unable to bind wl_seat");
	}
	if (wl_data_device_manager == NULL) {
		sysfatal("Unable to bind wl_data_device_manager");
	}
	wl_output_add_listener(wl_output, &wl_output_listener, NULL);
	xdg_wm_base_add_listener(xdg_wm_base, &xdg_wm_base_listener, NULL);
	wl_data_device = wl_data_device_manager_get_data_device(
		wl_data_device_manager, wl_seat);
	wl_data_device_add_listener(wl_data_device, &wl_data_device_listener, NULL);
	wl_display_roundtrip(wl_display);

	entered_gfx_loop = 1;
	gfx_started();
	DEBUG("gfx_main: entering loop\n");
	while (wl_display_dispatch(wl_display))
		;
}

static void rpc_resizeimg(Client*) {
	DEBUG("rpc_resizeimg\n");
}

static void rpc_resizewindow(Client*, Rectangle) {
	DEBUG("rpc_resizewindow\n");
}

static int next_shm = 0;

WaylandBuffer *new_buffer(int w, int h, int format) {
	int stride = w * 4;
	int size = stride * h;

	// Create an anonymous shared memory file.
	char name[128];
	snprintf(name, 128, "/acme_wl_shm-%d-%d", getpid(), next_shm++);
	int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
	if (fd < 0) {
		sysfatal("shm_open failed");
	}
	shm_unlink(name);

	// Set the file's size.
	int ret;
	do {
		ret = ftruncate(fd, size);
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		sysfatal("ftruncate failed");
	}

	char *d = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (d == MAP_FAILED) {
		sysfatal("mmap failed");
	}

	WaylandBuffer *b = malloc(sizeof(WaylandBuffer));
	b->w = w;
	b->h = h;
	b->size = size;
	b->data = d;
	struct wl_shm_pool *p = wl_shm_create_pool(wl_shm, fd, size);
	b->wl_buffer = wl_shm_pool_create_buffer(p, 0, w, h, stride, format);
	wl_shm_pool_destroy(p);
	close(fd);
	return b;
}

void wayland_set_cursor(WaylandClient *wl, Cursor *cursor) {
	// Convert bitmap to ARGB.
	// Yes, this is super clunky. Sorry about that.
	const uint32_t fg = 0xFF000000;
	const uint32_t a = 0x00FFFFFF;
	uint32_t data[8*32];
	int j = 0;
	for (int i = 0; i < 32; i++) {
		char c = cursor->set[i];
		data[j++] = (c >>7) & 1 == 1 ? fg : a;
		data[j++] = (c >> 6) & 1 == 1 ? fg : a;
		data[j++] = (c >> 5) & 1 == 1 ? fg : a;
		data[j++] = (c >> 4) & 1 == 1 ? fg : a;
		data[j++] = (c >> 3) & 1 == 1 ? fg : a;
		data[j++] = (c >> 2) & 1 == 1 ? fg : a;
		data[j++] = (c >> 1) & 1 == 1 ? fg : a;
		data[j++] = (c >> 0) & 1 == 1 ? fg : a;
	}

	WaylandBuffer *b = new_buffer(16, 16, WL_SHM_FORMAT_ARGB8888);
	memcpy(b->data, (char*) &data[0], b->size);

	// We don't want to bother saving this buffer in xrgb8888_buffers.
	// Unmap and use NULL for it's listener data.
	// This will cause it to be destroyed when it is released.
	munmap(b->data, b->size);
	wl_buffer_add_listener(b->wl_buffer, &wl_buffer_listener, NULL);

	wl_surface_attach(wl->wl_surface_cursor, b->wl_buffer, 0, 0);
	wl_surface_damage_buffer(wl->wl_surface_cursor, 0, 0, 16, 16);
	wl_surface_commit(wl->wl_surface_cursor);
}

static void rpc_setcursor(Client *c, Cursor *cursor, Cursor2*) {
	DEBUG("rpc_setcursor\n");
	WaylandClient *wl = (WaylandClient*) c->view;
	qlock(&wayland_lock);

	if (cursor == NULL) {
		cursor = &bigarrow;
	}
	wayland_set_cursor(wl, cursor);

	qunlock(&wayland_lock);
}

static void rpc_setlabel(Client *c, char *label) {
	WaylandClient *wl = (WaylandClient*) c->view;
	qlock(&wayland_lock);

	xdg_toplevel_set_title(wl->xdg_toplevel, label);

	qunlock(&wayland_lock);
}

static void rpc_setmouse(Client *c, Point p) {
	if (pointer_constraints == NULL) {
		// If there is no pointer constraints extension,
		// we cannot warp the mouse.
		return;
	}
	WaylandClient *wl = (WaylandClient*) c->view;
	qlock(&wayland_lock);

	// Wayland does not directly support warping the pointer.
	// Instead, we use (misuse?) the pointer constraints extension,
	// which allows sending the compositor a new pointer location
	// hint when the pointer is unlocked.
	// We lock the pointer, and immediately unlock it with a hint
	// of the desired wrap location.

	struct zwp_locked_pointer_v1 *lock = zwp_pointer_constraints_v1_lock_pointer(
		pointer_constraints, wl->wl_surface, wl->wl_pointer, NULL,
		ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
	int x = wl_fixed_from_int(p.x / wl_output_scale_factor);
	int y = wl_fixed_from_int(p.y / wl_output_scale_factor);
	zwp_locked_pointer_v1_set_cursor_position_hint(lock, x, y);
	wl_surface_commit(wl->wl_surface);
	zwp_locked_pointer_v1_destroy(lock);

	qunlock(&wayland_lock);
}

static void rpc_topwin(Client*) {
	DEBUG("rpc_topwin\n");
}

static void rpc_bouncemouse(Client*, Mouse) {
	DEBUG("rpc_bouncemouse\n");
}

// Must be called with the lock held.
WaylandBuffer *get_xrgb8888_buffer(int w, int h) {
	for (int i = 0; i < N_XRGB8888_BUFFERS; i++) {
		if (xrgb8888_buffers[i] == NULL) {
			continue;
		}
		// Delete any cached buffers that are not the right size.
		if (xrgb8888_buffers[i]->w != w || xrgb8888_buffers[i]->h != h) {
			delete_buffer(xrgb8888_buffers[i]);
			xrgb8888_buffers[i] = NULL;
			continue;
		}
		WaylandBuffer *b = xrgb8888_buffers[i];
		xrgb8888_buffers[i] = NULL;
		return b;
	}
	WaylandBuffer *b = new_buffer(w, h, WL_SHM_FORMAT_XRGB8888);
	wl_buffer_add_listener(b->wl_buffer, &wl_buffer_listener, b);
	return b;
}

static void rpc_flush(Client *c, Rectangle r) {
	WaylandClient *wl = (WaylandClient*) c->view;
	qlock(&wayland_lock);

	int w = Dx(wl->memimage->r);
	int h = Dy(wl->memimage->r);
	WaylandBuffer *b = get_xrgb8888_buffer(w, h);
	memcpy(b->data, (char*) wl->memimage->data->bdata, b->size);
	wl_surface_attach(wl->wl_surface, b->wl_buffer, 0, 0);
	wl_surface_damage_buffer(wl->wl_surface, r.min.x, r.min.y, Dx(r), Dy(r));
	wl_surface_commit(wl->wl_surface);
	wl_display_flush(wl_display);

	qunlock(&wayland_lock);
}

static ClientImpl wayland_impl = {
	rpc_resizeimg,
	rpc_resizewindow,
	rpc_setcursor,
	rpc_setlabel,
	rpc_setmouse,
	rpc_topwin,
	rpc_bouncemouse,
	rpc_flush
};

Memimage *rpc_attach(Client *c, char *label, char *winsize) {
	DEBUG("rpc_attach(%s)\n", label);

	qlock(&wayland_lock);

	WaylandClient *wl = calloc(1, sizeof(WaylandClient));
	c->impl = &wayland_impl;
	c->view = wl;

	wl->wl_surface = wl_compositor_create_surface(wl_compositor);

	wl->xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, wl->wl_surface);
	xdg_surface_add_listener(wl->xdg_surface, &xdg_surface_listener, c);

	wl->xdg_toplevel = xdg_surface_get_toplevel(wl->xdg_surface);
	xdg_toplevel_add_listener(wl->xdg_toplevel, &xdg_toplevel_listener, c);
	xdg_toplevel_set_title(wl->xdg_toplevel, label);

	wl->wl_callback = wl_surface_frame(wl->wl_surface);
	wl_callback_add_listener(wl->wl_callback, &wl_callback_listener, c);

	wl->wl_pointer = wl_seat_get_pointer(wl_seat);
	wl_pointer_add_listener(wl->wl_pointer, &pointer_listener, c);
	wl->wl_surface_cursor = wl_compositor_create_surface(wl_compositor);
	wayland_set_cursor(wl, &bigarrow);

	wl->wl_keyboard = wl_seat_get_keyboard(wl_seat);
	wl_keyboard_add_listener(wl->wl_keyboard, &keyboard_listener, c);
	wl->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

	// If the xdg decorations extension is available,
	// enable server-side decorations.
	// Otherwise there will be no window decorations
	// (title, resize, buttons, etc.).
	if (decoration_manager != NULL) {
		struct zxdg_toplevel_decoration_v1 *d =
			zxdg_decoration_manager_v1_get_toplevel_decoration(
				decoration_manager, wl->xdg_toplevel);
		zxdg_toplevel_decoration_v1_set_mode(d,
			ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}

	// TODO: parse winsize.
	int w = 640*wl_output_scale_factor;
	int h = 480*wl_output_scale_factor;
	Rectangle r = Rect(0, 0, w, h);
	wl->memimage = _allocmemimage(r, XRGB32);
	c->mouserect = r;
	c->displaydpi = 110 * wl_output_scale_factor;
	wl_surface_set_buffer_scale(wl->wl_surface, wl_output_scale_factor);
	wl_surface_commit(wl->wl_surface);

	qunlock(&wayland_lock);
	return wl->memimage;
}

char *rpc_getsnarf(void) {
	DEBUG("rpc_getsnarf\n");
	qlock(&wayland_lock);

	if (snarf == NULL) {
		qunlock(&wayland_lock);
		return NULL;
	}

	int n = strlen(snarf);
	char *copy = calloc(1, n+1);
	strncpy(copy, snarf, n);
	qunlock(&wayland_lock);
	return copy;
}

void	rpc_putsnarf(char *snarf_in) {
	DEBUG("rpc_putsnarf\n");
	qlock(&wayland_lock);

	int n = strlen(snarf_in);
	free(snarf);
	snarf = calloc(1, n+1);
	strncpy(snarf, snarf_in, n);

	struct wl_data_source *source =
		wl_data_device_manager_create_data_source(wl_data_device_manager);
	wl_data_source_add_listener(source, &wl_data_source_listener, NULL);
	wl_data_source_offer(source, "text/plain");
	wl_data_device_set_selection(wl_data_device, source, keyboard_enter_serial);

	qunlock(&wayland_lock);
}

void	rpc_shutdown(void) {
	DEBUG("rpc_shutdown\n");
}

void rpc_gfxdrawlock(void) {
	qlock(&wayland_lock);
}

void rpc_gfxdrawunlock(void) {
	qunlock(&wayland_lock);
}

int cloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata) {
	return _cloadmemimage(i, r, data, ndata);
}

int loadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata) {
	return _loadmemimage(i, r, data, ndata);
}

int unloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata) {
	return _unloadmemimage(i, r, data, ndata);
}
