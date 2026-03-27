# SDL3 Backend for devdraw — Replacing X11

## Motivation

Acme (and all Plan 9 graphical tools) depend on devdraw, which on Linux uses
X11 directly. This means running under XWayland on modern desktops. An SDL3
backend provides native Wayland support, plus portability to any platform
SDL3 supports.

## Architecture

The X11 dependency is isolated behind a clean `ClientImpl` interface in
`devdraw.h`:

```c
struct ClientImpl {
    void (*rpc_resizeimg)(Client*);
    void (*rpc_resizewindow)(Client*, Rectangle);
    void (*rpc_setcursor)(Client*, Cursor*, Cursor2*);
    void (*rpc_setlabel)(Client*, char*);
    void (*rpc_setmouse)(Client*, Point);
    void (*rpc_topwin)(Client*);
    void (*rpc_bouncemouse)(Client*, Mouse);
    void (*rpc_flush)(Client*, Rectangle);
};
```

Backends also implement memory image operations (`allocmemimage`, `memimagedraw`,
etc.) and an event loop calling `gfx_mousetrack()` / `gfx_keystroke()`.

## X11 to SDL3 Mapping

| Feature | X11 (~3,500 lines, 10 files) | SDL3 (~965 lines, 2 files) |
|---------|-------------|-----------------|
| Window creation | XCreateWindow + ICCCM properties | SDL_CreateWindow |
| Window title/raise | XSetWMProperties, XMapRaised | SDL_SetWindowTitle, SDL_RaiseWindow |
| Mouse/keyboard | XNextEvent loop, XLookupString, 877-line keysym table | SDL_WaitEvent, SDL_EVENT_TEXT_INPUT |
| Custom cursor | XCreatePixmapCursor from bitmaps | SDL_CreateColorCursor from ARGB surface |
| Mouse warp | XWarpPointer + XWayland hack | SDL_WarpMouseInWindow |
| Pixel upload | XPutImage to server pixmap, XCopyArea to window | SDL_UpdateTexture + SDL_RenderTexture |
| Pixel readback | XGetSubImage (server to client) | Not needed (RAM is authoritative) |
| Clipboard | X11 selection protocol (PRIMARY, CLIPBOARD, TARGETS, UTF8_STRING) | SDL_SetClipboardText / SDL_GetClipboardText |
| Color management | 8-bit RGBV colormap, XStoreColors, byte-order detection | XRGB32 only (matches SDL_PIXELFORMAT_XRGB8888 directly) |
| DPI | Xft.dpi from X resource database | SDL_GetWindowDisplayScale |
| Fullscreen | Manual XConfigureWindow with screen rect | SDL_SetWindowFullscreen |
| Thread safety | Single QLock around all X11 calls | Custom SDL events to dispatch render ops to graphics thread |

## Implementation

### Files created
- **`src/cmd/devdraw/sdl3-screen.c`** (~900 lines) — Complete backend:
  window management, event loop, input handling (keyboard, mouse, scroll,
  text input with IME), custom cursor, clipboard, all `rpc_*` functions,
  `gfx_main`, cross-thread flush/resize dispatch.
- **`src/cmd/devdraw/sdl3-draw.c`** (~65 lines) — Software memdraw operations,
  identical to mac-draw.c. Pure software drawing, no GPU acceleration, no
  server-side pixmaps.

### Files modified
- **`src/cmd/devdraw/mkwsysrules.sh`** — Added SDL3 detection via pkg-config
  or sdl3-config. SDL3 is preferred over X11 when available; set `WSYSTYPE=x11`
  to override.

### Design decisions

1. **Software rendering with texture upload.** All drawing happens in client
   RAM via the existing `memdraw` library (same as macOS backend). Dirty
   regions are uploaded to an SDL texture at flush time via
   `SDL_UpdateTexture`. This eliminates the X11 backend's server-side pixmaps,
   GC caching, and pixel round-trips.

2. **Cross-thread dispatch.** SDL3 requires all `SDL_Render*` calls on the
   thread that created the renderer. Since `rpc_flush` is called from the RPC
   thread, it posts a custom SDL event; the graphics thread picks it up and
   does the actual `SDL_UpdateTexture` / `SDL_RenderPresent`. `rpc_resizeimg`
   uses the same pattern with a `Rendez` condition variable for synchronous
   completion (the caller must wait for the new screen image).

3. **XRGB32 pixel format.** Plan 9's XRGB32 (ignore, R, G, B at 8 bits each)
   maps directly to SDL's `SDL_PIXELFORMAT_XRGB8888` with no conversion.
   The X11 backend had to detect byte order and potentially swap to XBGR32;
   SDL3 handles this transparently.

4. **Keyboard input.** Regular text comes via `SDL_EVENT_TEXT_INPUT` (handles
   dead keys, IME, Unicode composition). Control sequences and special keys
   (arrows, Home/End, function keys) are handled directly from
   `SDL_EVENT_KEY_DOWN` with explicit keycode mapping. The 877-line
   `x11-keysym2ucs.c` table is eliminated entirely.

5. **Modifier chord emulation.** Ctrl-click = button 2, Alt-click = button 3,
   matching the X11 backend. Modifier changes while buttons are held generate
   synthetic mouse events. Shift applies a 5-bit left shift to the button
   mask, matching Plan 9 convention.

### API verification

All SDL3 constants and API usage verified against SDL3 documentation:
- `ev.key.key` is the `SDL_Keycode` (SDL3 removed the `keysym` intermediate
  struct that SDL2 used as `ev.key.keysym.sym`)
- `SDLK_A`..`SDLK_Z` are lowercase ASCII values ('a'..'z')
- Event timestamps are nanoseconds (`Uint64`), converted to milliseconds
  for Plan 9's `uint msec` field
- All `SDL_EVENT_*`, `SDL_KMOD_*`, `SDL_BUTTON_*`, `SDL_PIXELFORMAT_*`
  constants confirmed valid for SDL3
- `SDL_GetClipboardText` returns SDL-allocated string, freed with `SDL_free`
- `SDL_Init` returns `bool` (true on success) in SDL3

### What's not implemented
- **PRIMARY selection** — SDL3 only exposes CLIPBOARD, not X11 PRIMARY
  selection. Middle-click paste from other X11 apps won't work.
- **rpc_bouncemouse** — No-op. This is for rio/9wm window manager integration.
- **8-bit indexed color** — Only XRGB32. No modern display needs 8-bit.

## Building

Install SDL3 development libraries, then build normally:
```sh
# Auto-detect (prefers SDL3 over X11)
./INSTALL

# Force a specific backend
WSYSTYPE=sdl3 ./INSTALL
WSYSTYPE=x11 ./INSTALL
```

## Remaining work

- Test with acme, 9term, and other graphical Plan 9 tools.
- Performance comparison vs X11 backend (expected: equivalent or better for
  text editor workloads, since we avoid X server round-trips).
- Verify on Wayland compositors (Sway, GNOME, KDE).
- Consider PRIMARY selection support via SDL3's X11 interop
  (`SDL_GetProperty` to get the underlying X11 display/window, then use
  X11 selection APIs directly for middle-click paste).
