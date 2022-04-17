// Put forward declarations for a lot of the pageantry here, so the actual
// source reads better.

// Global registry handler
static void
registry_global(void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version);
static void
registry_global_remove(void *data, struct wl_registry *wl_registry, uint32_t name);
static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

// XDG protocol handler
static void
xdg_wm_base_ping(void *data, struct xdg_wm_base *base, uint32_t serial);
static const struct xdg_wm_base_listener xdg_wm_base_listener = {
	.ping = xdg_wm_base_ping,
};

// surface listener
static void
xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial);
static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_configure,
};

// toplevel listener
static void
xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t wd, int32_t ht, struct wl_array *states);
static void
xdg_toplevel_configure_bounds(void *data, struct xdg_toplevel *xdg_toplevel, int32_t wd, int32_t ht);
static void
xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel);
static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_toplevel_configure,
	.configure_bounds = xdg_toplevel_configure_bounds,
	.close = xdg_toplevel_close,
};

// buffer listener
static void
wl_buffer_release(void *data, struct wl_buffer *wl_buffer);
static const struct wl_buffer_listener wl_buffer_listener = {
	.release = wl_buffer_release,
};

// seat listener
static void
wl_seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t caps);
static void
wl_seat_name(void *data, struct wl_seat *wl_seat, const char *name);
static const struct wl_seat_listener wl_seat_listener = {
	.capabilities = wl_seat_capabilities,
	.name = wl_seat_name,
};

// output handler
extern struct wl_output_listener output_listener;

// See wayland-pointer.c
extern const struct wl_pointer_listener wl_pointer_listener;
extern const struct zwp_pointer_constraints_listener pointer_constraints;
