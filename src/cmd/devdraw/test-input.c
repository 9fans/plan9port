/*
 * DEVDRAW_TEST_INPUT=file
 *
 * Read a sequence of one-line textual commands from file and execute them,
 * driving the keyboard, mouse, window size and retina mode as if a user were
 * operating the window. This runs as a separate proc so it can pace itself
 * with delays while the RPC service proc keeps handling draw requests.
 *
 * It works with both the in-memory backend (test-screen.c) and the host
 * backend, since it drives the client through the same backend-independent
 * entry points (gfx_keystroke, gfx_mousetrack, the ClientImpl callbacks).
 *
 * Commands (blank lines and lines beginning with # are ignored):
 *
 *	delay MS		sleep MS milliseconds (let the client catch up,
 *				or separate two clicks so they are not a double-click)
 *	waitframe N		block until the client has emitted at least N image
 *				frames (file.0..file.N-1), synchronizing the script
 *				with the output stream instead of racing it with a
 *				real-time delay
 *	waitidle MS		block until the client has emitted no new frame for MS
 *				milliseconds, i.e. the rendering from the preceding
 *				input has settled (no absolute frame count needed)
 *	move X Y		move the mouse to pixel (X,Y), buttons unchanged
 *	button MASK		set the mouse button mask (1,2,4 = left,middle,right)
 *	click X Y [MASK]	move to (X,Y), press MASK (default 1), release
 *	type TEXT		send each rune of TEXT as a keystroke (\n \t \b \r \\)
 *	key NAME		send one key: a single rune, an escape like \n, a
 *				named key (up down left right home end pgup pgdown
 *				ins del esc bs tab enter space), optionally prefixed
 *				with cmd- ctrl- alt-
 *	resize W H		resize the window to W x H pixels (simulated user resize)
 *	retina			toggle retina mode (the Cmd-R shortcut)
 *	label TEXT		set the window label
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

static int mx, my, mbut;	// current mouse position and button mask

static uint
msec(void)
{
	return nsec()/1000000;
}

// keyname converts a single key token to a rune, or -1 if unknown.
static int
keyname(char *s)
{
	Rune r;
	int n, i;
	static struct {
		char *name;
		int key;
	} keys[] = {
		{"backspace", Kbs},
		{"bs", Kbs},
		{"del", Kdel},
		{"delete", Kdel},
		{"down", Kdown},
		{"end", Kend},
		{"enter", '\n'},
		{"esc", Kesc},
		{"escape", Kesc},
		{"home", Khome},
		{"ins", Kins},
		{"left", Kleft},
		{"nl", '\n'},
		{"pgdn", Kpgdown},
		{"pgdown", Kpgdown},
		{"pgup", Kpgup},
		{"return", '\n'},
		{"right", Kright},
		{"space", ' '},
		{"tab", '\t'},
		{"up", Kup},
	};

	if(s[0] == 0)
		return -1;
	if(s[0] == '\\' && s[1] != 0){
		switch(s[1]){
		case 'n': return '\n';
		case 't': return '\t';
		case 'b': return '\b';
		case 'r': return '\r';
		case '\\': return '\\';
		}
	}
	n = chartorune(&r, s);
	if(s[n] == 0)		// a single rune
		return r;
	for(i = 0; i < nelem(keys); i++)
		if(cistrcmp(s, keys[i].name) == 0)
			return keys[i].key;
	fprint(2, "devdraw test input: unknown key %q\n", s);
	return -1;
}

// sendkey sends one key token, honoring cmd-/ctrl-/alt- prefixes.
static void
sendkey(Client *c, char *s)
{
	int k, cmd, ctl, alt;

	cmd = ctl = alt = 0;
	for(;;){
		if(cistrncmp(s, "cmd-", 4) == 0){ cmd = 1; s += 4; continue; }
		if(cistrncmp(s, "ctrl-", 5) == 0){ ctl = 1; s += 5; continue; }
		if(cistrncmp(s, "alt-", 4) == 0){ alt = 1; s += 4; continue; }
		break;
	}
	k = keyname(s);
	if(k < 0)
		return;
	if(ctl && k >= '@' && k < 0x7f)
		k &= 0x1f;		// control character
	if(alt)
		gfx_keystroke(c, Kalt);
	if(cmd)
		k += Kcmd;
	gfx_keystroke(c, k);
}

// sendtext sends each rune of s as a keystroke, applying \n \t \b \r \\ escapes.
static void
sendtext(Client *c, char *s)
{
	Rune r;
	int n;

	while(*s){
		if(*s == '\\' && s[1] != 0){
			s++;
			switch(*s){
			case 'n': gfx_keystroke(c, '\n'); s++; continue;
			case 't': gfx_keystroke(c, '\t'); s++; continue;
			case 'b': gfx_keystroke(c, '\b'); s++; continue;
			case 'r': gfx_keystroke(c, '\r'); s++; continue;
			case '\\': gfx_keystroke(c, '\\'); s++; continue;
			}
			s--;
		}
		n = chartorune(&r, s);
		gfx_keystroke(c, r);
		s += n;
	}
}

// waitframe blocks until the client has fully emitted at least n image frames
// (c->testoutdone >= n; testoutdone counts frames whose window AND cursor file
// are both on disk, unlike testoutn, which is bumped before either is written).
// It lets the script synchronize with the output stream so that a following
// command (e.g. a mouse move) lands at a deterministic frame boundary instead
// of racing the client's redraws -- or frame n-1's still-unwritten cursor. It
// gives up after a few seconds so a wedged client cannot hang the test
// indefinitely.
static void
waitframe(Client *c, int n)
{
	uint end;

	end = msec() + 5000;
	while(c->testoutdone < n){
		if((int)(msec() - end) >= 0){
			fprint(2, "devdraw test input: waitframe %d timed out (have %d)\n",
				n, c->testoutdone);
			return;
		}
		sleep(5);
	}
}

// recordsettle appends the current frame count to $DEVDRAW_TEST_SETTLE (if set)
// each time waitidle settles. The test harness uses these counts, captured
// during a -update run that paces with waitidle, to rewrite the script's
// waitframe N commands: each settle point becomes the absolute frame count a
// later (non-update) run must wait for, so the script synchronizes with the
// output stream deterministically instead of racing it.
static void
recordsettle(Client *c)
{
	static int fd = -2;
	char *p;
	char buf[16];

	if(fd == -2){
		fd = -1;
		if((p = getenv("DEVDRAW_TEST_SETTLE")) != nil && p[0] != 0)
			fd = create(p, OWRITE, 0666);
	}
	if(fd >= 0){
		snprint(buf, sizeof buf, "%d\n", c->testoutdone);
		write(fd, buf, strlen(buf));
	}
}

// waitidle blocks until the client has emitted no new image frame for ms
// milliseconds -- i.e. the rendering triggered by the preceding input has
// settled. Unlike waitframe it needs no absolute frame count, so it is used
// during -update to discover the settle points (see recordsettle), which then
// become the waitframe counts a normal run synchronizes with.
static void
waitidle(Client *c, int ms)
{
	int last;
	uint quiet, now, end;

	now = msec();
	last = c->testoutdone;
	quiet = now;
	end = now + 5000;
	for(;;){
		sleep(5);
		now = msec();
		if(c->testoutdone != last){
			last = c->testoutdone;
			quiet = now;
		}else if((int)(now - quiet) >= ms)
			return;
		if((int)(now - end) >= 0){
			fprint(2, "devdraw test input: waitidle timed out\n");
			return;
		}
	}
}

static void
docmd(Client *c, char *line)
{
	char *cmd, *p, *args[8];
	int n, b;

	while(*line == ' ' || *line == '\t')
		line++;
	if(*line == 0 || *line == '#')
		return;

	// Split off the command word; keep the remainder intact (it may have
	// embedded spaces, e.g. "type hello world").
	cmd = line;
	p = line;
	while(*p && *p != ' ' && *p != '\t')
		p++;
	if(*p)
		*p++ = 0;
	while(*p == ' ' || *p == '\t')
		p++;

	if(strcmp(cmd, "delay") == 0){
		sleep(atoi(p));
	}else if(strcmp(cmd, "waitframe") == 0){
		waitframe(c, atoi(p));
	}else if(strcmp(cmd, "waitidle") == 0){
		waitidle(c, atoi(p));
		recordsettle(c); // record this settle point for the waitframes golden
	}else if(strcmp(cmd, "move") == 0){
		n = tokenize(p, args, nelem(args));
		if(n >= 2){
			mx = atoi(args[0]);
			my = atoi(args[1]);
			gfx_mousetrack(c, mx, my, mbut, msec());
		}
	}else if(strcmp(cmd, "button") == 0 || strcmp(cmd, "buttons") == 0){
		mbut = (int)strtol(p, nil, 0);
		gfx_mousetrack(c, mx, my, mbut, msec());
	}else if(strcmp(cmd, "click") == 0){
		n = tokenize(p, args, nelem(args));
		if(n >= 2){
			mx = atoi(args[0]);
			my = atoi(args[1]);
			b = n >= 3 ? (int)strtol(args[2], nil, 0) : 1;
			gfx_mousetrack(c, mx, my, 0, msec());
			gfx_mousetrack(c, mx, my, b, msec());
			gfx_mousetrack(c, mx, my, 0, msec());
			mbut = 0;
		}
	}else if(strcmp(cmd, "type") == 0){
		sendtext(c, p);
	}else if(strcmp(cmd, "key") == 0){
		sendkey(c, p);
	}else if(strcmp(cmd, "resize") == 0){
		n = tokenize(p, args, nelem(args));
		if(n >= 2){
			c->impl->rpc_resizewindow(c, Rect(0, 0, atoi(args[0]), atoi(args[1])));
			// rpc_resizewindow swaps the screen quietly now; a scripted user
			// resize must still notify the client (samterm relies on this).
			gfx_mouseresized(c);
		}
	}else if(strcmp(cmd, "retina") == 0){
		gfx_keystroke(c, Kcmd+'r');
	}else if(strcmp(cmd, "label") == 0){
		c->impl->rpc_setlabel(c, p);
	}else{
		fprint(2, "devdraw test input: unknown command %q\n", cmd);
	}
}

// slurp reads the entire file into a NUL-terminated malloc'ed buffer.
static char*
slurp(char *file)
{
	char *buf, tmp[8192];
	int fd, n, cap, tot;

	fd = open(file, OREAD);
	if(fd < 0){
		fprint(2, "devdraw test input: open %s: %r\n", file);
		return nil;
	}
	buf = nil;
	cap = 0;
	tot = 0;
	while((n = read(fd, tmp, sizeof tmp)) > 0){
		if(tot+n+1 > cap){
			cap = (tot+n+1)*2;
			buf = realloc(buf, cap);
			if(buf == nil)
				sysfatal("devdraw test input: out of memory");
		}
		memmove(buf+tot, tmp, n);
		tot += n;
	}
	close(fd);
	if(buf == nil)
		buf = malloc(1);
	buf[tot] = 0;
	return buf;
}

static void
inputproc(void *v)
{
	Client *c;
	char *buf, *p, *nl, *donefile;

	c = v;

	// Wait for the client to attach its window before driving it.
	while(c->screenimage == nil)
		sleep(10);

	buf = slurp(testinfile);
	if(buf == nil)
		return;

	for(p = buf; *p != 0; p = nl){
		nl = strchr(p, '\n');
		if(nl != nil)
			*nl++ = 0;
		else
			nl = p + strlen(p);
		docmd(c, p);
	}
	free(buf);

	// Signal the harness that the whole script has been delivered and the
	// client has settled. This lets the harness stop waiting on an explicit
	// end-of-script marker instead of guessing from a frame-quiet timeout,
	// which a long frameless stretch mid-script -- e.g. a window sweep reading
	// several mouse events that draw nothing -- would otherwise trip
	// prematurely, truncating the captured frames at a script-dependent point.
	donefile = getenv("DEVDRAW_TEST_DONE");
	if(donefile != nil && donefile[0] != 0){
		int fd;

		waitidle(c, 200);
		if((fd = create(donefile, OWRITE, 0666)) >= 0)
			close(fd);
	}
}

void
test_input_start(Client *c)
{
	proccreate(inputproc, c, 65536);
}
