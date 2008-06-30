#include <u.h>
#include <libc.h>
#include <ctype.h>

static int
isme(char *uid)
{
	int n;
	char *p;

	n = strtol(uid, &p, 10);
	if(*p == 0 && p > uid)
		return n == getuid();
	return strcmp(getuser(), uid) == 0;
}
/*
 * Absent other hints, it works reasonably well to use
 * the X11 display name as the name space identifier.
 * This is how sam's B has worked since the early days.
 * Since most programs using name spaces are also using X,
 * this still seems reasonable.  Terminal-only sessions
 * can set $NAMESPACE.
 */
static char*
nsfromdisplay(void)
{
	int fd;
	Dir *d;
	char *disp, *p;

	if((disp = getenv("DISPLAY")) == nil){
#ifdef __APPLE__
		// Might be running native GUI on OS X.
		disp = strdup(":0.0");
		if(disp == nil)
			return nil;
#else
		werrstr("$DISPLAY not set");
		return nil;
#endif
	}

	/* canonicalize: xxx:0.0 => xxx:0 */
	p = strrchr(disp, ':');
	if(p){
		p++;
		while(isdigit((uchar)*p))
			p++;
		if(strcmp(p, ".0") == 0)
			*p = 0;
	}
	
	/* turn /tmp/launch/:0 into _tmp_launch_:0 (OS X 10.5) */
	for(p=disp; *p; p++)
		if(*p == '/')
			*p = '_';

	p = smprint("/tmp/ns.%s.%s", getuser(), disp);
	free(disp);
	if(p == nil){
		werrstr("out of memory");
		return p;
	}
	if((fd=create(p, OREAD, DMDIR|0700)) >= 0){
		close(fd);
		return p;
	}
	if((d = dirstat(p)) == nil){
		free(d);
		werrstr("stat %s: %r", p);
		free(p);
		return nil;
	}
	if((d->mode&0777) != 0700 || !isme(d->uid)){
		werrstr("bad name space dir %s", p);
		free(p);
		free(d);
		return nil;
	}
	free(d);
	return p;
}

char*
getns(void)
{
	char *ns;

	ns = getenv("NAMESPACE");
	if(ns == nil)
		ns = nsfromdisplay();
	if(ns == nil){
		werrstr("$NAMESPACE not set, %r");
		return nil;
	}
	return ns;
}
