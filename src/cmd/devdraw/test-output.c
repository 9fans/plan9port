/*
 * DEVDRAW_TEST_OUTPUT=file
 *
 * On each flush, write the full window image (with the mouse cursor
 * composited on top, according to its current shape and position) to a
 * numbered Plan 9 image file: file.0, file.1, file.2, ...
 *
 * This works with both the in-memory test backend (test-screen.c) and the
 * host window-system backend, which calls test_output from its flush path.
 * The image is written with writememimage and can be viewed with topng(1).
 */

#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <mouse.h>
#include <cursor.h>
#include <keyboard.h>
#include <drawfcall.h>
#include "devdraw.h"

// Our own copy of the default arrow cursor, used when the client has not set
// one. bigarrow.h *defines* bigarrow/bigarrow2; rename them so they do not
// collide with the host backend's copy (e.g. mac-screen.m), and so this file
// is self-contained on backends that have no arrow of their own (x11).
#define bigarrow t_bigarrow
#define bigarrow2 t_bigarrow2
#include "bigarrow.h"
#undef bigarrow
#undef bigarrow2

// drawcursor composites a 16x16 cursor onto the XRGB32 image m, with the
// cursor's hotspot at mp. A pixel is black where the cursor's set bit is on,
// white where only the clr bit is on, and left untouched (transparent)
// otherwise -- matching the standard Plan 9 cursor semantics.
static void
drawcursor(Memimage *m, Cursor *cur, Point mp)
{
	int x, y, set, clr, mask;
	uchar v, *p;
	Point q;

	for(y = 0; y < 16; y++)
	for(x = 0; x < 16; x++){
		mask = 1 << (7 - (x & 7));
		set = cur->set[y*2 + (x>>3)] & mask;
		clr = cur->clr[y*2 + (x>>3)] & mask;
		if(!set && !clr)
			continue;
		q.x = mp.x + cur->offset.x + x;
		q.y = mp.y + cur->offset.y + y;
		if(q.x < m->r.min.x || q.x >= m->r.max.x
		|| q.y < m->r.min.y || q.y >= m->r.max.y)
			continue;
		v = set ? 0x00 : 0xFF;
		p = byteaddr(m, q);
		p[0] = v;
		p[1] = v;
		p[2] = v;
		p[3] = 0xFF;
	}
}

void
test_output(Client *c)
{
	Memimage *m, *frame;
	Cursor *cur;
	char *file;
	long nbyte;
	int fd;

	if(testoutfile == nil)
		return;

	qlock(&drawlk);
	m = c->screenimage;
	if(m == nil){
		qunlock(&drawlk);
		return;
	}

	// Copy the window image so the cursor can be composited without
	// disturbing the live screen image. Both images are XRGB32 with the
	// same rectangle, so the backing store is a straight copy.
	frame = _allocmemimage(m->r, m->chan);
	if(frame == nil){
		qunlock(&drawlk);
		fprint(2, "test_output: allocmemimage: %r\n");
		return;
	}
	nbyte = (long)m->width * sizeof(u32int) * Dy(m->r);
	memmove(byteaddr(frame, m->r.min), byteaddr(m, m->r.min), nbyte);

	cur = c->testcursorset ? &c->testcursor : &t_bigarrow;
	drawcursor(frame, cur, c->mouse.last.xy);
	qunlock(&drawlk);

	file = smprint("%s.%d", testoutfile, c->testoutn++);
	if(file == nil){
		_freememimage(frame);
		return;
	}
	fd = create(file, OWRITE, 0666);
	if(fd < 0)
		fprint(2, "test_output: create %s: %r\n", file);
	else{
		if(writememimage(fd, frame) < 0)
			fprint(2, "test_output: writememimage %s: %r\n", file);
		close(fd);
	}
	free(file);
	_freememimage(frame);
}
