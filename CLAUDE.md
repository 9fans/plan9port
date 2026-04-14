# Project Context

This is **plan9port** (Plan 9 from User Space) — a port of Plan 9 from Bell
Labs tools and libraries to Unix/Linux. Primarily maintained by Russ Cox (rsc).

## Repository Layout

- `src/cmd/` — commands (acme, sam, rc, devdraw, 9term, etc.)
- `src/lib*/` — 34 libraries (lib9, libdraw, libthread, libmemdraw, etc.)
- `bin/` — installed binaries (mix of tracked shell scripts and built executables)
- `lib/` — support files (keyboard, fonts, plumbing rules)
- `include/` — Plan 9 C headers (u.h, libc.h, draw.h, etc.)
- `man/` — manual pages

## Build System

Uses Plan 9 `mk` (not make). Build files are named `mkfile`.
- `src/mkhdr` — common build variables ($O=o, CC=9c, LD=9l)
- `src/mkone` — template for single-binary commands
- `src/mklib` — template for libraries
- `src/mkcommon` — shared clean/nuke targets
- `./INSTALL` — top-level build script (calls mk internally)
- `./INSTALL -b` — build only, don't rewrite paths
- `Makefile` — GNU make wrapper with `clean` and `distclean` targets

## SDL3 Backend (active work)

We replaced the X11 backend in `devdraw` with SDL3 for native Wayland support.

### Files we created/modified:
- `src/cmd/devdraw/sdl3-screen.c` (~900 lines) — SDL3 graphics backend
- `src/cmd/devdraw/sdl3-draw.c` (~65 lines) — software memdraw ops (like mac-draw.c)
- `src/cmd/devdraw/mkwsysrules.sh` — added SDL3 detection, preferred over X11
- `src/cmd/fontsrv/sdl3.c` — includes x11.c (fontsrv uses fontconfig, not display)
- `src/cmd/fontsrv/freetyperules.sh` — added sdl3 case for freetype flags
- `install.txt` — documented SDL3 as preferred backend
- `INSTALL` — added SDL3 detection diagnostic message
- `Makefile` — added clean/distclean targets

### Key design points:
- Software rendering via memdraw, texture upload at flush (like macOS backend)
- Cross-thread dispatch: rpc_flush posts custom SDL event, graphics thread renders
- rpc_resizeimg uses Rendez condition variable for synchronous dispatch
- XRGB32 maps directly to SDL_PIXELFORMAT_XRGB8888
- No PRIMARY selection (SDL3 only exposes CLIPBOARD)
- rpc_bouncemouse is a no-op (rio/9wm specific)
- fontsrv reuses x11.c via include (fontconfig/freetype, not display)

### Backend interface (devdraw.h):
```c
struct ClientImpl {
    rpc_resizeimg, rpc_resizewindow, rpc_setcursor, rpc_setlabel,
    rpc_setmouse, rpc_topwin, rpc_bouncemouse, rpc_flush
};
```
Plus: gfx_main, rpc_attach, rpc_getsnarf, rpc_putsnarf, rpc_shutdown,
rpc_gfxdrawlock/unlock, and memdraw ops (allocmemimage, memimagedraw, etc.)

## Documentation files we created:
- `ACME.md` — detailed analysis of acme editor (~15K lines, 32 files)
- `SDL3.md` — SDL3 backend design doc with full implementation details

## Coding style
- Plan 9 C style: terse, minimal abstractions, tabs for indentation
- nil instead of NULL (defined as 0 in u.h)
- Functions: emalloc, smprint, sysfatal, fprint, werrstr
- Threading: libthread with QLock, Rendez, channels (CSP-style)
