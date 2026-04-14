/*
 * SDL3 backend for devdraw.
 *
 * Replaces the X11 backend (x11-screen.c and friends) with SDL3,
 * providing native Wayland support alongside X11, plus portability
 * to any platform SDL3 supports.
 *
 * Follows the macOS backend pattern: all drawing happens in software
 * via memdraw; the screen image is uploaded to an SDL texture at flush time.
 * No server-side pixmaps or GPU-accelerated drawing.
 */

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>
#include <thread.h>
#include "devdraw.h"

/*
 * SDL3 headers conflict with Plan 9 nil macro.
 * Temporarily undefine it around the include.
 */
#undef nil
#include <SDL3/SDL.h>
#define nil ((void*)0)

typedef struct Sdlwin Sdlwin;

struct Sdlwin
{
	SDL_Window	*win;
	SDL_Renderer	*renderer;
	SDL_Texture	*texture;
	Client		*client;

	Memimage	*screenimage;
	Rectangle	screenr;
	Rectangle	windowrect;
	Rectangle	screenrect;	/* full display bounds */
	int		fullscreen;

	Sdlwin		*next;
};

static Sdlwin *windows;
static QLock sdllk;
static int kbuttons;
static int kstate;	/* modifier state: bit 0=shift, 1=ctrl, 2=alt */
static int altdown;
static Uint32 flushevent;	/* custom SDL event type for cross-thread flush */
static Uint32 resizeevent;	/* custom SDL event for cross-thread resize */
static Uint32 attachevent;	/* custom SDL event for cross-thread attach */
static QLock flushlk;
static Rectangle flushr;	/* pending flush rectangle */
static Client *flushclient;	/* client for pending flush */
static Client *resizeclient;	/* client for pending resize */
static QLock resizelk;
static Rendez resizewait;	/* signaled when resize completes */
static int resizedone;
static Client *attachclient;	/* client for pending attach */
static char *attachlabel;
static char *attachwinsize;
static Memimage *attachresult;
static QLock attachlk;
static Rendez attachwait;	/* signaled when attach completes */
static int attachdone;

static void	sdlloop(void);
static void	_sdlflush(Sdlwin*, Rectangle);
static void	rpc_resizeimg(Client*);
static void	rpc_resizewindow(Client*, Rectangle);
static void	rpc_setcursor(Client*, Cursor*, Cursor2*);
static void	rpc_setlabel(Client*, char*);
static void	rpc_setmouse(Client*, Point);
static void	rpc_topwin(Client*);
static void	rpc_bouncemouse(Client*, Mouse);
static void	rpc_flush(Client*, Rectangle);

static ClientImpl sdl3impl = {
	rpc_resizeimg,
	rpc_resizewindow,
	rpc_setcursor,
	rpc_setlabel,
	rpc_setmouse,
	rpc_topwin,
	rpc_bouncemouse,
	rpc_flush
};

static void
sdllock(void)
{
	qlock(&sdllk);
}

static void
sdlunlock(void)
{
	qunlock(&sdllk);
}

static Sdlwin*
newsdlwin(Client *c)
{
	Sdlwin *w;

	w = mallocz(sizeof *w, 1);
	if(w == nil)
		sysfatal("out of memory");
	w->client = c;
	w->next = windows;
	windows = w;
	c->impl = &sdl3impl;
	c->view = w;
	return w;
}

static Sdlwin*
findsdlwin(SDL_WindowID id)
{
	Sdlwin *w, **l;

	for(l = &windows; (w = *l) != nil; l = &w->next) {
		if(SDL_GetWindowID(w->win) == id) {
			/* move to front */
			*l = w->next;
			w->next = windows;
			windows = w;
			return w;
		}
	}
	return nil;
}

/*
 * Recreate the SDL texture to match the current screen image size.
 */
static void
recreate_texture(Sdlwin *w)
{
	if(w->texture)
		SDL_DestroyTexture(w->texture);
	w->texture = SDL_CreateTexture(w->renderer,
		SDL_PIXELFORMAT_XRGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		Dx(w->screenr), Dy(w->screenr));
	if(w->texture == nil)
		fprint(2, "sdl3: SDL_CreateTexture failed: %s\n", SDL_GetError());
}

