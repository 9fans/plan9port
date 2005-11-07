#include <u.h>
#include <libc.h>
#include <authsrv.h>
#include <bio.h>
#include <ndb.h>

int
authdial(char *netroot, char *dom)
{
	char *p;
	int rv;
	Ndb *db;
	char *file;

	if(dom){
		file = unsharp("#9/ndb/local");
		db = ndbopen(file);
		if(db == nil){
			fprint(2, "open %s: %r\n", file);
			free(file);
			return -1;
		}
		free(file);
		p = ndbgetvalue(db, nil, "authdom", dom, "auth", nil);
		if(p == nil)
			p = ndbgetvalue(db, nil, "dom", dom, "auth", nil);
		if(p == nil)
			p = dom;
		rv = dial(netmkaddr(p, "tcp", "ticket"), 0, 0, 0);
		if(p != dom)
			free(p);
		ndbclose(db);
		return rv;
	}
	p = getenv("auth");
	if(p == nil)
		p = "$auth";
	return dial(netmkaddr(p, "tcp", "ticket"), 0, 0, 0);
}
