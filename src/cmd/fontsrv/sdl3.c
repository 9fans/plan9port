/*
 * fontsrv uses fontconfig/freetype for font enumeration,
 * independent of the display backend. When devdraw uses SDL3,
 * fontsrv still uses the same fontconfig/freetype code as X11.
 */
#include "x11.c"
