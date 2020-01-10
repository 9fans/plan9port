/*
 * Original code posted to comp.sources.x
 * Modifications by Russ Cox <rsc@swtch.com>.
 */

/*
Path: uunet!wyse!mikew
From: mikew@wyse.wyse.com (Mike Wexler)
Newsgroups: comp.sources.x
Subject: v02i056:  subroutine to print events in human readable form, Part01/01
Message-ID: <1935@wyse.wyse.com>
Date: 22 Dec 88 19:28:25 GMT
Organization: Wyse Technology, San Jose
Lines: 1093
Approved: mikew@wyse.com

Submitted-by: richsun!darkstar!ken
Posting-number: Volume 2, Issue 56
Archive-name: showevent/part01


There are times during debugging when it would be real useful to be able to
print the fields of an event in a human readable form.  Too many times I found
myself scrounging around in section 8 of the Xlib manual looking for the valid
fields for the events I wanted to see, then adding printf's to display the
numeric values of the fields, and then scanning through X.h trying to decode
the cryptic detail and state fields.  After playing with xev, I decided to
write a couple of standard functions that I could keep in a library and call
on whenever I needed a little debugging verbosity.  The first function,
GetType(), is useful for returning the string representation of the type of
an event.  The second function, ShowEvent(), is used to display all the fields
of an event in a readable format.  The functions are not complicated, in fact,
they are mind-numbingly boring - but that's just the point nobody wants to
spend the time writing functions like this, they just want to have them when
they need them.

A simple, sample program is included which does little else but to demonstrate
the use of these two functions.  These functions have saved me many an hour
during debugging and I hope you find some benefit to these.  If you have any
comments, suggestions, improvements, or if you find any blithering errors you
can get it touch with me at the following location:

			ken@richsun.UUCP
*/

#include <stdio.h>
#include <X11/Intrinsic.h>
#include <X11/Xproto.h>
#include "printevent.h"

static char* sep = " ";

/******************************************************************************/
/**** Miscellaneous routines to convert values to their string equivalents ****/
/******************************************************************************/

