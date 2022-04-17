#include <u.h>
#include "wayland-inc.h"
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

#include "wayland-shm.h"
#include <sys/mman.h>

AUTOLIB(wayland);

Memimage *
allocmemimage(Rectangle r, u32int chan) {
	int d;
	Memimage *i;
	if ((d = chantodepth(chan)) == 0) {
		werrstr("bad channel descriptor %.8lux", chan);
		return nil;
	}
	//	fprint(2, "allocmemimage: start\n");
	// Too lazy to write a real allocator, just create shm mappings and rely on
	// wayland's book-keeping to tear them down.
	int l = wordsperline(r, d);
	int nw = l * Dy(r);
	uint32 sz = sizeof(shmTab) + ((1 + nw) * sizeof(ulong));
	Memdata *md = malloc(sizeof(Memdata));
	if (md == nil) {
		//		fprint(2, "allocmemimage: malloc fail\n");
		return nil;
	}
	//	fprint(2, "allocmemimage: shm\n");
	int fd = allocate_shm_file(sz);
	if (fd == -1) {
		//		fprint(2, "allocmemimage: shm fail\n");
		free(md);
		return nil;
	}
	//	fprint(2, "allocmemimage: mmap\n");
	uchar *data = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		//		fprint(2, "allocmemimage: map fail\n");
		free(md);
		close(fd);
		return nil;
	}

	md->ref = 1;
	md->base = (u32int *)data;
	shmTab *t = (shmTab *)data;
	t->md = md;
	t->fd = fd;
	t->sz = sz;
	// Should only be 24 bytes used, max.
	md->bdata = (uchar *)(md->base + sizeof(shmTab));
	md->allocd = 1;
	//	fprint(2, "allocmemimage: allocmemimaged\n");
	i = allocmemimaged(r, chan, md, t);
	if (i == nil) {
		//		fprint(2, "allocmemimage: allocmemimaged fail\n");
		munmap(data, sz);
		free(md);
		close(fd);
		return nil;
	}
	md->imref = i;
	return i;
}

/*
static void
buffer_release(void *opaque, struct wl_buffer *buf) {
    wl_buffer_destroy(buf);
}

static struct wl_buffer_listener kill_buffer = {
    .release = buffer_release,
};

void
mk_buffer(window *w, Memimage *i) {
    if (i == nil)
        return;
    if (i->X != nil)
        return;
    if (i->data->ref == 0 || !i->data->allocd) // zombie memimage ???
        return;
    shmTab *tab = (shmTab *)i->data->base;
    Globals *g = w->global;
    struct wl_shm_pool *pool = wl_shm_create_pool(g->wl_shm, tab->fd, tab->sz);
    if (pool == nil) {
        fprint(2, "mk_buffer: pool fail\n");
        return;
    }
    struct wl_buffer *buf = wl_shm_pool_create_buffer(
        pool, 64, Dx(i->r), Dy(i->r), Dx(i->r) * 4, WL_SHM_FORMAT_XRGB8888);
    if (buf == nil) {
        fprint(2, "mk_buffer: buffer fail\n");
        return;
    }
    wl_shm_pool_destroy(pool);
    fprint(2, "mkbuffer: %p\n", i);
    i->X = buf;
    wl_buffer_add_listener(buf, &kill_buffer, NULL);
}
*/

void
freememimage(Memimage *i) {
	if (i == nil)
		return;
	if (i->data->ref-- == 1 && i->data->allocd) {
		if (i->data->base) {
			shmTab *tab = (shmTab *)i->data->base;
			if (tab->md != i->data) {
				fprint(2, "maritan memdata\n");
				return _freememimage(i);
			};
			close(tab->fd);
			munmap(i->data->base, tab->sz);
		}
		free(i->data);
	}
	free(i);
}

/*
void
memimagedraw(Memimage *dst, Rectangle r, Memimage *src, Point sp,
             Memimage *mask, Point mp, int op) {
    fprint(2, "memimagedraw\n");
}

void
memfillcolor(Memimage *m, u32int val) {
    _memfillcolor(m, val);
}

u32int
pixelbits(Memimage *m, Point p) {
    return _pixelbits(m, p);
}
*/

int
loadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata) {
	return _loadmemimage(i, r, data, ndata);
}

int
cloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata) {
	return _cloadmemimage(i, r, data, ndata);
}

int
unloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata) {
	return _unloadmemimage(i, r, data, ndata);
}