/*
 * Replace the screen image after a resize.
 */
static int
sdl_replacescreenimage(Client *client)
{
	Memimage *m;
	Sdlwin *w;

	w = (Sdlwin*)client->view;
	m = allocmemimage(w->screenr, XRGB32);
	if(m == nil)
		return 0;
	w->screenimage = m;
	recreate_texture(w);
	client->mouserect = w->screenr;
	sdlunlock();
	gfx_replacescreenimage(client, m);
	sdllock();
	return 1;
}

static Memimage*
sdlattach(Client *client, char *label, char *winsize)
{
	int width, height, havemin;
	Rectangle r;
	SDL_DisplayID display;
	const SDL_DisplayMode *dm;
	Sdlwin *w;

	/*
	 * Determine initial window size.
	 */
	display = SDL_GetPrimaryDisplay();
	dm = SDL_GetCurrentDisplayMode(display);
	if(dm == nil) {
		width = 1024;
		height = 768;
	} else {
		width = dm->w;
		height = dm->h;
	}

	if(winsize && winsize[0]) {
		if(parsewinsize(winsize, &r, &havemin) < 0)
			sysfatal("%r");
	} else {
		r = Rect(0, 0, width*3/4, height*3/4);
		if(Dx(r) > Dy(r)*3/2)
			r.max.x = r.min.x + Dy(r)*3/2;
		if(Dy(r) > Dx(r)*3/2)
			r.max.y = r.min.y + Dx(r)*3/2;
		havemin = 0;
	}

	w = newsdlwin(client);

	w->win = SDL_CreateWindow(
		label ? label : "devdraw",
		Dx(r), Dy(r),
		SDL_WINDOW_RESIZABLE);
	if(w->win == nil)
		sysfatal("SDL_CreateWindow: %s", SDL_GetError());

	w->renderer = SDL_CreateRenderer(w->win, nil);
	if(w->renderer == nil)
		sysfatal("SDL_CreateRenderer: %s", SDL_GetError());

	/* Get actual window size (WM may have adjusted) */
	{
		int ww, wh;
		SDL_GetWindowSize(w->win, &ww, &wh);
		if(ww > 0 && wh > 0)
			r = Rect(0, 0, ww, wh);
	}

	w->screenr = r;
	w->windowrect = r;

	/* Get full display rect for fullscreen toggle */
	{
		SDL_Rect dr;
		if(SDL_GetDisplayBounds(display, &dr))
			w->screenrect = Rect(dr.x, dr.y, dr.x + dr.w, dr.y + dr.h);
		else
			w->screenrect = Rect(0, 0, width, height);
	}

	/* Get DPI */
	{
		float dpi = SDL_GetWindowDisplayScale(w->win);
		if(dpi > 0)
			client->displaydpi = (int)(96.0f * dpi);
	}

	w->screenimage = allocmemimage(r, XRGB32);
	if(w->screenimage == nil)
		sysfatal("allocmemimage: %r");
	client->mouserect = r;

	recreate_texture(w);
	SDL_StartTextInput(w->win);

	return w->screenimage;
}

/*
 * rpc_attach is called on the RPC thread, but SDL requires
 * window and renderer creation on the graphics thread (the
 * thread that called SDL_Init owns the GPU context).
 * Dispatch to the graphics thread and wait, like rpc_resizeimg.
 */
Memimage*
rpc_attach(Client *client, char *label, char *winsize)
{
	SDL_Event ev;
	Memimage *m;

	qlock(&attachlk);
	attachclient = client;
	attachlabel = label;
	attachwinsize = winsize;
	attachresult = nil;
	attachdone = 0;

	memset(&ev, 0, sizeof ev);
	ev.type = attachevent;
	SDL_PushEvent(&ev);

	while(!attachdone)
		rsleep(&attachwait);
	m = attachresult;
	qunlock(&attachlk);
	return m;
}

