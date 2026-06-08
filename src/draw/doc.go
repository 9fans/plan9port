// Package draw is a port of Plan 9's libdraw to Go.
// It connects to the 'devdraw' binary built as part of
// Plan 9 from User Space (http://swtch.com/plan9port/).
// All graphics operations are done in the remote server.
//
// Displays
//
// Graphics operations are mediated through a Display, obtained by calling Init.
// See the Display documentation for details.
//
// Colors and Pixel Formats
//
// This package represents colors as RGBA values, 8 bits per channel,
// packed into a uint32 type called Color. Color implements the image/color
// package's Color interface.
//
// The images in this package store pixel values in more compact formats
// specified by a pixel format represented by the Pix type.
// (Plan 9 C calls this format a ‘chan’ but that name is already taken in Go.)
// The Pix details the sequence of image channels packed into each pixel.
// For example RGB24, defined as MakePix(CRed, 8, CGreen, 8, CBlue, 8),
// describes a 24-bit pixel format consisting of 8 bits each for red, green, and blue,
// with no explicit alpha channel.
//
// The external representation of a Pix is a string, a sequence of two-character
// channel descriptions, each comprising a letter (r for red, g for green,
// b for blue, a for alpha, m for color-mapped, k for greyscale, and
// x for “don't care”) followed by a number of bits per pixel.
// The sum of the channel bits per pixel is the depth of the image, which
// must be either a divisor or a multiple of eight. It is an error to
// have more than one of any channel but x. An image must have either a
// greyscale channel; a color mapped channel; or red, green, and blue
// channels. If the alpha channel is present, it must be at least as deep
// as any other channel. For example, RGB24 is “r8g8b8”.
//
// The packing of 1-, 2- or 4-bit pixels into bytes is big-endian,
// meaning that the first pixel in a 1-bit image is the 0x80 bit.
// But the packing of 16-, 24-, and 32-bit pixels into byte data
// is little-endian, meaning that the byte order for the RGB24 format
// is actually blue, green, red. This odd convention was chosen for
// compatibility with Plan 9, which in turn chose it for compatibility
// with VGA frame buffers, especially 16-bit pixel data like RGB16.
// Counterintuitively, then, the pixel formats corresponding to Go's
// image.RGBA are ABGR32 or XBGR32.
//
// The color-mapped values, which now are only of historical interest,
// use a 4x4x4 subdivision with 4 shades in each subcube.
// See https://9fans.github.io/plan9port/man/man7/color.html
// for the details.
//
// Image Format
//
// Fonts and images as used by Image.Load, Image.Unload, Display.ReadImage,
// and so on are stored in a machine-independent format defined by Plan 9.
// See https://9fans.github.io/plan9port/man/man7/image.html
// for the details.
//
// Fonts
//
// External bitmap fonts are described by a plain text file that can be
// read using Display.OpenFont.
// See https://9fans.github.io/plan9port/man/man7/font.html
// for the details.
//
// Font Names
//
// Font names in this package (following Plan 9 from User Space) are a
// small language describing a font. The most basic form is the name of
// an existing bitmap font file, following the convention:
//
//	/lib/font/bit/name/range.size.font
//
// where size is approximately the height in pixels of the lower case
// letters (without ascenders or descenders). Range gives some indication
// of which characters will be available: for example ascii, latin1,
// euro, or unicode. Euro includes most European languages, punctuation
// marks, the International Phonetic Alphabet, etc., but no Oriental
// languages. Unicode includes every character for which
// appropriate-sized images exist on the system.
//
// In Plan 9 from User Space, the font files are rooted in $PLAN9/font
// instead of /lib/font/bit, but to keep old references working, paths
// beginning with /lib/font/bit are interpreted as references to the
// actual font directory.
//
// Fonts need not be stored on disk in the Plan 9 format. If the font
// name has the form /mnt/font/name/size/font, fontsrv is invoked to
// synthesize a bitmap font from the operating system's installed vector
// fonts. The command ‘fontsrv -p .’ lists the available fonts.
// See https://9fans.github.io/plan9port/man/man4/fontsrv.html for more.
//
// If the font name has the form scale*fontname, where scale is a small
// decimal integer, the fontname is loaded and then scaled by pixel
// repetition.
//
// The Plan 9 bitmap fonts were designed for screens with pixel density
// around 100 DPI. When used on screens with pixel density above 200 DPI,
// the bitmap fonts are automatically pixel doubled. Similarly, fonts
// loaded from https://9fans.github.io/plan9port/man/man4/fontsrv.html
// are automatically doubled in size by varying
// the effective size path element. In both cases, the effect is that a
// single font name can be used on both low- and high-density displays
// (or even in a window moved between differing displays) while keeping
// roughly the same effective size.
//
// For more control over the fonts used on low- and high-density
// displays, if the font name has the form ‘lowfont,highfont’, then
// lowfont is used on low-density displays and highfont on high-density
// displays. In effect, the behavior described above is that the font
// name
//
//	/lib/font/bit/lucsans/euro.8.font
//
// really means
//
//	/lib/font/bit/lucsans/euro.8.font,2*/lib/font/bit/lucsans/euro.8.font
//
// and similarly
//
//	/mnt/font/LucidaGrande/15a/font
//
// really means
//
//	/mnt/font/LucidaGrande/15a/font,/mnt/font/LucidaGrande/30a/font
//
// Using an explicit comma-separated font pair allows finer control, such
// as using a Plan 9 bitmap font on low-density displays but switching to
// a system-installed vector font on high-density displays:
//
//	/lib/font/bit/lucsans/euro.8.font,/mnt/font/LucidaGrande/30a/font
//
// Libdraw Cheat Sheet
//
// The mapping from the Plan 9 C libdraw names defined in <draw.h>
// to names in this package (omitting unchanged names) is:
//
//	ARROW → Arrow
//	Borderwidth → BorderWidth
//	CACHEAGE → unexported
//	CHAN1, CHAN2, CHAN3, CHAN4 → MakePix
//	Cachefont → unexported
//	Cacheinfo → unexported
//	Cachesubf → unexported
//	DBlack → Black
//	DBlue → Blue
//	DBluegreen → BlueGreen
//	DCyan → Cyan
//	DDarkYellow → DarkYellow
//	DDarkblue → DarkBlue
//	DDarkgreen → DarkGreen
//	DGreen → Green
//	DGreyblue → GreyBlue
//	DGreygreen → GreyGreen
//	DMagenta → Magenta
//	DMedGreen → MedGreen
//	DMedblue → MedBlue
//	DNofill → NoFill
//	DNotacolor → undefined
//	DOpaque → Opaque
//	DPaleYellow → PaleYellow
//	DPaleblue → PaleBlue
//	DPalebluegreen → PaleBlueGreen
//	DPalegreen → PaleGreen
//	DPalegreyblue → PaleGreyBlue
//	DPalegreygreen → PaleGreyGreen
//	DPurpleblue → PurpleBlue
//	DRed → Red
//	DSUBF → unexported
//	DTransparent → Transparent
//	DWhite → White
//	DYellow → Yellow
//	DYellowgreen → YellowGreen
//	Displaybufsize → unexported
//	Drawop → Op
//	Dx → Rectangle.Dx
//	Dy → Rectangle.Dy
//	Endarrow → EndArrow
//	Enddisc → EndDisc
//	Endmask → EndMask
//	Endsquare → EndSquare
//	Font → Font
//	Fontchar → Fontchar
//	Image → Image
//	LOG2NFCACHE → unexported
//	MAXFCACHE → unexported
//	MAXSUBF → unexported
//	NBITS, TYPE → Pix.Split
//	NFCACHE → unexported
//	NFLOOK → unexported
//	NFSUBF → unexported
//	NOREFRESH → not available
//	Pfmt → not implemented; use %v instead of %P
//	Point → Point (alias for image.Point)
//	Rectangle → Rectangle (alias for image.Rectangle)
//	Refbackup → RefBackup
//	Refmesg → RefMesg
//	Refnone → RefNone
//	Rfmt → not implemented; use %v instead of %P
//	SUBFAGE → unexported
//	_allocwindow → unexported
//	_screen → Display.Screen
//	_string → unexported; use String, Runes, etc.
//	addpt → Point.Add
//	agefont → unexported
//	allocimage → Display.AllocImage
//	allocimagemix → Display.AllocImageMix
//	allocscreen → Image.AllocScreen
//	allocsubfont → unexported
//	allocwindow → unexported
//	arc → Image.Arc
//	arcop → Image.ArcOp
//	bezier → Image.Bezier
//	bezierop → Image.BezierOp
//	bezspline → Image.BSpline
//	bezsplineop → Image.BSplineOp
//	bezsplinepts → unexported
//	border → Image.Border
//	borderop → Image.BorderOp
//	bottomnwindows → unexported
//	bottomwindow → unexported
//	bufimage → unexported
//	buildfont → Display.BuildFont
//	bytesperline → BytesPerLine
//	cachechars → unexported
//	canonrect → Rectangle.Canon
//	chantodepth → Pix.Depth
//	chantostr → Pix.String
//	cloadimage → Image.Cload
//	closedisplay → Display.Close
//	cmap2rgb → unexported
//	cmap2rgba → unexported
//	combinerect → CombineRect
//	creadimage → not available; use Display.ReadImage (readimage) with "compressed\n" prefix
//	cursorset → Display.MoveCursor
//	cursorswitch → Display.SwitchCursor
//	display → global variable removed; use result of Init
//	divpt → Point.Div
//	draw → Image.Draw
//	drawerror → not available
//	drawlsetrefresh → not available
//	drawop → Image.DrawOp
//	drawrepl → Repl
//	drawreplxy → ReplXY
//	drawresizewindow → Display.Resize
//	drawsetlabel → Display.SetLabel
//	drawtopwindow → Display.Top
//	ellipse → Image.Ellipse
//	ellipseop → Image.EllipseOp
//	eqpt → Point.Eq (or ==)
//	eqrect → Rectangle.Eq (but treats all empty rectangles equal)
//	fillarc → Image.FillArc
//	fillarcop → Image.FillArcOp
//	fillbezier, fillbezierop → not implemented
//	fillbezspline, fillbezsplineop → not implemented
//	fillellipse → Image.FillEllipse
//	fillellipseop → Image.FillEllipseOp
//	fillpoly → Image.FillPoly
//	fillpolyop → Image.FillPolyOp
//	flushimage → Display.Flush
//	font → Display.Font
//	freefont → Font.Free
//	freeimage → Image.Free
//	freescreen → Screen.Free
//	freesubfont → unexported
//	gendraw → Image.GenDraw
//	gendrawop → Image.GenDrawOp
//	gengetwindow → not available; use Display.Attach (getwindow)
//	geninitdraw → not available; use Init (initdraw)
//	getwindow → Display.Attach
//	icossin → IntCosSin
//	icossin2 → IntCosSin2
//	initdraw → Init
//	insetrect → Rectangle.Inset
//	installsubfont → unexported
//	line → Image.Line
//	lineop → Image.LineOp
//	loadchar → unexported
//	loadhidpi → unexported
//	loadimage → Image.Load
//	lockdisplay → unexported
//	lookupsubfont → unexported
//	mkfont → unexported
//	mulpt → Point.Mul
//	namedimage → not available
//	nameimage → not available
//	newwindow → not available
//	openfont → Display.OpenFont
//	originwindow → unexported
//	parsefontscale → unexported
//	poly → Image.Poly
//	polyop → Image.PolyOp
//	ptinrect → Point.In
//	publicscreen → unexported
//	readimage → Display.ReadImage
//	readsubfont → unexported
//	readsubfonti → unexported
//	rectXrect → RectXRect
//	rectaddpt → Rectangle.Add
//	rectclip → RectClip
//	rectinrect → RectInRect
//	rectsubpt → Rectangle.Sub
//	replclipr → Image.ReplClipr
//	rgb2cmap → unexported
//	runestring, runestringn → Image.Runes
//	runestringbg, runestringnbg → Image.RunesBg
//	runestringbgop, runestringnbgop → Image.RunesBgOp
//	runestringop, runestringnop → Image.RunesOp
//	runestringsize → Font.RunesSize
//	runestringwidth, runestringnwidth → Font.RunesWidth
//	scalecursor → ScaleCursor
//	scalesize → Display.Scale
//	screen → Display.ScreenImage
//	setalpha → Color.WithAlpha
//	string, stringn → Image.String
//	stringbg, stringnbg → Image.StringBg
//	stringbgop, stringnbgop → Image.StringBgOp
//	stringop, stringnop → Image.StringOp
//	stringsize → Font.StringSize
//	stringsubfont → not implemented
//	stringwidth, stringnwidth → Font.StringWidth
//	strsubfontwidth → not available
//	strtochan → ParsePix
//	subfontname → unexported
//	subpt → Point.Sub
//	swapfont → unexported
//	topnwindows → unexported
//	topwindow → unexported
//	uint32 as color type → Color
//	uint32 as image channel (format) type → Pix
//	uninstallsubfont → unexported
//	unloadimage → Image.Unload
//	unlockdisplay → unexported
//	wordsperline → WordsPerLine
//	writeimage → not implemented (TODO)
//	writesubfont → not available
//
// Note that the %P and %R print formats are now simply %v,
// but since Point and Rectangle are aliases for the types in package image,
// the formats have changed: Points and Rectangles format as
// (1,2) and (1,2)-(3,4) instead of [1 2] and [[1 2] [3 4]].
//
package draw
