#define _POSIX_C_SOURCE 200809L
#include <sys/mman.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <linux/input-event-codes.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
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
wlallocbuffer(Wlwin *win)
{
	int stride, size;
	int fd;
	struct wl_shm_pool *pool;

	stride = win->dx * 4;
	size = stride * win->dy;

	fd = wlcreateshm(size);
	if(fd < 0)
		sysfatal("could not mk_shm_fd");

	win->shm_data = mmap(nil, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(win->shm_data == MAP_FAILED)
		sysfatal("could not mmap shm_data");

	pool = wl_shm_create_pool(win->shm, fd, size);
	win->buffer = wl_shm_pool_create_buffer(pool, 0, win->dx, win->dy, stride, WL_SHM_FORMAT_XRGB8888);
	/* TODO: optimize alloc a larger pool */
	wl_shm_pool_destroy(pool);
}

