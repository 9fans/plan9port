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
#include "wl-inc.h"

static void
randname(char *buf)
{
	struct timespec ts;
	int i;

	clock_gettime(CLOCK_REALTIME, &ts);
	long r = ts.tv_nsec;
	for(i=0; i < 6; i++) {
		buf[i] = 'A'+(r&15)+(r+16)*2;
		r >>= 5;
	}
}

static int
wlcreateshm(off_t size)
{
	char name[] = "/devdraw--XXXXXX";
	int retries = 100;
	int fd;

	do {
		randname(name + strlen(name) - 6);
		--retries;
		fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if(fd >= 0){
			shm_unlink(name);
			if(ftruncate(fd, size) < 0){
				close(fd);
				return -1;
			}
			return fd;
		}
	} while (retries > 0 && errno == EEXIST);
	return -1;
}

void
wlallocpool(Wlwin *wl)
{
	int screenx, screeny;
	int screensize, cursorsize;
	int depth;
	int fd;

	if(wl->pool != nil)
		wl_shm_pool_destroy(wl->pool);

	depth = 4;
	screenx = wl->dx > wl->monx ? wl->dx : wl->monx;
	screeny = wl->dy > wl->mony ? wl->dy : wl->mony;
	screensize = screenx * screeny * depth;
	cursorsize = 16 * 16 * depth;

	fd = wlcreateshm(screensize+cursorsize);
	if(fd < 0)
		sysfatal("could not mk_shm_fd");

	wl->shm_data = mmap(nil, screensize+cursorsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(wl->shm_data == MAP_FAILED)
		sysfatal("could not mmap shm_data");

	wl->pool = wl_shm_create_pool(wl->shm, fd, screensize+cursorsize);
	wl->poolsize = screensize+cursorsize;
	close(fd);
}

void
wlallocbuffer(Wlwin *wl)
{
	int depth;
	int size;

	depth = 4;
	size = wl->dx * wl->dy * depth;
	if(wl->pool == nil || size+(16*16*depth) > wl->poolsize)
		wlallocpool(wl);

	if(wl->screenbuffer != nil)
		wl_buffer_destroy(wl->screenbuffer);
	if(wl->cursorbuffer != nil)
		wl_buffer_destroy(wl->cursorbuffer);

	wl->screenbuffer = wl_shm_pool_create_buffer(wl->pool, 0, wl->dx, wl->dy, wl->dx*4, WL_SHM_FORMAT_XRGB8888);
	wl->cursorbuffer = wl_shm_pool_create_buffer(wl->pool, size, 16, 16, 16*4, WL_SHM_FORMAT_ARGB8888);
}

static enum {
	White = 0xFFFFFFFF,
	Black = 0xFF000000,
	Green = 0xFF00FF00,
	Transparent = 0x00000000,
};

void
wldrawcursor(Wlwin *wl, Cursor *c)
{
	int i, j;
	int pos, mask;
	u32int *buf;
	u16int clr[16], set[16];

	buf = wl->shm_data+(wl->dx*wl->dy*4);
	for(i=0,j=0; i < 16; i++,j+=2){
		clr[i] = c->clr[j]<<8 | c->clr[j+1];
		set[i] = c->set[j]<<8 | c->set[j+1];
	}
	for(i=0; i < 16; i++){
		for(j = 0; j < 16; j++){
			pos = i*16 + j;
			mask = (1<<16) >> j;

			buf[pos] = Transparent;
			if(clr[i] & mask)
				buf[pos] = White;
			if(set[i] & mask)
				buf[pos] = Black;
		}
	}
	if(wl->cursorsurface != nil)
		wl_surface_destroy(wl->cursorsurface);
	wl->cursorsurface = wl_compositor_create_surface(wl->compositor);
	wl_surface_attach(wl->cursorsurface, wl->cursorbuffer, 0, 0);
	wl_surface_commit(wl->cursorsurface);
	wl_pointer_set_cursor(wl->pointer, wl->pointerserial, wl->cursorsurface, 0, 0);
}
