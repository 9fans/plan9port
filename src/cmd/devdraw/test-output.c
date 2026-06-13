/*
 * DEVDRAW_TEST_OUTPUT=file
 *
 * Write each drawing operation's changed rectangle to a numbered Plan 9 image
 * file: file.0, file.1, file.2, ... -- one per drawing operation (see testframe
 * in devdraw.c). Each file is just the subimage the operation touched; the test
 * harness reconstructs full frames by compositing them. The mouse cursor is
 * intentionally not composited; see test_output.
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

// writeuncompressed writes m in the uncompressed image(6) format: the same
// header writememimage emits but without the leading "compressed\n", followed by
// the raw pixel bytes. Test frames are large and short-lived, so skipping
// compression here (and the matching decompression in the reader) trades disk
// space for speed.
static int
writeuncompressed(int fd, Memimage *m)
{
	char hdr[5*12+1], cbuf[30];
	uchar *data;
	int bpl, n;

	bpl = bytesperline(m->r, m->depth);
	n = Dy(m->r) * bpl;
	if((data = malloc(n)) == nil)
		return -1;
	if(unloadmemimage(m, m->r, data, n) != n){
		free(data);
		return -1;
	}
	sprint(hdr, "%11s %11d %11d %11d %11d ",
		chantostr(cbuf, m->chan), m->r.min.x, m->r.min.y, m->r.max.x, m->r.max.y);
	if(write(fd, hdr, 5*12) != 5*12 || write(fd, data, n) != n){
		free(data);
		return -1;
	}
	free(data);
	return 0;
}

void
test_output(Client *c, Rectangle r)
{
	Memimage *m, *frame;
	char *file;
	int fd, y, bpl;

	if(testoutfile == nil)
		return;

	qlock(&drawlk);
	m = c->screenimage;
	if(m == nil){
		qunlock(&drawlk);
		return;
	}

	// Write only the rectangle r that the drawing operation changed; the harness
	// reconstructs each full frame by compositing the rectangles onto a running
	// canvas (the rest of the frame is identical to the previous one). If r does
	// not meet the screen in a non-empty rectangle -- a draw clipped off-screen,
	// or a degenerate zero-area draw -- nothing changed, so write no frame.
	if(!rectclip(&r, m->r) || Dx(r) <= 0 || Dy(r) <= 0){
		qunlock(&drawlk);
		return;
	}

	// Snapshot r so it can be written without holding drawlk across the file
	// write (and without a later draw disturbing it).
	//
	// We deliberately do NOT composite the mouse cursor here. The cursor is a
	// devdraw overlay drawn at c->mouse.last.xy, which the mouse thread updates
	// asynchronously from the draw thread that produces these per-operation
	// frames; compositing it would stamp each frame with a nondeterministic
	// cursor position (whichever mouse event had arrived by then) and make the
	// frame sequence differ run to run. The frames capture exactly what the
	// client drew, which is what the Go and C terminals are compared on.
	frame = _allocmemimage(r, m->chan);
	if(frame == nil){
		qunlock(&drawlk);
		fprint(2, "test_output: allocmemimage: %r\n");
		return;
	}
	bpl = bytesperline(r, m->depth);
	for(y = r.min.y; y < r.max.y; y++)
		memmove(byteaddr(frame, Pt(r.min.x, y)), byteaddr(m, Pt(r.min.x, y)), bpl);
	qunlock(&drawlk);

	// Mark the frame boundary in the opcode trace, so the log and the image
	// files (testoutfile.N) stay in lockstep: the opcodes since the previous
	// marker are the ones that produced this frame.
	if(drawlogfd() >= 0)
		fprint(drawlogfd(), "# frame %d\n", c->testoutn);

	file = smprint("%s.%d", testoutfile, c->testoutn++);
	if(file == nil){
		_freememimage(frame);
		return;
	}
	fd = create(file, OWRITE, 0666);
	if(fd < 0)
		fprint(2, "test_output: create %s: %r\n", file);
	else{
		if(writeuncompressed(fd, frame) < 0)
			fprint(2, "test_output: write %s: %r\n", file);
		close(fd);
	}
	free(file);
	_freememimage(frame);
}
