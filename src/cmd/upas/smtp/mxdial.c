#include "common.h"
#include <ndb.h>
#include "smtp.h"	/* to publish dial_string_parse */
#include <ip.h>
#include <thread.h>

enum
{
	Nmx=	16,
	Maxstring=	256
};

typedef struct Mx	Mx;
struct Mx
{
	char host[256];
	char ip[24];
	int pref;
};
static Mx mx[Nmx];

Ndb *db;
extern int debug;

static int	mxlookup(DS*, char*);
static int	compar(const void*, const void*);
static int	callmx(DS*, char*, char*);
static void expand_meta(DS *ds);
extern int	cistrcmp(char*, char*);

/* Taken from imapdial, replaces tlsclient call with stunnel */
static int
smtpdial(char *server)
{
	int p[2];
	int fd[3];
	char *tmp;
	char *fpath;

	if(pipe(p) < 0)
		return -1;
	fd[0] = dup(p[0], -1);
	fd[1] = dup(p[0], -1);
	fd[2] = dup(2, -1);
#ifdef PLAN9PORT
	tmp = smprint("%s:587", server);
	fpath = searchpath("stunnel3");
	if (!fpath) {
		werrstr("stunnel not found. it is required for tls support.");
		return -1;
	}
	if(threadspawnl(fd, fpath, "stunnel", "-n", "smtp" , "-c", "-r", tmp, nil) < 0) {
#else
	tmp = smprint("tcp!%s!587", server);
	if(threadspawnl(fd, "/bin/tlsclient", "tlsclient", tmp, nil) < 0){
#endif
		free(tmp);
		close(p[0]);
		close(p[1]);
		close(fd[0]);
		close(fd[1]);
		close(fd[2]);
		return -1;
	}
	free(tmp);
	close(p[0]);
	return p[1];
}

int
mxdial(char *addr, char *ddomain, char *gdomain)
{
	int fd;
	DS ds;
	char err[Errlen];

	addr = netmkaddr(addr, 0, "smtp");
	dial_string_parse(addr, &ds);

	/* try connecting to destination or any of it's mail routers */
	fd = callmx(&ds, addr, ddomain);

	/* try our mail gateway */
	rerrstr(err, sizeof(err));
	if(fd < 0 && gdomain && strstr(err, "can't translate") != 0)
		fd = dial(netmkaddr(gdomain, 0, "smtp"), 0, 0, 0);

	return fd;
}

static int
timeout(void *v, char *msg)
{
	USED(v);

	if(strstr(msg, "alarm"))
		return 1;
	return 0;
}

/*
 *  take an address and return all the mx entries for it,
 *  most preferred first
 */
static int
callmx(DS *ds, char *dest, char *domain)
{
	int fd, i, nmx;
	char addr[Maxstring];

	/* get a list of mx entries */
	nmx = mxlookup(ds, domain);
	if(nmx < 0){
		/* dns isn't working, don't just dial */
		return -1;
	}
	if(nmx == 0){
		if(debug)
			fprint(2, "mxlookup returns nothing\n");
		return dial(dest, 0, 0, 0);
	}

	/* refuse to honor loopback addresses given by dns */
	for(i = 0; i < nmx; i++){
		if(strcmp(mx[i].ip, "127.0.0.1") == 0){
			if(debug)
				fprint(2, "mxlookup returns loopback\n");
			werrstr("illegal: domain lists 127.0.0.1 as mail server");
			return -1;
		}
	}

	/* sort by preference */
	if(nmx > 1)
		qsort(mx, nmx, sizeof(Mx), compar);

	if(debug){
		for(i=0; i<nmx; i++)
			print("%s %d\n", mx[i].host, mx[i].pref);
	}
	/* dial each one in turn */
	for(i = 0; i < nmx; i++){
#ifdef PLAN9PORT
		snprint(addr, sizeof(addr), "%s", mx[i].host);
#else
		snprint(addr, sizeof(addr), "%s!%s!%s", ds->proto,
			mx[i].host, ds->service);
#endif
		if(debug)
			fprint(2, "mxdial trying %s (%d)\n", addr, i);
		atnotify(timeout, 1);
		alarm(10*1000);
#ifdef PLAN9PORT
		fd = smtpdial(addr);
#else
		fd = dial(addr, 0, 0, 0);
#endif
		alarm(0);
		atnotify(timeout, 0);
		if(fd >= 0)
			return fd;
	}
	return -1;
}

/*
 *  use dns to resolve the mx request
 */
static int
mxlookup(DS *ds, char *domain)
{
	int i, n, nmx;
	Ndbtuple *t, *tmx, *tpref, *tip;

	strcpy(domain, ds->host);
	ds->netdir = "/net";
	nmx = 0;
	if((t = dnsquery(nil, ds->host, "mx")) != nil){
		for(tmx=t; (tmx=ndbfindattr(tmx->entry, nil, "mx")) != nil && nmx<Nmx; ){
			for(tpref=tmx->line; tpref != tmx; tpref=tpref->line){
				if(strcmp(tpref->attr, "pref") == 0){
					strncpy(mx[nmx].host, tmx->val, sizeof(mx[n].host)-1);
					mx[nmx].pref = atoi(tpref->val);
					nmx++;
					break;
				}
			}
		}
		ndbfree(t);
	}

	/*
	 * no mx record? try name itself.
	 */
	/*
	 * BUG? If domain has no dots, then we used to look up ds->host
	 * but return domain instead of ds->host in the list.  Now we return
	 * ds->host.  What will this break?
	 */
	if(nmx == 0){
		mx[0].pref = 1;
		strncpy(mx[0].host, ds->host, sizeof(mx[0].host));
		nmx++;
	}

	/*
	 * look up all ip addresses
	 */
	for(i = 0; i < nmx; i++){
		if((t = dnsquery(nil, mx[i].host, "ip")) == nil)
			goto no;
		if((tip = ndbfindattr(t, nil, "ip")) == nil){
			ndbfree(t);
			goto no;
		}
		strncpy(mx[i].ip, tip->val, sizeof(mx[i].ip)-1);
		ndbfree(t);
		continue;

	no:
		/* remove mx[i] and go around again */
		nmx--;
		mx[i] = mx[nmx];
		i--;
	}
	return nmx;
}

static int
compar(const void *a, const void *b)
{
	return ((Mx*)a)->pref - ((Mx*)b)->pref;
}

/* break up an address to its component parts */
void
dial_string_parse(char *str, DS *ds)
{
	char *p, *p2;

	strncpy(ds->buf, str, sizeof(ds->buf));
	ds->buf[sizeof(ds->buf)-1] = 0;

	p = strchr(ds->buf, '!');
	if(p == 0) {
		ds->netdir = 0;
		ds->proto = "net";
		ds->host = ds->buf;
	} else {
		if(*ds->buf != '/'){
			ds->netdir = 0;
			ds->proto = ds->buf;
		} else {
			for(p2 = p; *p2 != '/'; p2--)
				;
			*p2++ = 0;
			ds->netdir = ds->buf;
			ds->proto = p2;
		}
		*p = 0;
		ds->host = p + 1;
	}
	ds->service = strchr(ds->host, '!');
	if(ds->service)
		*ds->service++ = 0;
	if(*ds->host == '$')
		expand_meta(ds);
}

static void
expand_meta(DS *ds)
{
	static Ndb *db;
	Ndbs s;
	char *sys, *smtpserver;

	/* can't ask cs, so query database directly. */
	sys = sysname();
	if(db == nil)
		db = ndbopen(0);
	smtpserver = ndbgetvalue(db, &s, "sys", sys, "smtp", nil);
	snprint(ds->host, 128, "%s", smtpserver);
}
