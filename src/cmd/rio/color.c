/* Copyright (c) 2004 Russ Cox, see README for licence details */
#include <stdio.h>
#include <signal.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "dat.h"
#include "fns.h"

unsigned long
colorpixel(Display *dpy, int depth, unsigned long rgb)
{
	int r, g, b;

	r = rgb>>16;
	g = (rgb>>8)&0xFF;
	b = rgb&0xFF;

	switch(depth){
	case 1:
	case 2:
	case 4:
	case 8:
	default:
		/* not going to waste color map entries */
		if(rgb == 0xFFFFFF)
			return WhitePixel(dpy, DefaultScreen(dpy));
		return BlackPixel(dpy, DefaultScreen(dpy));
	case 15:
		r >>= 3;
		g >>= 3;
		b >>= 3;
		return (r<<10) | (g<<5) | b;
	case 16:
		r >>= 3;
		g >>= 2;
		b >>= 3;
		return (r<<11) | (g<<5) | b;
	case 24:
	case 32:
		return rgb;
	}
}

