#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ndb.h>
#include <ip.h>
#include <thread.h>
#include "dns.h"

Area *owned;
Area *delegated;

/*
 *  true if a name is in our area
 */
Area*
inmyarea(char *name)
{
	int len;
	Area *s, *d;

	len = strlen(name);
	for(s = owned; s; s = s->next){
		if(s->len > len)
			continue;
		if(cistrcmp(s->soarr->owner->name, name + len - s->len) == 0)
			if(len == s->len || name[len - s->len - 1] == '.')
				break;
	}
	if(s == 0)
		return 0;

	for(d = delegated; d; d = d->next){
		if(d->len > len)
			continue;
		if(cistrcmp(d->soarr->owner->name, name + len - d->len) == 0)
			if(len == d->len || name[len - d->len - 1] == '.')
				return 0;
	}

	return s;
}

/*
 *  our area is the part of the domain tree that
 *  we serve
 */
void
addarea(DN *dp, RR *rp, Ndbtuple *t)
{
	Area **l, *s;

	if(t->val[0])
		l = &delegated;
	else
		l = &owned;

	/*
	 *  The area contains a copy of the soa rr that created it.
	 *  The owner of the the soa rr should stick around as long
	 *  as the area does.
	 */
	s = emalloc(sizeof(*s));
	s->len = strlen(dp->name);
	rrcopy(rp, &s->soarr);
	s->soarr->owner = dp;
	s->soarr->db = 1;
	s->soarr->ttl = Hour;
	s->neednotify = 1;
	s->needrefresh = 0;

syslog(0, logfile, "new area %s", dp->name);

	s->next = *l;
	*l = s;
}

void
freearea(Area **l)
{
	Area *s;

	while(s = *l){
		*l = s->next;
		rrfree(s->soarr);
		free(s);
	}
}

/*
 * refresh all areas that need it
 *  this entails running a command 'zonerefreshprogram'.  This could
 *  copy over databases from elsewhere or just do a zone transfer.
 */
void
refresh_areas(Area *s)
{
	Waitmsg *w;
	char *argv[3];

	argv[0] = zonerefreshprogram;
	argv[1] = "XXX";
	argv[2] = nil;

	for(; s != nil; s = s->next){
		if(!s->needrefresh)
			continue;

		if(zonerefreshprogram == nil){
			s->needrefresh = 0;
			continue;
		}

		argv[1] = s->soarr->owner->name;
		w = runproc(argv[0], argv, 0);
		free(w);
	}
}
