/* Copyright (c) 2004 Russ Cox, see README for licence details */
#include <stdio.h>
#include <signal.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "dat.h"
#include "fns.h"

unsigned long
colorpixel(Display *dpy, ScreenInfo *s, int depth, unsigned long rgb, unsigned long def)
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
		return def;
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
		/* try to find byte order */
		if(s->vis->red_mask & 0xff)
			return (r) | (g<<8) | (b<<16); /* OK on Sun */
		return rgb;
	}
}