/*
 * Convert SDL keycode to Plan 9 key.
 * Returns -1 if the key should be ignored.
 */
static int
sdltoplan9kbd(SDL_Keycode k, SDL_Keymod mod)
{
	/* Letters: SDL3 keycodes are lowercase ASCII */
	if(k >= SDLK_A && k <= SDLK_Z) {
		int c = (int)k;
		if(mod & SDL_KMOD_SHIFT)
			c = c - 'a' + 'A';
		if(mod & SDL_KMOD_CTRL)
			c &= 0x9f;
		return c;
	}

	switch(k) {
	case SDLK_RETURN:	return '\n';
	case SDLK_KP_ENTER:	return '\n';
	case SDLK_BACKSPACE:	return '\b';
	case SDLK_TAB:		return '\t';
	case SDLK_ESCAPE:	return 0x1b;
	case SDLK_DELETE:	return 0x7f;

	case SDLK_HOME:		return Khome;
	case SDLK_END:		return Kend;
	case SDLK_LEFT:		return Kleft;
	case SDLK_RIGHT:	return Kright;
	case SDLK_UP:		return Kup;
	case SDLK_DOWN:		return Kdown;
	case SDLK_PAGEUP:	return Kpgup;
	case SDLK_PAGEDOWN:	return Kpgdown;
	case SDLK_INSERT:	return Kins;

	case SDLK_KP_0:	return '0';
	case SDLK_KP_1:	return '1';
	case SDLK_KP_2:	return '2';
	case SDLK_KP_3:	return '3';
	case SDLK_KP_4:	return '4';
	case SDLK_KP_5:	return '5';
	case SDLK_KP_6:	return '6';
	case SDLK_KP_7:	return '7';
	case SDLK_KP_8:	return '8';
	case SDLK_KP_9:	return '9';
	case SDLK_KP_DIVIDE:	return '/';
	case SDLK_KP_MULTIPLY:	return '*';
	case SDLK_KP_MINUS:	return '-';
	case SDLK_KP_PLUS:	return '+';
	case SDLK_KP_PERIOD:	return '.';

	/* Modifier keys by themselves produce no character */
	case SDLK_LSHIFT:
	case SDLK_RSHIFT:
	case SDLK_LCTRL:
	case SDLK_RCTRL:
	case SDLK_LALT:
	case SDLK_RALT:
	case SDLK_LGUI:
	case SDLK_RGUI:
		return -1;
	}

	/* Printable ASCII (space, numbers, punctuation) */
	if(k >= 0x20 && k < 0x7f) {
		int c = (int)k;
		if(mod & SDL_KMOD_CTRL)
			c &= 0x9f;
		return c;
	}

	return -1;
}

/*
 * Convert SDL mouse button to Plan 9 button mask.
 */
static int
sdltoplan9buttons(int sdlbutton)
{
	switch(sdlbutton) {
	case SDL_BUTTON_LEFT:	return 1;
	case SDL_BUTTON_MIDDLE:	return 2;
	case SDL_BUTTON_RIGHT:	return 4;
	case SDL_BUTTON_X1:	return 8;
	case SDL_BUTTON_X2:	return 16;
	}
	return 0;
}

static int buttons;	/* currently held mouse buttons in Plan 9 encoding */
static int mousex, mousey;
static uint mousems;

/*
 * Main SDL event loop. Runs on the graphics thread.
 */
