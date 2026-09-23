/*
 *  fontnames-array takes two fonts, first one
 *  it treats as a proportional-width font and uses
 *  everywhere possible and as a main UI font, while
 *  the second font it treats as a fixed-width font,
 *  changeable to any text window by executing `Font`
 *  from tag window.
 *
 *  Note: `Font` can also be executed
 *  with arguments with any `fontsrv -p .` approved
 *  fonts, thus allowing you to experiment with font
 *  sizes and whether you want anti-aliasing or not.
 */

char *fontnames[2] = {
	"/lib/font/bit/profont/profont-12.font",
	"/lib/font/bit/mntcarlo/mntcarlo.font"
};

/*
 *  globalautoindent tries to guess where to
 *  indent by the context of the previous line.
 *
 *  comes highly suggested.
 */

int globalautoindent	= TRUE;

/*
 *  swapping scroll buttons makes B1 scroll down
 *  instead of up and B2 up instead of down
 */

int	swapscrollbuttons	= FALSE;

/*
 *  bartflag triggers quote unquote experimental
 *  mode, in which instead of mouse following focus
 *  from window to window etc. it in fact, does not.
 *
 *  comes highly suggested.
 */

int bartflag			= TRUE;

/*
 *  two compile-time palettes, switched at runtime by executing
 *  the built-in Dark / Light commands from any tag (same as
 *  Put or Font).  darkmode picks the scheme acme starts
 *  with.  colors are 0xRRGGBBFF, no hashtag.
 *
 *  slots, in order: tag back/fore/hlback/hlfore, text
 *  back/fore/hlback/hlfore, column button, window button
 *  (tag border), dirty marker, scrollbar, button-2 highlight,
 *  button-3 highlight, desktop, borders.
 */

int darkmode = FALSE;

enum {
	P_TAGBG = 0, P_TAGFG, P_TAGHLBG, P_TAGHLFG,
	P_TXTBG, P_TXTFG, P_TXTHLBG, P_TXTHLFG,
	P_WINBUTTON, P_COLBUTTON, P_TMPBUTTON, P_SCROLLBG,
	P_BUTTON2HL, P_BUTTON3HL, P_DESKTOPBG, P_BORDER,
	P_NCOLORS
};

/*  classic-dark: a dark take on the classic acme hues.
 *  cool slate tags vs. warm body, gold column button, teal
 *  window button, red dirty marker, olive scrollbar, and a
 *  near-black desktop so empty space never blinds.  */
static uint darkpal[P_NCOLORS] = {
	0x252B33FF,	/* P_TAGBG */
	0xB9C4CFFF,	/* P_TAGFG */
	0x3E4A57FF,	/* P_TAGHLBG */
	0xE6ECF2FF,	/* P_TAGHLFG */
	0x282620FF,	/* P_TXTBG */
	0xD8D3C6FF,	/* P_TXTFG */
	0x6E5F22FF,	/* P_TXTHLBG */
	0xF5F1E4FF,	/* P_TXTHLFG */
	0xC9973BFF,	/* P_WINBUTTON */
	0x3E8F8FFF,	/* P_COLBUTTON */
	0xB84A4AFF,	/* P_TMPBUTTON */
	0x5A6B3AFF,	/* P_SCROLLBG */
	0xAA3A3AFF,	/* P_BUTTON2HL */
	0x3E7A4EFF,	/* P_BUTTON3HL */
	0x14161AFF,	/* P_DESKTOPBG */
	0x3A4048FF,	/* P_BORDER */
};

/*  the default classic acme colors: pale blue-green tags,
 *  cream body, purple-blue buttons, blue dirty marker,
 *  yellow-green scrollbar, red/green button flashes.  */
static uint lightpal[P_NCOLORS] = {
	0xEAFFFFFF,	/* P_TAGBG */
	0x000000FF,	/* P_TAGFG */
	0x9EEEEEFF,	/* P_TAGHLBG */
	0x000000FF,	/* P_TAGHLFG */
	0xFFFFEAFF,	/* P_TXTBG */
	0x000000FF,	/* P_TXTFG */
	0xEEEE9EFF,	/* P_TXTHLBG */
	0x000000FF,	/* P_TXTHLFG */
	0x8888CCFF,	/* P_WINBUTTON */
	0x8888CCFF,	/* P_COLBUTTON */
	0x000099FF,	/* P_TMPBUTTON */
	0x99994CFF,	/* P_SCROLLBG */
	0xAA0000FF,	/* P_BUTTON2HL */
	0x006600FF,	/* P_BUTTON3HL */
	0xFFFFFFFF,	/* P_DESKTOPBG */
	0x000000FF,	/* P_BORDER */
};
