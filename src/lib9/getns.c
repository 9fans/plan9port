#include <u.h>
#include <libc.h>
#include <ctype.h>

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
		werrstr("$DISPLAY not set");
		return nil;
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
	if((d->mode&0777) != 0700 || strcmp(d->uid, getuser()) != 0){
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