static void
sdlloop(void)
{
	SDL_Event ev;
	Sdlwin *w;
	int c, shift;

	sdllock();
	for(;;) {
		sdlunlock();
		if(!SDL_WaitEvent(&ev)) {
			sdllock();
			continue;
		}
		sdllock();

		w = nil;
		if(ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED
		|| ev.type == SDL_EVENT_WINDOW_RESIZED
		|| ev.type == SDL_EVENT_WINDOW_EXPOSED
		|| ev.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
			w = findsdlwin(ev.window.windowID);
		} else if(ev.type == SDL_EVENT_KEY_DOWN || ev.type == SDL_EVENT_KEY_UP) {
			w = findsdlwin(ev.key.windowID);
		} else if(ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN
		|| ev.type == SDL_EVENT_MOUSE_BUTTON_UP) {
			w = findsdlwin(ev.button.windowID);
		} else if(ev.type == SDL_EVENT_MOUSE_MOTION) {
			w = findsdlwin(ev.motion.windowID);
		} else if(ev.type == SDL_EVENT_MOUSE_WHEEL) {
			w = findsdlwin(ev.wheel.windowID);
		} else if(ev.type == SDL_EVENT_TEXT_INPUT) {
			w = findsdlwin(ev.text.windowID);
		} else if(ev.type == SDL_EVENT_QUIT) {
			threadexitsall(nil);
		}
		if(w == nil)
			w = windows;
		if(w == nil && ev.type != flushevent
		&& ev.type != resizeevent && ev.type != attachevent)
			continue;

		switch(ev.type) {
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			threadexitsall(nil);
			break;

		case SDL_EVENT_WINDOW_EXPOSED:
			_sdlflush(w, w->screenr);
			break;

		case SDL_EVENT_WINDOW_RESIZED: {
			int ww, wh;
			Rectangle r;

			SDL_GetWindowSize(w->win, &ww, &wh);
			r = Rect(0, 0, ww, wh);
			if(ww != Dx(w->screenr) || wh != Dy(w->screenr)) {
				w->screenr = r;
				if(!w->fullscreen)
					w->windowrect = r;
				sdl_replacescreenimage(w->client);
			}
			break;
		}

		case SDL_EVENT_MOUSE_BUTTON_DOWN: {
			int b = sdltoplan9buttons(ev.button.button);
			/* Ctrl-click = button 2, Alt-click = button 3 */
			if(b == 1) {
				SDL_Keymod mod = SDL_GetModState();
				if(mod & SDL_KMOD_CTRL)
					b = 2;
				else if(mod & SDL_KMOD_ALT)
					b = 4;
			}
			altdown = 0;
			buttons |= b;
			mousex = (int)ev.button.x;
			mousey = (int)ev.button.y;
			mousems = (uint)(ev.button.timestamp / 1000000);
			shift = 0;
			if(SDL_GetModState() & SDL_KMOD_SHIFT)
				shift = 5;
			gfx_mousetrack(w->client, mousex, mousey, (buttons|kbuttons)<<shift, mousems);
			break;
		}

		case SDL_EVENT_MOUSE_BUTTON_UP: {
			int b = sdltoplan9buttons(ev.button.button);
			altdown = 0;
			buttons &= ~b;
			mousex = (int)ev.button.x;
			mousey = (int)ev.button.y;
			mousems = (uint)(ev.button.timestamp / 1000000);
			shift = 0;
			if(SDL_GetModState() & SDL_KMOD_SHIFT)
				shift = 5;
			gfx_mousetrack(w->client, mousex, mousey, (buttons|kbuttons)<<shift, mousems);
			break;
		}

		case SDL_EVENT_MOUSE_MOTION:
			mousex = (int)ev.motion.x;
			mousey = (int)ev.motion.y;
			mousems = (uint)(ev.motion.timestamp / 1000000);
			shift = 0;
			if(SDL_GetModState() & SDL_KMOD_SHIFT)
				shift = 5;
			gfx_mousetrack(w->client, mousex, mousey, (buttons|kbuttons)<<shift, mousems);
			break;

		case SDL_EVENT_MOUSE_WHEEL:
			/* Scroll wheel: button 4 (up) or 5 (down) */
			if(ev.wheel.y != 0) {
				int b = ev.wheel.y > 0 ? 8 : 16;
				shift = 0;
				if(SDL_GetModState() & SDL_KMOD_SHIFT)
					shift = 5;
				gfx_mousetrack(w->client, mousex, mousey, (buttons|kbuttons|b)<<shift, mousems);
				gfx_mousetrack(w->client, mousex, mousey, (buttons|kbuttons)<<shift, mousems);
			}
			break;

		case SDL_EVENT_KEY_DOWN: {
			SDL_Keymod mod = ev.key.mod;

			/* Handle Alt tap for Kalt */
			switch(ev.key.key) {
			case SDLK_LALT:
			case SDLK_RALT:
				altdown = 1;
				break;
			default:
				altdown = 0;
				break;
			}

			/* F11 = fullscreen toggle */
			if(ev.key.key == SDLK_F11) {
				w->fullscreen = !w->fullscreen;
				SDL_SetWindowFullscreen(w->win, w->fullscreen);
				break;
			}

			/* Update modifier-based mouse buttons */
			{
				int oldkb = kbuttons;
				kbuttons = 0;
				if(mod & SDL_KMOD_CTRL)
					kbuttons |= 2;
				if(mod & SDL_KMOD_ALT)
					kbuttons |= 4;
				if(oldkb != kbuttons && (buttons || kbuttons)) {
					altdown = 0;
					shift = 0;
					if(mod & SDL_KMOD_SHIFT)
						shift = 5;
					gfx_mousetrack(w->client, mousex, mousey, (buttons|kbuttons)<<shift, mousems);
				}
			}

			/* Don't generate characters for pure modifier presses
			 * or when text input will handle it */
			if(ev.key.key == SDLK_LSHIFT || ev.key.key == SDLK_RSHIFT
			|| ev.key.key == SDLK_LCTRL || ev.key.key == SDLK_RCTRL
			|| ev.key.key == SDLK_LALT || ev.key.key == SDLK_RALT
			|| ev.key.key == SDLK_LGUI || ev.key.key == SDLK_RGUI)
				break;

			/* For control sequences and special keys, use our mapping.
			 * For regular text, SDL_EVENT_TEXT_INPUT handles it. */
			if((mod & SDL_KMOD_CTRL) || ev.key.key > 127
			|| ev.key.key == SDLK_RETURN || ev.key.key == SDLK_KP_ENTER
			|| ev.key.key == SDLK_BACKSPACE || ev.key.key == SDLK_TAB
			|| ev.key.key == SDLK_ESCAPE || ev.key.key == SDLK_DELETE) {
				c = sdltoplan9kbd(ev.key.key, mod);
				if(c >= 0)
					gfx_keystroke(w->client, c);
			}
			break;
		}

		case SDL_EVENT_KEY_UP: {
			SDL_Keymod mod = ev.key.mod;

			/* Alt release = Kalt if no other key was pressed */
			if((ev.key.key == SDLK_LALT || ev.key.key == SDLK_RALT) && altdown) {
				altdown = 0;
				gfx_keystroke(w->client, Kalt);
			}

			/* Update modifier-based mouse buttons */
			{
				int oldkb = kbuttons;
				kbuttons = 0;
				if(mod & SDL_KMOD_CTRL)
					kbuttons |= 2;
				if(mod & SDL_KMOD_ALT)
					kbuttons |= 4;
				if(oldkb != kbuttons && (buttons || kbuttons)) {
					shift = 0;
					if(mod & SDL_KMOD_SHIFT)
						shift = 5;
					gfx_mousetrack(w->client, mousex, mousey, (buttons|kbuttons)<<shift, mousems);
				}
			}
			break;
		}

		case SDL_EVENT_TEXT_INPUT: {
			/*
			 * SDL_EVENT_TEXT_INPUT provides proper Unicode text
			 * including dead key composition, IME, etc.
			 * We use this for regular text and our key handler
			 * above for control sequences and special keys.
			 */
			char *p;
			Rune r;

			/* Skip if ctrl is held — our key handler deals with that */
			if(SDL_GetModState() & SDL_KMOD_CTRL)
				break;

			altdown = 0;
			for(p = (char*)ev.text.text; *p; ) {
				p += chartorune(&r, p);
				if(r != Runeerror && r != 0)
					gfx_keystroke(w->client, r);
			}
			break;
		}

		case SDL_EVENT_WINDOW_FOCUS_LOST:
			/*
			 * Clear modifier state on focus loss so we don't
			 * get stuck keys (e.g. Alt-Tab).
			 */
			kstate = 0;
			kbuttons = 0;
			altdown = 0;
			gfx_abortcompose(w->client);
			break;

		default:
			/*
			 * Handle custom events from RPC thread.
			 */
			if(ev.type == flushevent) {
				Rectangle fr;
				Client *fc;
				Sdlwin *fw;

				qlock(&flushlk);
				fr = flushr;
				fc = flushclient;
				flushr = Rect(0, 0, 0, 0);
				qunlock(&flushlk);

				if(fc != nil) {
					fw = (Sdlwin*)fc->view;
					_sdlflush(fw, fr);
				}
			} else if(ev.type == resizeevent) {
				qlock(&resizelk);
				if(resizeclient != nil)
					sdl_replacescreenimage(resizeclient);
				resizedone = 1;
				rwakeup(&resizewait);
				qunlock(&resizelk);
			} else if(ev.type == attachevent) {
				qlock(&attachlk);
				if(attachclient != nil)
					attachresult = sdlattach(attachclient, attachlabel, attachwinsize);
				attachdone = 1;
				rwakeup(&attachwait);
				qunlock(&attachlk);
			}
			break;
		}
	}
}

