/* a few snippets for selections */
#define MODIFY_SELECTION(name, xe) \
	r.xselection.property = xe->property; \
         name = 0x0; \
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
		xunlock(); \
		qlock(&clip.lk); \
		xlock(); \
		XChangeProperty(_x.display, xe->requestor, xe->property, xe->target, \
			8, PropModeReplace, (uchar*)clip.buf, strlen(clip.buf)); \
		qunlock(&clip.lk); \
	}else
