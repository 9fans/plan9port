#include <u.h>
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <libc.h>
#include <ctype.h>

AUTOLIB(X11);

typedef struct Rectangle Rectangle;
struct Rectangle
{
	struct {
		int x;
		int y;
	} min, max;
};
#define Dx(r) ((r).max.x - (r).min.x)
#define Dy(r) ((r).max.y - (r).min.y)

typedef struct Win Win;
struct Win
{
	Window xw;
	int x;
	int y;
	int dx;
	int dy;
	char *idstr;
	char *class;
	char *instance;
	char *name;
	char *iconname;
};

Display *dpy;
Window root;

Win *w;
int nw;

void getinfo(void);
void listwindows(void);
int parsewinsize(char*, Rectangle*, int*, int*, int*);
void shove(char*, char*);

void
usage(void)
{
	fprint(2, "usage: xshove [window rectangle]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	int screen;

	screen = 0;
	ARGBEGIN{
	case 's':
		screen = atoi(EARGF(usage()));
		break;
	default:
		usage();
		break;
	}ARGEND

	dpy = XOpenDisplay("");
	if(dpy == nil)
		sysfatal("open display: %r");

	root = RootWindow(dpy, screen);
	getinfo();

	if(argc == 0){
		listwindows();
		exits(0);
	}
	if(argc != 2)
		usage();
	shove(argv[0], argv[1]);
	exits(0);
}

char*
getproperty(Window w, Atom a)
{
	uchar *p;
	int fmt;
	Atom type;
	ulong n, dummy;

	n = 100;
	p = nil;
	XGetWindowProperty(dpy, w, a, 0, 100L, 0,
		AnyPropertyType, &type, &fmt,
		&n, &dummy, &p);
	if(p == nil || *p == 0)
		return nil;
	return strdup((char*)p);
}

Window
findname(Window w)
{
	int i;
	uint nxwin;
	Window dw1, dw2, *xwin;

	if(getproperty(w, XA_WM_NAME))
		return w;
	if(!XQueryTree(dpy, w, &dw1, &dw2, &xwin, &nxwin))
		return 0;
	for(i=0; i<nxwin; i++)
		if((w = findname(xwin[i])) != 0)
			return w;
	return 0;
}

void
getinfo(void)
{
	int i;
	uint nxwin;
	Window dw1, dw2, *xwin;
	XClassHint class;
	XWindowAttributes attr;

	if(!XQueryTree(dpy, root, &dw1, &dw2, &xwin, &nxwin))
		return;
	w = mallocz(nxwin*sizeof w[0], 1);
	if(w == 0)
		sysfatal("malloc: %r");

	Win *ww = w;
	for(i=0; i<nxwin; i++){
		memset(&attr, 0, sizeof attr);
		xwin[i] = findname(xwin[i]);
		if(xwin[i] == 0)
			continue;
		XGetWindowAttributes(dpy, xwin[i], &attr);
		if(attr.width <= 0 || attr.override_redirect || attr.map_state != IsViewable)
			continue;
		ww->xw = xwin[i];
		char idstr[9];
		snprint(idstr, sizeof(idstr), "%08x", (uint)ww->xw);
		ww->idstr = strdup(idstr);
		ww->x = attr.x;
		ww->y = attr.y;
		ww->dx = attr.width;
		ww->dy = attr.height;
		XTranslateCoordinates(dpy, ww->xw, root, 0, 0, &ww->x, &ww->y, &dw1);
		if(XGetClassHint(dpy, ww->xw, &class)){
			ww->class = strdup(class.res_class);
			ww->instance = strdup(class.res_name);
		}
		ww->iconname = getproperty(ww->xw, XA_WM_ICON_NAME);
		ww->name = getproperty(ww->xw, XA_WM_NAME);
		ww++;
	}
	nw = ww - w;
}

void
listwindows(void)
{
	int i;

	for(i=0; i<nw; i++){
		Win *ww = &w[i];
		char rect[50];
		snprint(rect, sizeof rect, "%d,%d,%d,%d", ww->x, ww->y, ww->x+ww->dx, ww->y+ww->dy);
		print("%08x %-20s %-10s %s\n",
			(uint)ww->xw,
			rect,
			ww->instance,
			ww->class);
	}
}

void
shove(char *name, char *geom)
{
	int i;
	int isdelta, havemin, havesize;
	int old, new;
	Rectangle r;

	if(parsewinsize(geom, &r, &isdelta, &havemin, &havesize) < 0)
		sysfatal("bad window spec: %s", name);

	old = 0;
	new = 1;
	if(isdelta){
		old = 1;
		new = isdelta;
	}
	for(i=0; i<nw; i++){
		Win *ww = &w[i];
		if(ww->instance && strstr(ww->instance, name)
		   || ww->class && strstr(ww->class, name)
		   || ww->idstr && strstr(ww->idstr, name)){
			int value_mask;
			XWindowChanges e;

			memset(&e, 0, sizeof e);
			if(havemin){
				e.x = old*ww->x + new*r.min.x;
				e.y = old*ww->y + new*r.min.y;
			}else{
				e.x = ww->x;
				e.y = ww->y;
			}
			if(havesize){
				e.width = old*ww->dx + new*Dx(r);
				e.height = old*ww->dy + new*Dy(r);
			}else{
				e.width = ww->dx;
				e.height = ww->dy;
			}
			value_mask = CWX | CWY | CWWidth | CWHeight;
			XConfigureWindow(dpy, ww->xw, value_mask, &e);
			XFlush(dpy);
		}
	}
}

int
parsewinsize(char *s, Rectangle *r, int *isdelta, int *havemin, int *havesize)
{
	char c, *os;
	int i, j, k, l;

	os = s;
	if(*s == '-'){
		s++;
		*isdelta = -1;
	}else if(*s == '+'){
		s++;
		*isdelta = 1;
	}else
		*isdelta = 0;
	*havemin = 0;
	*havesize = 0;
	memset(r, 0, sizeof *r);
	if(!isdigit((uchar)*s))
		goto oops;
	i = strtol(s, &s, 0);
	if(*s == 'x'){
		s++;
		if(!isdigit((uchar)*s))
			goto oops;
		j = strtol(s, &s, 0);
		r->max.x = i;
		r->max.y = j;
		*havesize = 1;
		if(*s == 0)
			return 0;
		if(*s != '@')
			goto oops;

		s++;
		if(!isdigit((uchar)*s))
			goto oops;
		i = strtol(s, &s, 0);
		if(*s != ',' && *s != ' ')
			goto oops;
		s++;
		if(!isdigit((uchar)*s))
			goto oops;
		j = strtol(s, &s, 0);
		if(*s != 0)
			goto oops;
		r->min.x += i;
		r->max.x += i;
		r->min.y += j;
		r->max.y += j;
		*havesize = 1;
		*havemin = 1;
		return 0;
	}

	c = *s;
	if(c != ' ' && c != ',')
		goto oops;
	s++;
	if(!isdigit((uchar)*s))
		goto oops;
	j = strtol(s, &s, 0);
	if(*s == 0){
		r->min.x = i;
		r->min.y = j;
		*havemin = 1;
		return 0;
	}
	if(*s != c)
		goto oops;
	s++;
	if(!isdigit((uchar)*s))
		goto oops;
	k = strtol(s, &s, 0);
	if(*s != c)
		goto oops;
	s++;
	if(!isdigit((uchar)*s))
		goto oops;
	l = strtol(s, &s, 0);
	if(*s != 0)
		goto oops;
	r->min.x = i;
	r->min.y = j;
	r->max.x = k;
	r->max.y = l;
	*havemin = 1;
	*havesize = 1;
	return 0;

oops:
	werrstr("bad syntax in window size '%s'", os);
	return -1;
}