void
gfx_main(void)
{
	if(!SDL_Init(SDL_INIT_VIDEO))
		sysfatal("SDL_Init: %s", SDL_GetError());

	flushevent = SDL_RegisterEvents(3);
	if(flushevent == 0)
		sysfatal("SDL_RegisterEvents failed");
	resizeevent = flushevent + 1;
	attachevent = flushevent + 2;
	resizewait.l = &resizelk;
	attachwait.l = &attachlk;

	gfx_started();
	sdlloop();

	SDL_Quit();
	sysfatal("sdl event loop exited");
}

/*
 * Internal flush — must be called with sdllk held.
 */
static void
_sdlflush(Sdlwin *w, Rectangle r)
{
	Memimage *img;
	SDL_Rect sr;

	if(r.min.x >= r.max.x || r.min.y >= r.max.y)
		return;

	img = w->screenimage;
	if(img == nil || w->texture == nil)
		return;

	/*
	 * Upload the dirty region of the memimage to the SDL texture.
	 * The memimage data is in XRGB32 format which matches SDL_PIXELFORMAT_XRGB8888.
	 */
	sr.x = r.min.x;
	sr.y = r.min.y;
	sr.w = Dx(r);
	sr.h = Dy(r);

	{
		uchar *base;
		int stride;

		stride = img->width * sizeof(u32int);
		base = img->data->bdata + r.min.y * stride + r.min.x * 4;
		SDL_UpdateTexture(w->texture, &sr, base, stride);
	}

	SDL_RenderTexture(w->renderer, w->texture, nil, nil);
	SDL_RenderPresent(w->renderer);
}

