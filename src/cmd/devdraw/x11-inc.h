#define Colormap	XColormap
#define Cursor		XCursor
#define Display		XDisplay
#define Drawable	XDrawable
#define Font		XFont
#define GC		XGC
#define Point		XPoint
#define Rectangle	XRectangle
#define Screen		XScreen
#define Visual		XVisual
#define Window		XWindow

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#ifdef SHOWEVENT
#include <stdio.h>
#include "../rio/showevent/ShowEvent.c"
#endif

#undef Colormap
#undef Cursor
#undef Display
#undef Drawable
#undef Font
#undef GC
#undef Point
#undef Rectangle
#undef Screen
#undef Visual
#undef Window
