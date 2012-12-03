#include <u.h>

#define Point OSXPoint
#define Rect OSXRect
#define Cursor OSXCursor
#include <Carbon/Carbon.h>
#undef Rect
#undef Point
#undef Cursor
#undef offsetof
#undef nil

#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include "a.h"


extern void CGFontGetGlyphsForUnichars(CGFontRef, const UniChar[], const CGGlyph[], size_t);

int
mapUnicode(int i)
{
	switch(i) {
	case '\'':
		return 0x2019;
	case '`':
		return 0x2018;
	}
	return i;
}

char*
mac2c(CFStringRef s)
{
	char *p;
	int n;

	n = CFStringGetLength(s)*8;	
	p = malloc(n);
	CFStringGetCString(s, p, n, kCFStringEncodingUTF8);
	return p;
}

CFStringRef
c2mac(char *p)
{
	return CFStringCreateWithBytes(nil, (uchar*)p, strlen(p), kCFStringEncodingUTF8, false);
}

Rectangle
mac2r(CGRect r, int size, int unit)
{
	Rectangle rr;

	rr.min.x = r.origin.x*size/unit;
	rr.min.y = r.origin.y*size/unit;
	rr.max.x = (r.origin.x+r.size.width)*size/unit + 0.99999999;
	rr.max.y = (r.origin.x+r.size.width)*size/unit + 0.99999999;
	return rr;
}

void
loadfonts(void)
{
	int i, n;
	CTFontCollectionRef allc;
	CFArrayRef array;
	CFStringRef s;
	CTFontDescriptorRef f;

	allc = CTFontCollectionCreateFromAvailableFonts(0);
	array = CTFontCollectionCreateMatchingFontDescriptors(allc);
	n = CFArrayGetCount(array);
	xfont = emalloc9p(n*sizeof xfont[0]);
	for(i=0; i<n; i++) {
		f = (void*)CFArrayGetValueAtIndex(array, i);
		if(f == nil)
			continue;
		s = CTFontDescriptorCopyAttribute(f, kCTFontNameAttribute);
		xfont[nxfont].name = mac2c(s);		
		CFRelease(s);
		nxfont++;
	}
}

CGRect
subfontbbox(CGFontRef font, int lo, int hi)
{
	int i, first;
	CGRect bbox;

	bbox.origin.x = 0;
	bbox.origin.y = 0;
	bbox.size.height = 0;
	bbox.size.width = 0;

	first = 1;
	for(i=lo; i<=hi; i++) {
		UniChar u;
		CGGlyph g;
		CGRect r;

		u = mapUnicode(i);
		CGFontGetGlyphsForUnichars(font, &u, &g, 1);
		if(g == 0 || !CGFontGetGlyphBBoxes(font, &g, 1, &r))
			continue;

		r.size.width += r.origin.x;
		r.size.height += r.origin.y;
		if(first) {
			bbox = r;
			first = 0;
			continue;
		}
		if(bbox.origin.x > r.origin.x)
			bbox.origin.x = r.origin.x;	
		if(bbox.origin.y > r.origin.y)
			bbox.origin.y = r.origin.y;	
		if(bbox.size.width < r.size.width)
			bbox.size.width = r.size.width;
		if(bbox.size.height < r.size.height)
			bbox.size.height = r.size.height;
	}
	
	bbox.size.width -= bbox.origin.x;
	bbox.size.height -= bbox.origin.y;
	return bbox;
}

void
load(XFont *f)
{
	int i, j;
	CGFontRef font;
	CFStringRef s;
	UniChar u[256];
	CGGlyph g[256];
	CGRect bbox;

	if(f->loaded)
		return;
	f->loaded = 1;
	s = c2mac(f->name);
	font = CGFontCreateWithFontName(s);
	CFRelease(s);
	if(font == nil)
		return;
	
	// assume bbox gives latin1 is height/ascent for all
	bbox = subfontbbox(font, 0x00, 0xff);
	f->unit = CGFontGetUnitsPerEm(font);
	f->height = bbox.size.height;
	f->originy = bbox.origin.y;

	// figure out where the letters are
	for(i=0; i<0xffff; i+=0x100) {
		for(j=0; j<0x100; j++) {
			u[j] = mapUnicode(i+j);
			g[j] = 0;
		}
		CGFontGetGlyphsForUnichars(font, u, g, 256);
		for(j=0; j<0x100; j++) {
			if(g[j] != 0) {
				f->range[i>>8] = 1;
				f->nrange++;
				break;
			}
		}
	}
	CFRelease(font);
}