/* Returns the string equivalent of a boolean parameter */
static char*
TorF(int bool)
{
    switch (bool) {
    case True:
	return ("True");

    case False:
	return ("False");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of a property notify state */
static char*
PropertyState(int state)
{
    switch (state) {
    case PropertyNewValue:
	return ("PropertyNewValue");

    case PropertyDelete:
	return ("PropertyDelete");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of a visibility notify state */
static char*
VisibilityState(int state)
{
    switch (state) {
    case VisibilityUnobscured:
	return ("VisibilityUnobscured");

    case VisibilityPartiallyObscured:
	return ("VisibilityPartiallyObscured");

    case VisibilityFullyObscured:
	return ("VisibilityFullyObscured");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of a timestamp */
static char*
ServerTime(Time time)
{
    unsigned long msec;
    unsigned long sec;
    unsigned long min;
    unsigned long hr;
    unsigned long day;
    static char buffer[32];

    msec = time % 1000;
    time /= 1000;
    sec = time % 60;
    time /= 60;
    min = time % 60;
    time /= 60;
    hr = time % 24;
    time /= 24;
    day = time;

if(0)
    sprintf(buffer, "%lu day%s %02lu:%02lu:%02lu.%03lu",
      day, day == 1 ? "" : "(s)", hr, min, sec, msec);

    sprintf(buffer, "%lud%luh%lum%lu.%03lds", day, hr, min, sec, msec);
    return (buffer);
}

/* Simple structure to ease the interpretation of masks */
typedef struct MaskType MaskType;
struct MaskType
{
    unsigned int value;
    char *string;
};

/* Returns the string equivalent of a mask of buttons and/or modifier keys */
static char*
ButtonAndOrModifierState(unsigned int state)
{
    static char buffer[256];
    static MaskType masks[] = {
	{Button1Mask, "Button1Mask"},
	{Button2Mask, "Button2Mask"},
	{Button3Mask, "Button3Mask"},
	{Button4Mask, "Button4Mask"},
	{Button5Mask, "Button5Mask"},
	{ShiftMask, "ShiftMask"},
	{LockMask, "LockMask"},
	{ControlMask, "ControlMask"},
	{Mod1Mask, "Mod1Mask"},
	{Mod2Mask, "Mod2Mask"},
	{Mod3Mask, "Mod3Mask"},
	{Mod4Mask, "Mod4Mask"},
	{Mod5Mask, "Mod5Mask"},
    };
    int num_masks = sizeof(masks) / sizeof(MaskType);
    int i;
    Boolean first = True;

    buffer[0] = 0;

    for (i = 0; i < num_masks; i++)
	if (state & masks[i].value)
	    if (first) {
		first = False;
		strcpy(buffer, masks[i].string);
	    } else {
		strcat(buffer, " | ");
		strcat(buffer, masks[i].string);
	    }
    return (buffer);
}

/* Returns the string equivalent of a mask of configure window values */
static char*
ConfigureValueMask(unsigned int valuemask)
{
    static char buffer[256];
    static MaskType masks[] = {
	{CWX, "CWX"},
	{CWY, "CWY"},
	{CWWidth, "CWWidth"},
	{CWHeight, "CWHeight"},
	{CWBorderWidth, "CWBorderWidth"},
	{CWSibling, "CWSibling"},
	{CWStackMode, "CWStackMode"},
    };
    int num_masks = sizeof(masks) / sizeof(MaskType);
    int i;
    Boolean first = True;

    buffer[0] = 0;

    for (i = 0; i < num_masks; i++)
	if (valuemask & masks[i].value)
	    if (first) {
		first = False;
		strcpy(buffer, masks[i].string);
	    } else {
		strcat(buffer, " | ");
		strcat(buffer, masks[i].string);
	    }

    return (buffer);
}

/* Returns the string equivalent of a motion hint */
static char*
IsHint(char is_hint)
{
    switch (is_hint) {
    case NotifyNormal:
	return ("NotifyNormal");

    case NotifyHint:
	return ("NotifyHint");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of an id or the value "None" */
static char*
MaybeNone(int value)
{
    static char buffer[16];

    if (value == None)
	return ("None");
    else {
	sprintf(buffer, "0x%x", value);
	return (buffer);
    }
}

/* Returns the string equivalent of a colormap state */
static char*
ColormapState(int state)
{
    switch (state) {
    case ColormapInstalled:
	return ("ColormapInstalled");

    case ColormapUninstalled:
	return ("ColormapUninstalled");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of a crossing detail */
static char*
CrossingDetail(int detail)
{
    switch (detail) {
    case NotifyAncestor:
	return ("NotifyAncestor");

    case NotifyInferior:
	return ("NotifyInferior");

    case NotifyVirtual:
	return ("NotifyVirtual");

    case NotifyNonlinear:
	return ("NotifyNonlinear");

    case NotifyNonlinearVirtual:
	return ("NotifyNonlinearVirtual");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of a focus change detail */
static char*
FocusChangeDetail(int detail)
{
    switch (detail) {
    case NotifyAncestor:
	return ("NotifyAncestor");

    case NotifyInferior:
	return ("NotifyInferior");

    case NotifyVirtual:
	return ("NotifyVirtual");

    case NotifyNonlinear:
	return ("NotifyNonlinear");

    case NotifyNonlinearVirtual:
	return ("NotifyNonlinearVirtual");

    case NotifyPointer:
	return ("NotifyPointer");

    case NotifyPointerRoot:
	return ("NotifyPointerRoot");

    case NotifyDetailNone:
	return ("NotifyDetailNone");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of a configure detail */
static char*
ConfigureDetail(int detail)
{
    switch (detail) {
    case Above:
	return ("Above");

    case Below:
	return ("Below");

    case TopIf:
	return ("TopIf");

    case BottomIf:
	return ("BottomIf");

    case Opposite:
	return ("Opposite");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of a grab mode */
static char*
GrabMode(int mode)
{
    switch (mode) {
    case NotifyNormal:
	return ("NotifyNormal");

    case NotifyGrab:
	return ("NotifyGrab");

    case NotifyUngrab:
	return ("NotifyUngrab");

    case NotifyWhileGrabbed:
	return ("NotifyWhileGrabbed");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of a mapping request */
static char*
MappingRequest(int request)
{
    switch (request) {
    case MappingModifier:
	return ("MappingModifier");

    case MappingKeyboard:
	return ("MappingKeyboard");

    case MappingPointer:
	return ("MappingPointer");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of a stacking order place */
static char*
Place(int place)
{
    switch (place) {
    case PlaceOnTop:
	return ("PlaceOnTop");

    case PlaceOnBottom:
	return ("PlaceOnBottom");

    default:
	return ("?");
    }
}

/* Returns the string equivalent of a major code */
static char*
MajorCode(int code)
{
    static char buffer[32];

    switch (code) {
    case X_CopyArea:
	return ("X_CopyArea");

    case X_CopyPlane:
	return ("X_CopyPlane");

    default:
	sprintf(buffer, "0x%x", code);
	return (buffer);
    }
}

/* Returns the string equivalent the keycode contained in the key event */
static char*
Keycode(XKeyEvent *ev)
{
    static char buffer[256];
    KeySym keysym_str;
    char *keysym_name;
    char string[256];

    XLookupString(ev, string, 64, &keysym_str, NULL);

    if (keysym_str == NoSymbol)
	keysym_name = "NoSymbol";
    else if (!(keysym_name = XKeysymToString(keysym_str)))
	keysym_name = "(no name)";
    sprintf(buffer, "%u (keysym 0x%x \"%s\")",
      (int)ev->keycode, (int)keysym_str, keysym_name);
    return (buffer);
}

/* Returns the string equivalent of an atom or "None"*/
static char*
AtomName(Display *dpy, Atom atom)
{
    static char buffer[256];
    char *atom_name;

    if (atom == None)
	return ("None");

    atom_name = XGetAtomName(dpy, atom);
    strncpy(buffer, atom_name, 256);
    XFree(atom_name);
    return (buffer);
}

/******************************************************************************/
/**** Routines to print out readable values for the field of various events ***/
/******************************************************************************/

static void
VerbMotion(XMotionEvent *ev)
{
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("root=0x%x%s", (int)ev->root, sep);
    printf("subwindow=0x%x%s", (int)ev->subwindow, sep);
    printf("time=%s%s", ServerTime(ev->time), sep);
    printf("x=%d y=%d%s", ev->x, ev->y, sep);
    printf("x_root=%d y_root=%d%s", ev->x_root, ev->y_root, sep);
    printf("state=%s%s", ButtonAndOrModifierState(ev->state), sep);
    printf("is_hint=%s%s", IsHint(ev->is_hint), sep);
    printf("same_screen=%s\n", TorF(ev->same_screen));
}

static void
VerbButton(XButtonEvent *ev)
{
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("root=0x%x%s", (int)ev->root, sep);
    printf("subwindow=0x%x%s", (int)ev->subwindow, sep);
    printf("time=%s%s", ServerTime(ev->time), sep);
    printf("x=%d y=%d%s", ev->x, ev->y, sep);
    printf("x_root=%d y_root=%d%s", ev->x_root, ev->y_root, sep);
    printf("state=%s%s", ButtonAndOrModifierState(ev->state), sep);
    printf("button=%s%s", ButtonAndOrModifierState(ev->button), sep);
    printf("same_screen=%s\n", TorF(ev->same_screen));
}

static void
VerbColormap(XColormapEvent *ev)
{
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("colormap=%s%s", MaybeNone(ev->colormap), sep);
    printf("new=%s%s", TorF(ev->new), sep);
    printf("state=%s\n", ColormapState(ev->state));
}

static void
VerbCrossing(XCrossingEvent *ev)
{
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("root=0x%x%s", (int)ev->root, sep);
    printf("subwindow=0x%x%s", (int)ev->subwindow, sep);
    printf("time=%s%s", ServerTime(ev->time), sep);
    printf("x=%d y=%d%s", ev->x, ev->y, sep);
    printf("x_root=%d y_root=%d%s", ev->x_root, ev->y_root, sep);
    printf("mode=%s%s", GrabMode(ev->mode), sep);
    printf("detail=%s%s", CrossingDetail(ev->detail), sep);
    printf("same_screen=%s%s", TorF(ev->same_screen), sep);
    printf("focus=%s%s", TorF(ev->focus), sep);
    printf("state=%s\n", ButtonAndOrModifierState(ev->state));
}

static void
VerbExpose(XExposeEvent *ev)
{
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("x=%d y=%d%s", ev->x, ev->y, sep);
    printf("width=%d height=%d%s", ev->width, ev->height, sep);
    printf("count=%d\n", ev->count);
}

static void
VerbGraphicsExpose(XGraphicsExposeEvent *ev)
{
    printf("drawable=0x%x%s", (int)ev->drawable, sep);
    printf("x=%d y=%d%s", ev->x, ev->y, sep);
    printf("width=%d height=%d%s", ev->width, ev->height, sep);
    printf("major_code=%s%s", MajorCode(ev->major_code), sep);
    printf("minor_code=%d\n", ev->minor_code);
}

static void
VerbNoExpose(XNoExposeEvent *ev)
{
    printf("drawable=0x%x%s", (int)ev->drawable, sep);
    printf("major_code=%s%s", MajorCode(ev->major_code), sep);
    printf("minor_code=%d\n", ev->minor_code);
}

static void
VerbFocus(XFocusChangeEvent *ev)
{
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("mode=%s%s", GrabMode(ev->mode), sep);
    printf("detail=%s\n", FocusChangeDetail(ev->detail));
}

static void
VerbKeymap(XKeymapEvent *ev)
{
    int i;

    printf("window=0x%x%s", (int)ev->window, sep);
    printf("key_vector=");
    for (i = 0; i < 32; i++)
	printf("%02x", ev->key_vector[i]);
    printf("\n");
}

static void
VerbKey(XKeyEvent *ev)
{
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("root=0x%x%s", (int)ev->root, sep);
    if(ev->subwindow)
        printf("subwindow=0x%x%s", (int)ev->subwindow, sep);
    printf("time=%s%s", ServerTime(ev->time), sep);
    printf("[%d,%d]%s", ev->x, ev->y, sep);
    printf("root=[%d,%d]%s", ev->x_root, ev->y_root, sep);
    if(ev->state)
        printf("state=%s%s", ButtonAndOrModifierState(ev->state), sep);
    printf("keycode=%s%s", Keycode(ev), sep);
    if(!ev->same_screen)
        printf("!same_screen", TorF(ev->same_screen));
    printf("\n");
    return;

    printf("window=0x%x%s", (int)ev->window, sep);
    printf("root=0x%x%s", (int)ev->root, sep);
    printf("subwindow=0x%x%s", (int)ev->subwindow, sep);
    printf("time=%s%s", ServerTime(ev->time), sep);
    printf("x=%d y=%d%s", ev->x, ev->y, sep);
    printf("x_root=%d y_root=%d%s", ev->x_root, ev->y_root, sep);
    printf("state=%s%s", ButtonAndOrModifierState(ev->state), sep);
    printf("keycode=%s%s", Keycode(ev), sep);
    printf("same_screen=%s\n", TorF(ev->same_screen));
}

static void
VerbProperty(XPropertyEvent *ev)
{
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("atom=%s%s", AtomName(ev->display, ev->atom), sep);
    printf("time=%s%s", ServerTime(ev->time), sep);
    printf("state=%s\n", PropertyState(ev->state));
}

static void
VerbResizeRequest(XResizeRequestEvent *ev)
{
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("width=%d height=%d\n", ev->width, ev->height);
}

static void
VerbCirculate(XCirculateEvent *ev)
{
    printf("event=0x%x%s", (int)ev->event, sep);
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("place=%s\n", Place(ev->place));
}

static void
VerbConfigure(XConfigureEvent *ev)
{
    printf("event=0x%x%s", (int)ev->event, sep);
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("x=%d y=%d%s", ev->x, ev->y, sep);
    printf("width=%d height=%d%s", ev->width, ev->height, sep);
    printf("border_width=%d%s", ev->border_width, sep);
    printf("above=%s%s", MaybeNone(ev->above), sep);
    printf("override_redirect=%s\n", TorF(ev->override_redirect));
}

static void
VerbCreateWindow(XCreateWindowEvent *ev)
{
    printf("parent=0x%x%s", (int)ev->parent, sep);
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("x=%d y=%d%s", ev->x, ev->y, sep);
    printf("width=%d height=%d%s", ev->width, ev->height, sep);
    printf("border_width=%d%s", ev->border_width, sep);
    printf("override_redirect=%s\n", TorF(ev->override_redirect));
}

static void
VerbDestroyWindow(XDestroyWindowEvent *ev)
{
    printf("event=0x%x%s", (int)ev->event, sep);
    printf("window=0x%x\n", (int)ev->window);
}

static void
VerbGravity(XGravityEvent *ev)
{
    printf("event=0x%x%s", (int)ev->event, sep);
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("x=%d y=%d\n", ev->x, ev->y);
}

static void
VerbMap(XMapEvent *ev)
{
    printf("event=0x%x%s", (int)ev->event, sep);
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("override_redirect=%s\n", TorF(ev->override_redirect));
}

static void
VerbReparent(XReparentEvent *ev)
{
    printf("event=0x%x%s", (int)ev->event, sep);
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("parent=0x%x%s", (int)ev->parent, sep);
    printf("x=%d y=%d%s", ev->x, ev->y, sep);
    printf("override_redirect=%s\n", TorF(ev->override_redirect));
}

static void
VerbUnmap(XUnmapEvent *ev)
{
    printf("event=0x%x%s", (int)ev->event, sep);
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("from_configure=%s\n", TorF(ev->from_configure));
}

static void
VerbCirculateRequest(XCirculateRequestEvent *ev)
{
    printf("parent=0x%x%s", (int)ev->parent, sep);
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("place=%s\n", Place(ev->place));
}

static void
VerbConfigureRequest(XConfigureRequestEvent *ev)
{
    printf("parent=0x%x%s", (int)ev->parent, sep);
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("x=%d y=%d%s", ev->x, ev->y, sep);
    printf("width=%d height=%d%s", ev->width, ev->height, sep);
    printf("border_width=%d%s", ev->border_width, sep);
    printf("above=%s%s", MaybeNone(ev->above), sep);
    printf("detail=%s%s", ConfigureDetail(ev->detail), sep);
    printf("value_mask=%s\n", ConfigureValueMask(ev->value_mask));
}

static void
VerbMapRequest(XMapRequestEvent *ev)
{
    printf("parent=0x%x%s", (int)ev->parent, sep);
    printf("window=0x%x\n", (int)ev->window);
}

static void
VerbClient(XClientMessageEvent *ev)
{
    int i;

    printf("window=0x%x%s", (int)ev->window, sep);
    printf("message_type=%s%s", AtomName(ev->display, ev->message_type), sep);
    printf("format=%d\n", ev->format);
    printf("data (shown as longs)=");
    for (i = 0; i < 5; i++)
	printf(" 0x%08lx", ev->data.l[i]);
    printf("\n");
}

static void
VerbMapping(XMappingEvent *ev)
{
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("request=%s%s", MappingRequest(ev->request), sep);
    printf("first_keycode=0x%x%s", ev->first_keycode, sep);
    printf("count=0x%x\n", ev->count);
}

static void
VerbSelectionClear(XSelectionClearEvent *ev)
{
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("selection=%s%s", AtomName(ev->display, ev->selection), sep);
    printf("time=%s\n", ServerTime(ev->time));
}

static void
VerbSelection(XSelectionEvent *ev)
{
    printf("requestor=0x%x%s", (int)ev->requestor, sep);
    printf("selection=%s%s", AtomName(ev->display, ev->selection), sep);
    printf("target=%s%s", AtomName(ev->display, ev->target), sep);
    printf("property=%s%s", AtomName(ev->display, ev->property), sep);
    printf("time=%s\n", ServerTime(ev->time));
}

static void
VerbSelectionRequest(XSelectionRequestEvent *ev)
{
    printf("owner=0x%x%s", (int)ev->owner, sep);
    printf("requestor=0x%x%s", (int)ev->requestor, sep);
    printf("selection=%s%s", AtomName(ev->display, ev->selection), sep);
    printf("target=%s%s", AtomName(ev->display, ev->target), sep);
    printf("property=%s%s", AtomName(ev->display, ev->property), sep);
    printf("time=%s\n", ServerTime(ev->time));
}

static void
VerbVisibility(XVisibilityEvent *ev)
{
    printf("window=0x%x%s", (int)ev->window, sep);
    printf("state=%s\n", VisibilityState(ev->state));
}

/******************************************************************************/
/************ Return the string representation for type of an event ***********/
/******************************************************************************/

char *eventtype(XEvent *ev)
{
    static char buffer[20];

    switch (ev->type) {
    case KeyPress:
	return ("KeyPress");
    case KeyRelease:
	return ("KeyRelease");
    case ButtonPress:
	return ("ButtonPress");
    case ButtonRelease:
	return ("ButtonRelease");
    case MotionNotify:
	return ("MotionNotify");
    case EnterNotify:
	return ("EnterNotify");
    case LeaveNotify:
	return ("LeaveNotify");
    case FocusIn:
	return ("FocusIn");
    case FocusOut:
	return ("FocusOut");
    case KeymapNotify:
	return ("KeymapNotify");
    case Expose:
	return ("Expose");
    case GraphicsExpose:
	return ("GraphicsExpose");
    case NoExpose:
	return ("NoExpose");
    case VisibilityNotify:
	return ("VisibilityNotify");
    case CreateNotify:
	return ("CreateNotify");
    case DestroyNotify:
	return ("DestroyNotify");
    case UnmapNotify:
	return ("UnmapNotify");
    case MapNotify:
	return ("MapNotify");
    case MapRequest:
	return ("MapRequest");
    case ReparentNotify:
	return ("ReparentNotify");
    case ConfigureNotify:
	return ("ConfigureNotify");
    case ConfigureRequest:
	return ("ConfigureRequest");
    case GravityNotify:
	return ("GravityNotify");
    case ResizeRequest:
	return ("ResizeRequest");
    case CirculateNotify:
	return ("CirculateNotify");
    case CirculateRequest:
	return ("CirculateRequest");
    case PropertyNotify:
	return ("PropertyNotify");
    case SelectionClear:
	return ("SelectionClear");
    case SelectionRequest:
	return ("SelectionRequest");
    case SelectionNotify:
	return ("SelectionNotify");
    case ColormapNotify:
	return ("ColormapNotify");
    case ClientMessage:
	return ("ClientMessage");
    case MappingNotify:
	return ("MappingNotify");
    }
    sprintf(buffer, "%d", ev->type);
    return buffer;
}

/******************************************************************************/
/**************** Print the values of all fields for any event ****************/
/******************************************************************************/

void printevent(XEvent *e)
{
    XAnyEvent *ev = (void*)e;

    printf("%3ld %-20s ", ev->serial, eventtype(e));
    if(ev->send_event)
        printf("(sendevent) ");
    if(0){
        printf("type=%s%s", eventtype(e), sep);
        printf("serial=%lu%s", ev->serial, sep);
        printf("send_event=%s%s", TorF(ev->send_event), sep);
        printf("display=0x%p%s", ev->display, sep);
    }

    switch (ev->type) {
    case MotionNotify:
	VerbMotion((void*)ev);
	break;

    case ButtonPress:
    case ButtonRelease:
	VerbButton((void*)ev);
	break;

    case ColormapNotify:
	VerbColormap((void*)ev);
	break;

    case EnterNotify:
    case LeaveNotify:
	VerbCrossing((void*)ev);
	break;

    case Expose:
	VerbExpose((void*)ev);
	break;

    case GraphicsExpose:
	VerbGraphicsExpose((void*)ev);
	break;

    case NoExpose:
	VerbNoExpose((void*)ev);
	break;

    case FocusIn:
    case FocusOut:
	VerbFocus((void*)ev);
	break;

    case KeymapNotify:
	VerbKeymap((void*)ev);
	break;

    case KeyPress:
    case KeyRelease:
	VerbKey((void*)ev);
	break;

    case PropertyNotify:
	VerbProperty((void*)ev);
	break;

    case ResizeRequest:
	VerbResizeRequest((void*)ev);
	break;

    case CirculateNotify:
	VerbCirculate((void*)ev);
	break;

    case ConfigureNotify:
	VerbConfigure((void*)ev);
	break;

    case CreateNotify:
	VerbCreateWindow((void*)ev);
	break;

    case DestroyNotify:
	VerbDestroyWindow((void*)ev);
	break;

    case GravityNotify:
	VerbGravity((void*)ev);
	break;

    case MapNotify:
	VerbMap((void*)ev);
	break;

    case ReparentNotify:
	VerbReparent((void*)ev);
	break;

    case UnmapNotify:
	VerbUnmap((void*)ev);
	break;

    case CirculateRequest:
	VerbCirculateRequest((void*)ev);
	break;

    case ConfigureRequest:
	VerbConfigureRequest((void*)ev);
	break;

    case MapRequest:
	VerbMapRequest((void*)ev);
	break;

    case ClientMessage:
	VerbClient((void*)ev);
	break;

    case MappingNotify:
	VerbMapping((void*)ev);
	break;

    case SelectionClear:
	VerbSelectionClear((void*)ev);
	break;

    case SelectionNotify:
	VerbSelection((void*)ev);
	break;

    case SelectionRequest:
	VerbSelectionRequest((void*)ev);
	break;

    case VisibilityNotify:
	VerbVisibility((void*)ev);
	break;
    }
}
