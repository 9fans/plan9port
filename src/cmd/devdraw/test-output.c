/*
 * DEVDRAW_TEST_OUTPUT=file
 *
 * Write each drawing operation's changed rectangle to a numbered Plan 9 image
 * file: file.0, file.1, file.2, ... -- one per drawing operation (see testframe
 * in devdraw.c). Each file is just the subimage the operation touched; the test
 * harness reconstructs full frames by compositing them. The mouse cursor is not
 * composited into these window frames (it would break their flush-rectangle
 * bookkeeping); with DEVDRAW_TEST_CURSOR it is written as a parallel sequence of
 * transparent frames, file.cursor.N, that the harness overlays (see writecursor).
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

// The default cursor (when the client has not set one) is the standard arrow.
#define bigarrow t_bigarrow
#define bigarrow2 t_bigarrow2
#include "bigarrow.h"
#undef bigarrow
#undef bigarrow2

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

// writeframefile writes m to the numbered file "<prefix><suffix>.<n>".
static void
writeframefile(char *prefix, char *suffix, int n, Memimage *m)
{
	char *file;
	int fd;

	file = smprint("%s%s.%d", prefix, suffix, n);
	if(file == nil)
		return;
	fd = create(file, OWRITE, 0666);
	if(fd < 0)
		fprint(2, "test_output: create %s: %r\n", file);
	else{
		if(writeuncompressed(fd, m) < 0)
			fprint(2, "test_output: write %s: %r\n", file);
		close(fd);
	}
	free(file);
}

// writecursor writes the mouse cursor as a separate RGBA frame at its real
// screen position (testoutfile.cursor.N, in lockstep with the window frame N).
// Compositing the cursor into the per-operation window frames would break their
// flush-rectangle bookkeeping, so the cursor is a frame of its own that the GIF
// overlays with gif.DisposalPrevious. The sprite is the 16x16 Plan 9 cursor:
// set bits black, clr-only bits white, the rest transparent (alpha 0). The
// default (no client cursor) is the standard arrow t_bigarrow.
// test_setmouselog appends the current frame index to <testoutfile>.setmouse,
// one line per setmouse. -update reads C's and the Go port's logs and sets each
// scenario's per-setmouse deferral to C_frame[k]-Go_frame[k].
// test_setcursor does the same for cursor-sprite changes (<testoutfile>.setcursor).

// logframe appends the current frame index to <testoutfile><suffix>,
// opening the file on first use (*fd caches the descriptor).
static void
logframe(Client *c, char *suffix, int *fd)
{
	char *f;

	if(testoutfile == nil)
		return;
	if(*fd < 0){
		f = smprint("%s%s", testoutfile, suffix);
		*fd = create(f, OWRITE, 0666);
		free(f);
		if(*fd < 0)
			return;
	}
	fprint(*fd, "%d\n", c->testoutn);
}

void
test_setmouselog(Client *c)
{
	static int fd = -1;

	logframe(c, ".setmouse", &fd);
}

// test_setcursor records the client's cursor sprite (cur == nil means the
// default arrow) for output compositing. Like t_rpc_setmouse, the change is
// routed through a pend applied by writecursor whenever device input is
// scripted or a calibrated delay is in force, so which cursor frame first
// shows the new sprite is deterministic against the in-flight draw burst;
// otherwise (acme -D) it applies immediately, as it always did.
void
test_setcursor(Client *c, Cursor *cur)
{
	static int fd = -1;
	int delay;
	Cursor zero;

	if(testoutfile == nil)
		return;
	logframe(c, ".setcursor", &fd);
	delay = c->testsci < c->testscn ? c->testscdelay[c->testsci] : 0;
	c->testsci++;
	if(!test_hasscriptinput() && delay <= 0){
		if(cur == nil)
			c->testcursorset = 0;
		else{
			c->testcursorset = 1;
			c->testcursor = *cur;
		}
		return;
	}
	memset(&zero, 0, sizeof zero);
	c->testcurpendset = cur != nil;
	c->testcurpendcur = cur != nil ? *cur : zero;
	c->testcurpendn = delay;
	c->testcurpend = 1;
}

static void
writecursor(Client *c, int n)
{
	// Apply a deferred setmouse once its countdown elapses, before this frame's
	// cursor is rendered (DEVDRAW_TEST_SETMOUSE_DELAYS).
	if(c->testpend){
		if(c->testpendn <= 0){
			c->testmousexy = c->testpendxy;
			c->testpend = 0;
		}else
			c->testpendn--;
	}
	// Likewise a deferred setcursor (DEVDRAW_TEST_SETCURSOR_DELAYS).
	if(c->testcurpend){
		if(c->testcurpendn <= 0){
			c->testcursorset = c->testcurpendset;
			c->testcursor = c->testcurpendcur;
			c->testcurpend = 0;
		}else
			c->testcurpendn--;
	}
	Memimage *cm;
	Cursor *cur;
	Rectangle r, sr;
	Point mp;
	int x, y, set, clr, mask;
	uchar *p;

	qlock(&drawlk);
	if(c->screenimage == nil){
		qunlock(&drawlk);
		return;
	}
	mp = c->testmousexy;
	cur = c->testcursorset ? &c->testcursor : &t_bigarrow;
	r = Rect(mp.x+cur->offset.x, mp.y+cur->offset.y,
		mp.x+cur->offset.x+16, mp.y+cur->offset.y+16);
	sr = r;
	if(!rectclip(&sr, c->screenimage->r) || Dx(sr) <= 0 || Dy(sr) <= 0){
		qunlock(&drawlk); // cursor entirely off-screen: no frame
		return;
	}
	cm = _allocmemimage(sr, RGBA32);
	if(cm == nil){
		qunlock(&drawlk);
		return;
	}
	memset(byteaddr(cm, sr.min), 0, bytesperline(sr, cm->depth)*Dy(sr)); // transparent
	for(y = 0; y < 16; y++)
	for(x = 0; x < 16; x++){
		mask = 1 << (7 - (x&7));
		set = cur->set[y*2 + (x>>3)] & mask;
		clr = cur->clr[y*2 + (x>>3)] & mask;
		if(!set && !clr)
			continue;
		if(!ptinrect(Pt(r.min.x+x, r.min.y+y), sr))
			continue;
		// RGBA32 is little-endian r8g8b8a8: bytes are [a,b,g,r].
		p = byteaddr(cm, Pt(r.min.x+x, r.min.y+y));
		if(set){
			p[0] = 0xFF; p[1] = 0x00; p[2] = 0x00; p[3] = 0x00; // black, opaque
		}else{
			p[0] = 0xFF; p[1] = 0xFF; p[2] = 0xFF; p[3] = 0xFF; // white, opaque
		}
	}
	qunlock(&drawlk);
	writeframefile(testoutfile, ".cursor", n, cm);
	_freememimage(cm);
}

void
test_output(Client *c, Rectangle r)
{
	Memimage *m, *frame;
	int y, bpl, n;

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
	n = c->testoutn++;
	if(drawlogfd() >= 0)
		fprint(drawlogfd(), "# frame %d\n", n);

	writeframefile(testoutfile, "", n, frame);
	_freememimage(frame);

	// Write the cursor as a separate frame N (testoutfile.cursor.N), so every
	// window frame N has a matching cursor frame N. The harness interleaves the
	// two streams into window,cursor,window,cursor,... pairs.
	writecursor(c, n);

	// Only now does frame n fully exist. The input script's waitframe polls
	// testoutdone, not testoutn: testoutn is incremented above before either
	// file is written, and injecting input in that window (this thread can be
	// descheduled arbitrarily long under load) would let a scripted move slip
	// into frame n's cursor instead of frame n+1's.
	c->testoutdone = n + 1;
}