Memsubfont*
mksubfont(char *name, int lo, int hi, int size, int antialias)
{
	CFStringRef s;
	CGColorSpaceRef color;
	CGContextRef ctxt;
	CGFontRef font;
	CGRect bbox;
	Memimage *m, *mc, *m1;
	int x, y, y0;
	int i, unit;
	Fontchar *fc, *fc0;
	Memsubfont *sf;

	s = c2mac(name);
	font = CGFontCreateWithFontName(s);
	CFRelease(s);
	if(font == nil)
		return nil;
	bbox = subfontbbox(font, lo, hi);
	unit = CGFontGetUnitsPerEm(font);
	x = (int)(bbox.size.width * size / unit + 0.99999999);
	y = bbox.size.height * size/unit + 0.99999999;
	y0 = (int)(-bbox.origin.y * size/unit + 0.99999999);
	m = allocmemimage(Rect(0, 0, x*(hi+1-lo)+1, y+1), GREY8);
	if(m == nil)
		return nil;
	mc = allocmemimage(Rect(0, 0, x+1, y+1), GREY8);
	if(mc == nil)
		return nil;
	memfillcolor(m, DBlack);
	memfillcolor(mc, DBlack);
	fc = malloc((hi+2 - lo) * sizeof fc[0]);
	sf = malloc(sizeof *sf);
	if(fc == nil || sf == nil) {
		freememimage(m);
		freememimage(mc);
		free(fc);
		free(sf);
		return nil;
	}
	fc0 = fc;

	color = CGColorSpaceCreateWithName(kCGColorSpaceGenericGray);
	ctxt = CGBitmapContextCreate(byteaddr(mc, mc->r.min), Dx(mc->r), Dy(mc->r), 8,
		mc->width*sizeof(u32int), color, kCGImageAlphaNone);
	CGColorSpaceRelease(color);
	if(ctxt == nil) {
		freememimage(m);
		freememimage(mc);
		free(fc);
		free(sf);
		return nil;
	}

	CGContextSetFont(ctxt, font);
	CGContextSetFontSize(ctxt, size);
	CGContextSetAllowsAntialiasing(ctxt, antialias);
	CGContextSetRGBFillColor(ctxt, 1, 1, 1, 1);
	CGContextSetTextPosition(ctxt, 0, 0);	// XXX

	x = 0;
	for(i=lo; i<=hi; i++, fc++) {
		UniChar u[2];
		CGGlyph g[2];
		CGRect r[2];
		CGPoint p1;
		int n;

		fc->x = x;
		fc->top = 0;
		fc->bottom = Dy(m->r);

		n = 0;
		u[n++] = mapUnicode(i);
		if(0)	// debugging
			u[n++] = '|';
		g[0] = 0;
		CGFontGetGlyphsForUnichars(font, u, g, n);
		if(g[0] == 0 || !CGFontGetGlyphBBoxes(font, g, n, r)) {
		None:
			fc->width = 0;
			fc->left = 0;
			if(i == 0) {
				drawpjw(m, fc, x, (int)(bbox.size.width * size / unit + 0.99999999), y, y - y0);
				x += fc->width;
			}	
			continue;
		}
		memfillcolor(mc, DBlack);
		CGContextSetTextPosition(ctxt, 0, y0);
		CGContextShowGlyphs(ctxt, g, n);
		p1 = CGContextGetTextPosition(ctxt);
		if(p1.x <= 0)
			goto None;
		memimagedraw(m, Rect(x, 0, x + p1.x, y), mc, ZP, memopaque, ZP, S);
		fc->width = p1.x;
		fc->left = 0;
		x += p1.x;
	}
	fc->x = x;

	// round up to 32-bit boundary
	// so that in-memory data is same
	// layout as in-file data.
	if(x == 0)
		x = 1;
	if(y == 0)
		y = 1;
	if(antialias)
		x += -x & 3;
	else
		x += -x & 31;
	m1 = allocmemimage(Rect(0, 0, x, y), antialias ? GREY8 : GREY1);
	memimagedraw(m1, m1->r, m, m->r.min, memopaque, ZP, S);
	freememimage(m);

	sf->name = nil;
	sf->n = hi+1 - lo;
	sf->height = Dy(m1->r);
	sf->ascent = Dy(m1->r) - y0;
	sf->info = fc0;
	sf->bits = m1;
	
	return sf;
}

