/* I found such code in snarfer and in devdraw, so I moved it into this
    definition: MODIFY_SELECTION, that works for both functions
    x11-screen.c:/^_xselect/ and ../snarfer/snarfer.c:/^xselectionrequest/
    This removes annoying "cannot handle selection requests" for types
    'text/plain;charset=UTF-8' and 'text/uri-list', both now compared, ignoring
    case. */
#define MODIFY_SELECTION(xe) do { \
	Atom a[4]; \
         char* name; \
if(0) fprint(2, "xselect target=%d requestor=%d property=%d selection=%d\n", \
	xe->target, xe->requestor, xe->property, xe->selection); \
	r.xselection.property = xe->property; \
	memset(&name, 0, sizeof name); \
	if(xe->target == _x.targets){ \
		a[0] = _x.utf8string; \
		a[1] = XA_STRING; \
		a[2] = _x.text; \
		a[3] = _x.compoundtext; \
		XChangeProperty(_x.display, xe->requestor, xe->property, XA_ATOM, \
			32, PropModeReplace, (uchar*)a, nelem(a)); \
	}else if(xe->target == XA_STRING \
	|| xe->target == _x.utf8string \
	|| xe->target == _x.text \
	|| xe->target == _x.compoundtext \
	|| ((name = XGetAtomName(_x.display, xe->target)) \
           && ((cistrcmp(name, "text/plain;charset=UTF-8") == 0) \
              || (cistrcmp(name, "text/uri-list") == 0)))){ \
		/* text/plain;charset=UTF-8 seems nonstandard but is used by Synergy */ \
		/* if the target is STRING we're supposed to reply with Latin1 XXX */ \
		qlock(&clip.lk); \
		XChangeProperty(_x.display, xe->requestor, xe->property, xe->target, \
			8, PropModeReplace, (uchar*)clip.buf, strlen(clip.buf)); \
		qunlock(&clip.lk); \
	}else { \
		if(strcmp(name, "TIMESTAMP") != 0) \
		        fprint(2, "%s: cannot handle selection request for '%s' (%d)\n", \
		            argv0, name, (int)xe->target); \
		r.xselection.property = None; \
	} \
         if (name) XFree(name); \
} \
while (0)
