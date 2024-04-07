#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <mouse.h>
#include <cursor.h>
#include "devdraw.h"
#include "wayland-inc.h"

#include <math.h>

static void wl_output_geometry(void *opaque, struct wl_output *wl_output, int32_t x, int32_t y, int32_t physical_width, int32_t physical_height, int32_t subpixel, const char *make, const char *model, int32_t transform);
static void wl_output_mode(void *opaque, struct wl_output *wl_output, uint32_t flags, int32_t width, int32_t height, int32_t refresh);
static void wl_output_scale(void *opaque, struct wl_output *wl_output, int32_t factor);
static void wl_output_name(void *opaque, struct wl_output *wl_output, const char *name);
static void wl_output_done(void *opaque, struct wl_output *wl_output);

const struct wl_output_listener output_listener = {
	.geometry = wl_output_geometry,
	.mode = wl_output_mode,
	.scale = wl_output_scale,
	.name = wl_output_name,
	.done = wl_output_done,
};

static const double mmtoinch = 25.4;

static void
wl_output_geometry(void *opaque, struct wl_output *wl_output, int32_t x, int32_t y, int32_t physical_width, int32_t physical_height, int32_t subpixel, const char *make, const char *model, int32_t transform)
{
	struct output_elem *e;

	e = opaque;
	e->mm = sqrt(pow(physical_width, 2) + pow(physical_height, 2));
}

static void
wl_output_mode(void *opaque, struct wl_output *wl_output, uint32_t flags, int32_t width, int32_t height, int32_t refresh)
{
	struct output_elem *e;

	e = opaque;
	e->px = sqrt(pow(width, 2) + pow(height, 2));
}

static void
wl_output_scale(void *opaque, struct wl_output *wl_output, int32_t factor)
{
	struct output_elem *e;

	e = opaque;
	e->scale = factor;
}

static void
wl_output_name(void *opaque, struct wl_output *wl_output, const char *name)
{
}

static void
wl_output_done(void *opaque, struct wl_output *wl_output)
{
	struct output_elem *e;

	e = opaque;
	e->dpi = (int)(e->px / (e->mm / mmtoinch));
	//fprint(2, "output: %fpx/%fmm(%d)@%d\n", e->px, e->mm, e->dpi, e->scale);
}
