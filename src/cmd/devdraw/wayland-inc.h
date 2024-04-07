#include <u.h>
#include <libc.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>

#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-client-protocol.h"
#include "pointer-constraints-client-protocol.h"

struct bounds {
	int x, y;
};

struct output_elem {
	struct wl_output *o;
	int scale, dpi, on;
	double px, mm;
	struct wl_list link;
};

typedef struct Globals {
	struct wl_display *wl_display;
	struct wl_registry *wl_registry;
	struct wl_shm *wl_shm;
	struct wl_cursor_theme *wl_cursor_theme;
	struct wl_compositor *wl_compositor;
	struct wl_subcompositor *wl_subcompositor;
	struct xdg_wm_base *xdg_wm_base;
	struct wl_seat *wl_seat;
	struct wl_data_device_manager *data_device_manager;
	struct wl_data_device *data_device;
	struct wl_data_offer *snarf_offer;
	int snarf_fd;
	char *snarf_buf;
	struct zwp_pointer_constraints_v1 *pointer_constraints;
	struct zxdg_decoration_manager_v1 *zxdg_decoration_manager_v1;
	struct wl_list output_list;

	uint32_t seat_capabilities;
	struct pixfmt {
		uint32_t wl;
		uint32_t p9;
	} pixfmt;
	struct bounds bounds;
} Globals;

// This is the singleton for a given process.
//
// Wayland specific code should be using the passed data pointer, this is just
// for the plan9 interface callbacks.
extern Globals procState;

// Window is a big bag of state for painting a window on screen.
typedef struct window {
	// RWLock mu;
	Globals *global;
	struct wl_keyboard *wl_keyboard;
	struct wl_pointer *wl_pointer;
	struct wl_surface *wl_surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;
	struct zwp_pointer_constraints_v1 *pointer_constraints;
	// Frame and border need to be re-organized: "frame" was originally the
	// window contents with the main surface being the border, but this
	// arrangement breaks pointer warping in GNOME. This state of affair is
	// weird, but actually works.
	struct border {
		struct edge {
			struct wl_subsurface *subsurface;
			struct wl_surface *surface;
			int skip;
			int x, y;
		} edge[4]; // top, bottom, left, right
		struct wl_shm_pool *pool;
	} decoration;
	struct bounds bounds, cursize, wantsize;
	int scale;
	int resizing;
	// Mouse members: x, y, button state, time and entry serial.
	struct mouse {
		// RWLock mu;
		int X, Y, B, T, S;
		int in; // which surface?
	} m;
	struct cursor {
		// RWLock mu;
		struct wl_surface *surface;
		struct wl_shm_pool *pool;
		uint32 *buf;
		int x, y;
	} cursor;
	// Keyboard members: xkb members and serial
	struct keyboard {
		// RWLock mu;
		struct xkb_context *context;
		struct xkb_keymap *keymap;
		struct xkb_state *state;
		int S;
		int32 rate, delay;
		int32 time, key, event, delayed;
	} kb;
} window;

struct Memdata;
typedef struct shmTab {
	struct Memdata *md;
	int fd;
	size_t sz;
} shmTab;

extern int borderSz;
extern int barSz;
extern const int curBufSz;
