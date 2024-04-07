#include <u.h>
#include <libc.h>
#include "wayland-inc.h"
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>
#include "devdraw.h"
#include <sys/mman.h>

AUTOLIB(xkbcommon);

static void keymap(void *opaque, struct wl_keyboard *kb, uint32_t format, int32_t fd, uint32_t size);
static void enter(void *opaque, struct wl_keyboard *kb, uint32_t serial, struct wl_surface *surface, struct wl_array *keys);
static void leave(void *opaque, struct wl_keyboard *kb, uint32_t serial, struct wl_surface *surface);
static void key(void *opaque, struct wl_keyboard *kb, uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
static void modifiers(void *opaque, struct wl_keyboard *kb, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group);
static void repeat_info(void *opaque, struct wl_keyboard *kb, int32_t rate, int32_t delay);

const struct wl_keyboard_listener wl_keyboard_listener = {
	.keymap = keymap,
	.enter = enter,
	.leave = leave,
	.key = key,
	.modifiers = modifiers,
	.repeat_info = repeat_info,
};

static Rune top9key(struct xkb_state *s, uint key);

static void repeat_cb(void *opaque, struct wl_callback *cb, uint time);

// This callback handles key repeat events by repeatedly scheduling itself
// while a key is held down.
static const struct wl_callback_listener repeat_listener = {
	.done = repeat_cb,
};

static void
keymap(void *opaque, struct wl_keyboard *kb, uint32_t format, int32_t fd, uint32_t size)
{
	Client *c;
	window *w;
	char *map_shm;
	struct xkb_keymap *keymap;
	struct xkb_state *state;

	c = opaque;
	w = (window *)(c->view);
	switch (format) {
	case WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP:
		fprint(2, "??? no keymap ??? \n");
		return;
	case WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1:
		break;
	default:
		sysfatal("unknown keymap format: %x", format);
	};
	map_shm = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map_shm == MAP_FAILED) {
		fprint(2, "unable to mmap keymap\n");
		return;
	}
	keymap = xkb_keymap_new_from_string(w->kb.context, map_shm, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	munmap(map_shm, size);
	close(fd);
	state = xkb_state_new(keymap);

	xkb_keymap_unref(w->kb.keymap);
	xkb_state_unref(w->kb.state);
	w->kb.keymap = keymap;
	w->kb.state = state;
}

static void
enter(void *opaque, struct wl_keyboard *kb, uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
	Client *c;
	window *w;
	uint32 *key;
	Rune r;

	c = opaque;
	w = (void *)(c->view);
	if (w->kb.S > serial)
		return;
	w->kb.S = serial;
	wl_array_for_each(key, keys)
	{
		r = top9key(w->kb.state, *key);
		if (r == -1)
			continue;
		gfx_keystroke(c, r);
	}
}

static void
leave(void *opaque, struct wl_keyboard *kb, uint32_t serial, struct wl_surface *surface)
{
	Client *c;
	window *w;

	c = opaque;
	w = (void *)(c->view);
	if (w->kb.S > serial)
		return;
	w->kb.S = serial;
	w->kb.event = WL_KEYBOARD_KEY_STATE_RELEASED;
}

static void
key(void *opaque, struct wl_keyboard *kb, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
	Client *c;
	window *w;
	Rune r;
	struct wl_callback *repeat;

	c = opaque;
	w = (void *)(c->view);
	if (w->kb.S > serial)
		return;
	w->kb.S = serial;

	r = top9key(w->kb.state, key);
	if (r == -1)
		return;

	w->kb.time = time;
	w->kb.event = state;
	if (xkb_state_mod_name_is_active(w->kb.state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) == 1)
		gfx_keystroke(c, Kalt);
	if (r != 0 && state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		w->kb.key = r;
		w->kb.delayed = 0;
		gfx_keystroke(c, r);
		repeat = wl_surface_frame(w->wl_surface);
		wl_callback_add_listener(repeat, &repeat_listener, c);
		wl_surface_commit(w->wl_surface);
	}
}

static void
modifiers(void *opaque, struct wl_keyboard *kb, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
	Client *c;
	window *w;

	c = opaque;
	w = (void *)(c->view);
	if (w->kb.S > serial)
		return;
	w->kb.S = serial;
	xkb_state_update_mask(w->kb.state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

static void
repeat_info(void *opaque, struct wl_keyboard *kb, int32_t rate, int32_t delay)
{
	Client *c;
	window *w;

	c = opaque;
	w = (void *)c->view;
	w->kb.rate = rate;
	w->kb.delay = delay;
}

static Rune
top9key(struct xkb_state *s, uint key)
{
	// Adapted from the X11 input handling.
	xkb_keysym_t sym;
	Rune r;

	sym = xkb_state_key_get_one_sym(s, (key + 8));
	switch (sym) {
	case XKB_KEY_BackSpace:
	case XKB_KEY_Tab:
	case XKB_KEY_Escape:
	case XKB_KEY_Delete:
	case XKB_KEY_KP_0:
	case XKB_KEY_KP_1:
	case XKB_KEY_KP_2:
	case XKB_KEY_KP_3:
	case XKB_KEY_KP_4:
	case XKB_KEY_KP_5:
	case XKB_KEY_KP_6:
	case XKB_KEY_KP_7:
	case XKB_KEY_KP_8:
	case XKB_KEY_KP_9:
	case XKB_KEY_KP_Divide:
	case XKB_KEY_KP_Multiply:
	case XKB_KEY_KP_Subtract:
	case XKB_KEY_KP_Add:
	case XKB_KEY_KP_Decimal:
		r = sym & 0x7F;
		break;
	case XKB_KEY_Linefeed:
		r = '\r';
		break;
	case XKB_KEY_KP_Space:
		r = ' ';
		break;
	case XKB_KEY_Home:
	case XKB_KEY_KP_Home:
		r = Khome;
		break;
	case XKB_KEY_Left:
	case XKB_KEY_KP_Left:
		r = Kleft;
		break;
	case XKB_KEY_Up:
	case XKB_KEY_KP_Up:
		r = Kup;
		break;
	case XKB_KEY_Down:
	case XKB_KEY_KP_Down:
		r = Kdown;
		break;
	case XKB_KEY_Right:
	case XKB_KEY_KP_Right:
		r = Kright;
		break;
	case XKB_KEY_Page_Down:
	case XKB_KEY_KP_Page_Down:
		r = Kpgdown;
		break;
	case XKB_KEY_End:
	case XKB_KEY_KP_End:
		r = Kend;
		break;
	case XKB_KEY_Page_Up:
	case XKB_KEY_KP_Page_Up:
		r = Kpgup;
		break;
	case XKB_KEY_Insert:
	case XKB_KEY_KP_Insert:
		r = Kins;
		break;
	case XKB_KEY_KP_Enter:
	case XKB_KEY_Return:
		r = '\n';
		break;
	case XKB_KEY_Alt_L:
	case XKB_KEY_Meta_L: /* Shift Alt on PCs */
	case XKB_KEY_Alt_R:
	case XKB_KEY_Meta_R: /* Shift Alt on PCs */
	case XKB_KEY_Multi_key:
		return -1;
	default:
		r = xkb_keysym_to_utf32(sym);
	}
	if (xkb_state_mod_name_is_active(s, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) == 1)
		r &= 0x9f;

	return r;
}

static void
repeat_cb(void *opaque, struct wl_callback *cb, uint time)
{
	Client *c;
	window *w;
	struct wl_callback *next;
	int32 delta;

	c = opaque;
	w = (void *)c->view;
	wl_callback_destroy(cb);

	if (w->kb.event != WL_KEYBOARD_KEY_STATE_PRESSED)
		return;

	if (!w->kb.delayed)
		delta = w->kb.delay;
	else
		delta = w->kb.rate;
	if (time > (w->kb.time + delta)) {
		w->kb.delayed = 1;
		w->kb.time = time;
		gfx_keystroke(c, w->kb.key);
	}
	next = wl_surface_frame(w->wl_surface);
	wl_callback_add_listener(next, &repeat_listener, c);
	wl_surface_commit(w->wl_surface);
}
