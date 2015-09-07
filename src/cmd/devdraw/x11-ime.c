#include <u.h>
#include <utf.h>
#include <libc.h>
#include <draw.h>
#include <ime.h>
#include "x11-inc.h"
#include "x11-ime.h"

#if 1
#define Display	XDisplay
#define Window	XWindow
#endif

static XIC ic;
static XIM im;
static XFontSet fs;
static Display *disp;

#define XIMVERBOSEMSG	0

static
XFontSet
createfontset(Display *d)
{
	XFontSet tmp;
	char **missing;
	int nmissing;
	int i;
	const char* basefont = "-misc-fixed-*-*-*-*-*-*-*-*-*-*-*-*";

	tmp = XCreateFontSet(d, basefont, &missing, &nmissing, NULL);
	if(tmp == NULL)
		return NULL;
#if XIMVERBOSEMSG
	for(i = 0; i < nmissing; ++i)
		fprint(2, "The font(%s) is missing for the current locale\n",
			missing[i]);
#else
	USED(i);
#endif
	if(nmissing > 0)
		XFreeStringList(missing);
	return tmp;
}

static
void
freefontset(Display *d, XFontSet set)
{
	XFreeFontSet(d, set);
}

static
int
getximstyle(XIM im, XIMStyle *out)
{
	XIMStyles *s;
	unsigned short i;
	XIMStyle ss;
	int found = 0;

	if(XGetIMValues(im, XNQueryInputStyle, &s, NULL) != NULL){
		fprint(2, "XGetIMValues failed\n");
		return -1;
	}
	for(i = 0; i < s->count_styles; ++i){
		ss = s->supported_styles[i];
		if((ss & XIMPreeditPosition) == 0)
			continue;
		else if(ss & (XIMStatusNothing | XIMStatusNone)){
			*out = ss;
			found = 1;
			break;
		}
	}
	free(s);
	return found ? 0 : -1;			
}

static
XIC
createic(Display *d, XIM im, Window w, XFontSet f)
{
	XPoint pt;
	XIMStyle s;

	if(getximstyle(im, &s) != 0)
		return NULL;
	pt.x = pt.y = 0;
	return XCreateIC(im, XNInputStyle,
		s,
		XNClientWindow, w,
		XNFocusWindow, w,
		XNPreeditAttributes,
		XVaCreateNestedList(0,
			XNSpotLocation, &pt,
			XNFontSet, fs,
			NULL),
		NULL);
}

static
int
addeventfilters(Display *d, Window w, XIC ic)
{
	long mask;
	long t;
	XWindowAttributes att = { 0 };
	if(XGetWindowAttributes(d, w, &att) == 0){
		fprint(2, "XGetWindowAttributes failed\n");
		return -1;
	}
	mask = att.your_event_mask;
	if(XGetICValues(ic, XNFilterEvents, &t, NULL) != NULL){
		fprint(2, "XGetICValues failed\n");
		return -1;
	}
	mask |= t;
	XSelectInput(d, w, mask);
	return 0;
}

int
imeinit(Display *d, Window w)
{
	if(XSetLocaleModifiers("") == NULL)
		fprint(2, "XSetLocaleModifiers failed\n");
	else if ((fs = createfontset(d)) == NULL)
		fprint(2, "createfontset failed\n");
	else if ((im = XOpenIM(d, NULL, NULL, NULL)) == NULL)
		fprint(2, "XOpenIM failed\n");
	else if ((ic = createic(d, im, w, fs)) == NULL)
		fprint(2, "XCreateIC failed\n");
	else if(addeventfilters(d, w, ic) != 0)
		fprint(2, "addeventfilters failed\n");
	else {
		disp = d;
		return 0;
	}
	if(ic != NULL){
		XDestroyIC(ic);
		ic = NULL;
	}
	if(im != NULL){
		XCloseIM(im);
		im = NULL;
	}
	if(fs != NULL){
		freefontset(d, fs);
		fs = NULL;
	}
	return -1;
}

struct Imerunestr
{
	Rune *r;
	size_t size;
};

static struct Imerunestr runebuf;

void
imecleanup()
{
	if(ic != NULL){
		XDestroyIC(ic);
		ic = NULL;
	}
	if(im != NULL){
		XCloseIM(im);
		im = NULL;
	}
	if(fs != NULL){
		freefontset(disp, fs);
		fs = NULL;
		disp = NULL;
	}
	if(runebuf.r != NULL){
		free(runebuf.r);
		memset(&runebuf, 0, sizeof runebuf);
	}
}

#define AUNIT	32
int
imerunestr(char *p, struct Imerunestr *r)
{
	Rune *q;
	int req;
	size_t size = r->size;

	req = utflen(p) + 1;
	while(req > size)
		size += AUNIT;
	if(r->size != size){
		q = realloc(r->r, size * sizeof*q);
		if(q == 0)
			return -1;
		r->r = q;
	}
	r->size = size;
	q = r->r;
	while(*p)
		p += chartorune(q++, p);
	*q = 0;
	return 0;
}

Rune*
imestr(XKeyPressedEvent *e)
{
	char t[128];
	Status s;
	int l;
	char *p;
	char *bp = NULL;
	int r;

	if(ic == NULL)
		return NULL;
	l = XmbLookupString(ic, e, t, sizeof t - 1, NULL, &s);
	if (s != XLookupChars && s != XBufferOverflow)
		return NULL;
	else if (s == XBufferOverflow) {
		bp = mallocz(l + 1, 0);
		l = XmbLookupString(ic, e, bp, l, NULL, &s);
		bp[l] = '\0';
		p = bp;
	} else {
		t[l] = '\0';
		p = t;
	}
	r = imerunestr(p, &runebuf);
	if (bp)
		free(bp);
	return (r == 0) ? runebuf.r : NULL;
}

int
imesetspot(int x, int y)
{
	XPoint pt;
	char *p;

	if(ic == NULL)
		return -1;
	pt.x = x;
	pt.y = y;
	p = XSetICValues(ic, XNPreeditAttributes,
		XVaCreateNestedList(0, XNSpotLocation, &pt, NULL),
		NULL);
	if (p == NULL)
		return 0;
	else
		return -1;
}