/*
 * rpc_flush is called from the RPC thread but SDL rendering
 * must happen on the graphics thread. Post a custom event
 * to wake the graphics thread, which does the actual rendering.
 */
void
rpc_flush(Client *client, Rectangle r)
{
	SDL_Event ev;

	qlock(&flushlk);
	if(flushr.min.x >= flushr.max.x)
		flushr = r;
	else
		combinerect(&flushr, r);
	flushclient = client;
	qunlock(&flushlk);

	memset(&ev, 0, sizeof ev);
	ev.type = flushevent;
	SDL_PushEvent(&ev);
}

/*
 * rpc_resizeimg is called from the RPC thread but must create
 * textures on the graphics thread. Dispatch and wait.
 */
void
rpc_resizeimg(Client *client)
{
	SDL_Event ev;

	qlock(&resizelk);
	resizeclient = client;
	resizedone = 0;

	memset(&ev, 0, sizeof ev);
	ev.type = resizeevent;
	SDL_PushEvent(&ev);

	while(!resizedone)
		rsleep(&resizewait);
	qunlock(&resizelk);
}

void
rpc_setlabel(Client *client, char *label)
{
	Sdlwin *w = (Sdlwin*)client->view;

	sdllock();
	SDL_SetWindowTitle(w->win, label ? label : "devdraw");
	sdlunlock();
}

void
rpc_topwin(Client *client)
{
	Sdlwin *w = (Sdlwin*)client->view;

	sdllock();
	SDL_RaiseWindow(w->win);
	sdlunlock();
}

void
rpc_resizewindow(Client *client, Rectangle r)
{
	Sdlwin *w = (Sdlwin*)client->view;

	sdllock();
	SDL_SetWindowSize(w->win, Dx(r), Dy(r));
	sdlunlock();
}

void
rpc_setmouse(Client *client, Point p)
{
	Sdlwin *w = (Sdlwin*)client->view;

	sdllock();
	SDL_WarpMouseInWindow(w->win, (float)p.x, (float)p.y);
	sdlunlock();
}

void
rpc_setcursor(Client *client, Cursor *c, Cursor2 *c2)
{
	SDL_Cursor *sc;
	SDL_Surface *surface;
	uchar src[2*16], mask[2*16];
	int i, x, y;
	u32int *pixels;

	USED(c2);

	sdllock();
	if(c == nil) {
		SDL_SetCursor(SDL_GetDefaultCursor());
		sdlunlock();
		return;
	}

	/*
	 * Convert Plan 9 cursor format to ARGB surface.
	 * Plan 9 cursor: 16x16, two bit planes (set and clr).
	 * set=1: black pixel
	 * clr=1: white pixel
	 * both 0: transparent
	 */
	for(i = 0; i < 2*16; i++) {
		src[i] = c->set[i];
		mask[i] = c->set[i] | c->clr[i];
	}

	surface = SDL_CreateSurface(16, 16, SDL_PIXELFORMAT_ARGB8888);
	if(surface == nil) {
		sdlunlock();
		return;
	}
	pixels = (u32int*)surface->pixels;

	for(y = 0; y < 16; y++) {
		for(x = 0; x < 16; x++) {
			int byteoff = y*2 + x/8;
			int bit = 0x80 >> (x % 8);
			if(src[byteoff] & bit)
				pixels[y*16+x] = 0xFF000000;	/* black */
			else if(mask[byteoff] & bit)
				pixels[y*16+x] = 0xFFFFFFFF;	/* white */
			else
				pixels[y*16+x] = 0x00000000;	/* transparent */
		}
	}

	sc = SDL_CreateColorCursor(surface, -c->offset.x, -c->offset.y);
	SDL_DestroySurface(surface);
	if(sc != nil)
		SDL_SetCursor(sc);
	sdlunlock();
}

void
rpc_bouncemouse(Client *client, Mouse m)
{
	USED(client);
	USED(m);
	/* No equivalent in SDL — this is for rio/9wm integration only. */
}

/*
 * Clipboard support.
 * SDL3 provides simple text clipboard which covers the CLIPBOARD selection.
 * PRIMARY selection (middle-click paste) is not supported by SDL3.
 */
static QLock cliplk;
static char *clipbuf;

char*
rpc_getsnarf(void)
{
	char *s, *ret;

	qlock(&cliplk);
	ret = nil;
	s = SDL_GetClipboardText();
	if(s != nil) {
		if(s[0])
			ret = strdup(s);
		SDL_free(s);
	}
	if(ret == nil && clipbuf != nil)
		ret = strdup(clipbuf);
	qunlock(&cliplk);
	return ret;
}

void
rpc_putsnarf(char *data)
{
	qlock(&cliplk);
	free(clipbuf);
	clipbuf = strdup(data);
	SDL_SetClipboardText(data);
	qunlock(&cliplk);
}

void
rpc_shutdown(void)
{
}

/*
 * No draw lock needed — all drawing is in software memdraw,
 * same as the macOS backend.
 */
void
rpc_gfxdrawlock(void)
{
}

void
rpc_gfxdrawunlock(void)
{
}
