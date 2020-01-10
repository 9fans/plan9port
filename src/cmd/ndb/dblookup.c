#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ndb.h>
#include <ip.h>
#include "dns.h"

static Ndb *db;

static RR*	dblookup1(char*, int, int, int);
static RR*	addrrr(Ndbtuple*, Ndbtuple*);
static RR*	nsrr(Ndbtuple*, Ndbtuple*);
static RR*	cnamerr(Ndbtuple*, Ndbtuple*);
static RR*	mxrr(Ndbtuple*, Ndbtuple*);
static RR*	soarr(Ndbtuple*, Ndbtuple*);
static RR*	ptrrr(Ndbtuple*, Ndbtuple*);
static Ndbtuple* look(Ndbtuple*, Ndbtuple*, char*);
static RR*	doaxfr(Ndb*, char*);
static RR*	nullrr(Ndbtuple *entry, Ndbtuple *pair);
static RR*	txtrr(Ndbtuple *entry, Ndbtuple *pair);
static Lock	dblock;
static void	createptrs(void);

static int	implemented[Tall] =
{
	0,
	/* Ta */ 1,
	/* Tns */ 1,
	0,
	0,
	/* Tcname */ 1,
	/* Tsoa */ 1,
	0,
	0,
	0,
	/* Tnull */ 1,
	0,
	/* Tptr */ 1,
	0,
	0,
	/* Tmx */ 1,
	/* Ttxt */ 1
};

static void
nstrcpy(char *to, char *from, int len)
{
	strncpy(to, from, len);
	to[len-1] = 0;
}

int
opendatabase(void)
{
	char buf[256];
	Ndb *xdb;

	if(db == nil){
		snprint(buf, sizeof(buf), "%s/ndb", mntpt);
		xdb = ndbopen(dbfile);
		if(xdb != nil)
			xdb->nohash = 1;
		db = ndbcat(ndbopen(buf), xdb);
	}
	if(db == nil)
		return -1;
	else
		return 0;
}

/*
 *  lookup an RR in the network database, look for matches
 *  against both the domain name and the wildcarded domain name.
 *
 *  the lock makes sure only one process can be accessing the data
 *  base at a time.  This is important since there's a lot of
 *  shared state there.
 *
 *  e.g. for x.research.bell-labs.com, first look for a match against
 *       the x.research.bell-labs.com.  If nothing matches, try *.research.bell-labs.com.
 */
RR*
dblookup(char *name, int class, int type, int auth, int ttl)
{
	RR *rp, *tp;
	char buf[256];
	char *wild, *cp;
	DN *dp, *ndp;
	int err;

	/* so far only internet lookups are implemented */
	if(class != Cin)
		return 0;

	err = Rname;

	if(type == Tall){
		rp = 0;
		for (type = Ta; type < Tall; type++)
			if(implemented[type])
				rrcat(&rp, dblookup(name, class, type, auth, ttl));
		return rp;
	}

	lock(&dblock);
	rp = nil;
	dp = dnlookup(name, class, 1);
	if(opendatabase() < 0)
		goto out;
	if(dp->rr)
		err = 0;

	/* first try the given name */
	rp = 0;
	if(cachedb)
		rp = rrlookup(dp, type, NOneg);
	else
		rp = dblookup1(name, type, auth, ttl);
	if(rp)
		goto out;

	/* try lower case version */
	for(cp = name; *cp; cp++)
		*cp = tolower((uchar)*cp);
	if(cachedb)
		rp = rrlookup(dp, type, NOneg);
	else
		rp = dblookup1(name, type, auth, ttl);
	if(rp)
		goto out;

	/* walk the domain name trying the wildcard '*' at each position */
	for(wild = strchr(name, '.'); wild; wild = strchr(wild+1, '.')){
		snprint(buf, sizeof(buf), "*%s", wild);
		ndp = dnlookup(buf, class, 1);
		if(ndp->rr)
			err = 0;
		if(cachedb)
			rp = rrlookup(ndp, type, NOneg);
		else
			rp = dblookup1(buf, type, auth, ttl);
		if(rp)
			break;
	}
out:
	/* add owner to uncached records */
	if(rp){
		for(tp = rp; tp; tp = tp->next)
			tp->owner = dp;
	} else {
		/* don't call it non-existent if it's not ours */
		if(err == Rname && !inmyarea(name))
			err = Rserver;
		dp->nonexistent = err;
	}

	unlock(&dblock);
	return rp;
}

/*
 *  lookup an RR in the network database
 */
static RR*
dblookup1(char *name, int type, int auth, int ttl)
{
	Ndbtuple *t, *nt;
	RR *rp, *list, **l;
	Ndbs s;
	char dname[Domlen];
	char *attr;
	DN *dp;
	RR *(*f)(Ndbtuple*, Ndbtuple*);
	int found, x;

	dp = 0;
	switch(type){
	case Tptr:
		attr = "ptr";
		f = ptrrr;
		break;
	case Ta:
		attr = "ip";
		f = addrrr;
		break;
	case Tnull:
		attr = "nullrr";
		f = nullrr;
		break;
	case Tns:
		attr = "ns";
		f = nsrr;
		break;
	case Tsoa:
		attr = "soa";
		f = soarr;
		break;
	case Tmx:
		attr = "mx";
		f = mxrr;
		break;
	case Tcname:
		attr = "cname";
		f = cnamerr;
		break;
	case Taxfr:
	case Tixfr:
		return doaxfr(db, name);
	default:
		return nil;
	}

	/*
	 *  find a matching entry in the database
	 */
	free(ndbgetvalue(db, &s, "dom", name, attr, &t));

	/*
	 *  hack for local names
	 */
	if(t == 0 && strchr(name, '.') == 0)
		free(ndbgetvalue(db, &s, "sys", name, attr, &t));
	if(t == 0)
		return nil;

	/* search whole entry for default domain name */
	strncpy(dname, name, sizeof dname);
	for(nt = t; nt; nt = nt->entry)
		if(strcmp(nt->attr, "dom") == 0){
			nstrcpy(dname, nt->val, sizeof dname);
			break;
		}

	/* ttl is maximum of soa minttl and entry's ttl ala rfc883 */
	nt = look(t, s.t, "ttl");
	if(nt){
		x = atoi(nt->val);
		if(x > ttl)
			ttl = x;
	}

	/* default ttl is one day */
	if(ttl < 0)
		ttl = DEFTTL;

	/*
	 *  The database has 2 levels of precedence; line and entry.
	 *  Pairs on the same line bind tighter than pairs in the
	 *  same entry, so we search the line first.
	 */
	found = 0;
	list = 0;
	l = &list;
	for(nt = s.t;; ){
		if(found == 0 && strcmp(nt->attr, "dom") == 0){
			nstrcpy(dname, nt->val, sizeof dname);
			found = 1;
		}
		if(cistrcmp(attr, nt->attr) == 0){
			rp = (*f)(t, nt);
			rp->auth = auth;
			rp->db = 1;
			if(ttl)
				rp->ttl = ttl;
			if(dp == 0)
				dp = dnlookup(dname, Cin, 1);
			rp->owner = dp;
			*l = rp;
			l = &rp->next;
			nt->ptr = 1;
		}
		nt = nt->line;
		if(nt == s.t)
			break;
	}

	/* search whole entry */
	for(nt = t; nt; nt = nt->entry)
		if(nt->ptr == 0 && cistrcmp(attr, nt->attr) == 0){
			rp = (*f)(t, nt);
			rp->db = 1;
			if(ttl)
				rp->ttl = ttl;
			rp->auth = auth;
			if(dp == 0)
				dp = dnlookup(dname, Cin, 1);
			rp->owner = dp;
			*l = rp;
			l = &rp->next;
		}
	ndbfree(t);

	return list;
}

/*
 *  make various types of resource records from a database entry
 */
static RR*
addrrr(Ndbtuple *entry, Ndbtuple *pair)
{
	RR *rp;
	uchar addr[IPaddrlen];

	USED(entry);
	parseip(addr, pair->val);
	if(isv4(addr))
		rp = rralloc(Ta);
	else
		rp = rralloc(Taaaa);
	rp->ip = dnlookup(pair->val, Cin, 1);
	return rp;
}
static RR*
nullrr(Ndbtuple *entry, Ndbtuple *pair)
{
	RR *rp;

	USED(entry);
	rp = rralloc(Tnull);
	rp->null->data = (uchar*)estrdup(pair->val);
	rp->null->dlen = strlen((char*)rp->null->data);
	return rp;
}
/*
 *  txt rr strings are at most 255 bytes long.  one
 *  can represent longer strings by multiple concatenated
 *  <= 255 byte ones.
 */
static RR*
txtrr(Ndbtuple *entry, Ndbtuple *pair)
{
	RR *rp;
	Txt *t, **l;
	int i, len, sofar;

	USED(entry);
	rp = rralloc(Ttxt);
	l = &rp->txt;
	rp->txt = nil;
	len = strlen(pair->val);
	sofar = 0;
	while(len > sofar){
		t = emalloc(sizeof(*t));
		t->next = nil;

		i = len-sofar;
		if(i > 255)
			i = 255;

		t->p = emalloc(i+1);
		memmove(t->p, pair->val+sofar, i);
		t->p[i] = 0;
		sofar += i;

		*l = t;
		l = &t->next;
	}
	return rp;
}
static RR*
cnamerr(Ndbtuple *entry, Ndbtuple *pair)
{
	RR *rp;

	USED(entry);
	rp = rralloc(Tcname);
	rp->host = dnlookup(pair->val, Cin, 1);
	return rp;
}
static RR*
mxrr(Ndbtuple *entry, Ndbtuple *pair)
{
	RR * rp;

	rp = rralloc(Tmx);
	rp->host = dnlookup(pair->val, Cin, 1);
	pair = look(entry, pair, "pref");
	if(pair)
		rp->pref = atoi(pair->val);
	else
		rp->pref = 1;
	return rp;
}
static RR*
nsrr(Ndbtuple *entry, Ndbtuple *pair)
{
	RR *rp;
	Ndbtuple *t;

	rp = rralloc(Tns);
	rp->host = dnlookup(pair->val, Cin, 1);
	t = look(entry, pair, "soa");
	if(t && t->val[0] == 0)
		rp->local = 1;
	return rp;
}
static RR*
ptrrr(Ndbtuple *entry, Ndbtuple *pair)
{
	RR *rp;

	USED(entry);
	rp = rralloc(Tns);
	rp->ptr = dnlookup(pair->val, Cin, 1);
	return rp;
}
static RR*
soarr(Ndbtuple *entry, Ndbtuple *pair)
{
	RR *rp;
	Ndbtuple *ns, *mb, *t;
	char mailbox[Domlen];
	Ndb *ndb;
	char *p;

	rp = rralloc(Tsoa);
	rp->soa->serial = 1;
	for(ndb = db; ndb; ndb = ndb->next)
		if(ndb->mtime > rp->soa->serial)
			rp->soa->serial = ndb->mtime;
	rp->soa->refresh = Day;
	rp->soa->retry = Hour;
	rp->soa->expire = Day;
	rp->soa->minttl = Day;
	t = look(entry, pair, "ttl");
	if(t)
		rp->soa->minttl = atoi(t->val);
	t = look(entry, pair, "refresh");
	if(t)
		rp->soa->refresh = atoi(t->val);
	t = look(entry, pair, "serial");
	if(t)
		rp->soa->serial = strtoul(t->val, 0, 10);

	ns = look(entry, pair, "ns");
	if(ns == 0)
		ns = look(entry, pair, "dom");
	rp->host = dnlookup(ns->val, Cin, 1);

	/* accept all of:
	 *  mbox=person
	 *  mbox=person@machine.dom
	 *  mbox=person.machine.dom
	 */
	mb = look(entry, pair, "mbox");
	if(mb == nil)
		mb = look(entry, pair, "mb");
	if(mb){
		if(strchr(mb->val, '.')) {
			p = strchr(mb->val, '@');
			if(p != nil)
				*p = '.';
			rp->rmb = dnlookup(mb->val, Cin, 1);
		} else {
			snprint(mailbox, sizeof(mailbox), "%s.%s",
				mb->val, ns->val);
			rp->rmb = dnlookup(mailbox, Cin, 1);
		}
	} else {
		snprint(mailbox, sizeof(mailbox), "postmaster.%s",
			ns->val);
		rp->rmb = dnlookup(mailbox, Cin, 1);
	}

	/*  hang dns slaves off of the soa.  this is
	 *  for managing the area.
	 */
	for(t = entry; t != nil; t = t->entry)
		if(strcmp(t->attr, "dnsslave") == 0)
			addserver(&rp->soa->slaves, t->val);

	return rp;
}

/*
 *  Look for a pair with the given attribute.  look first on the same line,
 *  then in the whole entry.
 */
static Ndbtuple*
look(Ndbtuple *entry, Ndbtuple *line, char *attr)
{
	Ndbtuple *nt;

	/* first look on same line (closer binding) */
	for(nt = line;;){
		if(cistrcmp(attr, nt->attr) == 0)
			return nt;
		nt = nt->line;
		if(nt == line)
			break;
	}
	/* search whole tuple */
	for(nt = entry; nt; nt = nt->entry)
		if(cistrcmp(attr, nt->attr) == 0)
			return nt;
	return 0;
}

/* these are answered specially by the tcp version */
static RR*
doaxfr(Ndb *db, char *name)
{
	USED(db);
	USED(name);
	return 0;
}


/*
 *  read the all the soa's from the database to determine area's.
 *  this is only used when we're not caching the database.
 */
static void
dbfile2area(Ndb *db)
{
	Ndbtuple *t;

	if(debug)
		syslog(0, logfile, "rereading %s", db->file);
	Bseek(&db->b, 0, 0);
	while(t = ndbparse(db)){
		ndbfree(t);
	}
}

/*
 *  read the database into the cache
 */
static void
dbpair2cache(DN *dp, Ndbtuple *entry, Ndbtuple *pair)
{
	RR *rp;
	Ndbtuple *t;
	static ulong ord;

	rp = 0;
	if(cistrcmp(pair->attr, "ip") == 0){
		dp->ordinal = ord++;
		rp = addrrr(entry, pair);
	} else 	if(cistrcmp(pair->attr, "ns") == 0){
		rp = nsrr(entry, pair);
	} else if(cistrcmp(pair->attr, "soa") == 0){
		rp = soarr(entry, pair);
		addarea(dp, rp, pair);
	} else if(cistrcmp(pair->attr, "mx") == 0){
		rp = mxrr(entry, pair);
	} else if(cistrcmp(pair->attr, "cname") == 0){
		rp = cnamerr(entry, pair);
	} else if(cistrcmp(pair->attr, "nullrr") == 0){
		rp = nullrr(entry, pair);
	} else if(cistrcmp(pair->attr, "txtrr") == 0){
		rp = txtrr(entry, pair);
	}

	if(rp == 0)
		return;

	rp->owner = dp;
	rp->db = 1;
	t = look(entry, pair, "ttl");
	if(t)
		rp->ttl = atoi(t->val);
	rrattach(rp, 0);
}
static void
dbtuple2cache(Ndbtuple *t)
{
	Ndbtuple *et, *nt;
	DN *dp;

	for(et = t; et; et = et->entry){
		if(strcmp(et->attr, "dom") == 0){
			dp = dnlookup(et->val, Cin, 1);

			/* first same line */
			for(nt = et->line; nt != et; nt = nt->line){
				dbpair2cache(dp, t, nt);
				nt->ptr = 1;
			}

			/* then rest of entry */
			for(nt = t; nt; nt = nt->entry){
				if(nt->ptr == 0)
					dbpair2cache(dp, t, nt);
				nt->ptr = 0;
			}
		}
	}
}
static void
dbfile2cache(Ndb *db)
{
	Ndbtuple *t;

	if(debug)
		syslog(0, logfile, "rereading %s", db->file);
	Bseek(&db->b, 0, 0);
	while(t = ndbparse(db)){
		dbtuple2cache(t);
		ndbfree(t);
	}
}
void
db2cache(int doit)
{
	Ndb *ndb;
	Dir *d;
	ulong youngest, temp;
	static ulong lastcheck;
	static ulong lastyoungest;

	/* no faster than once every 2 minutes */
	if(now < lastcheck + 2*Min && !doit)
		return;

	refresh_areas(owned);

	lock(&dblock);

	if(opendatabase() < 0){
		unlock(&dblock);
		return;
	}

	/*
	 *  file may be changing as we are reading it, so loop till
	 *  mod times are consistent.
	 *
	 *  we don't use the times in the ndb records because they may
	 *  change outside of refreshing our cached knowledge.
	 */
	for(;;){
		lastcheck = now;
		youngest = 0;
		for(ndb = db; ndb; ndb = ndb->next){
			/* the dirfstat avoids walking the mount table each time */
			if((d = dirfstat(Bfildes(&ndb->b))) != nil ||
			   (d = dirstat(ndb->file)) != nil){
				temp = d->mtime;		/* ulong vs int crap */
				if(temp > youngest)
					youngest = temp;
				free(d);
			}
		}
		if(!doit && youngest == lastyoungest){
			unlock(&dblock);
			return;
		}

		/* forget our area definition */
		freearea(&owned);
		freearea(&delegated);

		/* reopen all the files (to get oldest for time stamp) */
		for(ndb = db; ndb; ndb = ndb->next)
			ndbreopen(ndb);

		if(cachedb){
			/* mark all db records as timed out */
			dnagedb();

			/* read in new entries */
			for(ndb = db; ndb; ndb = ndb->next)
				dbfile2cache(ndb);

			/* mark as authentic anything in our domain */
			dnauthdb();

			/* remove old entries */
			dnageall(1);
		} else {
			/* read all the soa's to get database defaults */
			for(ndb = db; ndb; ndb = ndb->next)
				dbfile2area(ndb);
		}

		doit = 0;
		lastyoungest = youngest;
		createptrs();
	}

	unlock(&dblock);
}

extern uchar	ipaddr[IPaddrlen];

/*
 *  get all my xxx
 */
Ndbtuple*
lookupinfo(char *attr)
{
	char buf[64];
	char *a[2];
	static Ndbtuple *t;

	snprint(buf, sizeof buf, "%I", ipaddr);
	a[0] = attr;

	lock(&dblock);
	if(opendatabase() < 0){
		unlock(&dblock);
		return nil;
	}
	t = ndbipinfo(db, "ip", buf, a, 1);
	unlock(&dblock);
	return t;
}

char *localservers = "local#dns#servers";
char *localserverprefix = "local#dns#server";

/*
 *  return non-zero is this is a bad delegation
 */
int
baddelegation(RR *rp, RR *nsrp, uchar *addr)
{
	Ndbtuple *nt;
	static Ndbtuple *t;

	if(t == nil)
		t = lookupinfo("dom");
	if(t == nil)
		return 0;

	for(; rp; rp = rp->next){
		if(rp->type != Tns)
			continue;

		/* see if delegation is looping */
		if(nsrp)
		if(rp->owner != nsrp->owner)
		if(subsume(rp->owner->name, nsrp->owner->name) &&
		   strcmp(nsrp->owner->name, localservers) != 0){
			syslog(0, logfile, "delegation loop %R -> %R from %I", nsrp, rp, addr);
			return 1;
		}

		/* see if delegating to us what we don't own */
		for(nt = t; nt != nil; nt = nt->entry)
			if(rp->host && cistrcmp(rp->host->name, nt->val) == 0)
				break;
		if(nt != nil && !inmyarea(rp->owner->name)){
			syslog(0, logfile, "bad delegation %R from %I", rp, addr);
			return 1;
		}
	}

	return 0;
}

static void
addlocaldnsserver(DN *dp, int class, char *ipaddr, int i)
{
	DN *nsdp;
	RR *rp;
	char buf[32];

	/* ns record for name server, make up an impossible name */
	rp = rralloc(Tns);
	snprint(buf, sizeof(buf), "%s%d", localserverprefix, i);
	nsdp = dnlookup(buf, class, 1);
	rp->host = nsdp;
	rp->owner = dp;
	rp->local = 1;
	rp->db = 1;
	rp->ttl = 10*Min;
	rrattach(rp, 1);

print("dns %s\n", ipaddr);
	/* A record */
	rp = rralloc(Ta);
	rp->ip = dnlookup(ipaddr, class, 1);
	rp->owner = nsdp;
	rp->local = 1;
	rp->db = 1;
	rp->ttl = 10*Min;
	rrattach(rp, 1);
}

/*
 *  return list of dns server addresses to use when
 *  acting just as a resolver.
 */
RR*
dnsservers(int class)
{
	Ndbtuple *t, *nt;
	RR *nsrp;
	DN *dp;
	char *p;
	int i, n;
	char *buf, *args[5];

	dp = dnlookup(localservers, class, 1);
	nsrp = rrlookup(dp, Tns, NOneg);
	if(nsrp != nil)
		return nsrp;

	p = getenv("DNSSERVER");
	if(p != nil){
		buf = estrdup(p);
		n = tokenize(buf, args, nelem(args));
		for(i = 0; i < n; i++)
			addlocaldnsserver(dp, class, args[i], i);
		free(buf);
	} else {
		t = lookupinfo("@dns");
		if(t == nil)
			return nil;
		i = 0;
		for(nt = t; nt != nil; nt = nt->entry){
			addlocaldnsserver(dp, class, nt->val, i);
			i++;
		}
		ndbfree(t);
	}

	return rrlookup(dp, Tns, NOneg);
}

static void
addlocaldnsdomain(DN *dp, int class, char *domain)
{
	RR *rp;

	/* A record */
	rp = rralloc(Tptr);
	rp->ptr = dnlookup(domain, class, 1);
	rp->owner = dp;
	rp->db = 1;
	rp->ttl = 10*Min;
	rrattach(rp, 1);
}

/*
 *  return list of domains to use when resolving names without '.'s
 */
RR*
domainlist(int class)
{
	Ndbtuple *t, *nt;
	RR *rp;
	DN *dp;

	dp = dnlookup("local#dns#domains", class, 1);
	rp = rrlookup(dp, Tptr, NOneg);
	if(rp != nil)
		return rp;

	t = lookupinfo("dnsdomain");
	if(t == nil)
		return nil;
	for(nt = t; nt != nil; nt = nt->entry)
		addlocaldnsdomain(dp, class, nt->val);
	ndbfree(t);

	return rrlookup(dp, Tptr, NOneg);
}

char *v4ptrdom = ".in-addr.arpa";
char *v6ptrdom = ".ip6.arpa";		/* ip6.int deprecated, rfc 3152 */

char *attribs[] = {
	"ipmask",
	0
};

/*
 *  create ptrs that are in our areas
 */
static void
createptrs(void)
{
	int len, dlen, n;
	Area *s;
	char *f[40];
	char buf[Domlen+1];
	uchar net[IPaddrlen];
	uchar mask[IPaddrlen];
	char ipa[48];
	Ndbtuple *t, *nt;

	dlen = strlen(v4ptrdom);
	for(s = owned; s; s = s->next){
		len = strlen(s->soarr->owner->name);
		if(len <= dlen)
			continue;
		if(cistrcmp(s->soarr->owner->name+len-dlen, v4ptrdom) != 0)
			continue;

		/* get mask and net value */
		strncpy(buf, s->soarr->owner->name, sizeof(buf));
		buf[sizeof(buf)-1] = 0;
		n = getfields(buf, f, nelem(f), 0, ".");
		memset(mask, 0xff, IPaddrlen);
		ipmove(net, v4prefix);
		switch(n){
		case 3: /* /8 */
			net[IPv4off] = atoi(f[0]);
			mask[IPv4off+1] = 0;
			mask[IPv4off+2] = 0;
			mask[IPv4off+3] = 0;
			break;
		case 4: /* /16 */
			net[IPv4off] = atoi(f[1]);
			net[IPv4off+1] = atoi(f[0]);
			mask[IPv4off+2] = 0;
			mask[IPv4off+3] = 0;
			break;
		case 5: /* /24 */
			net[IPv4off] = atoi(f[2]);
			net[IPv4off+1] = atoi(f[1]);
			net[IPv4off+2] = atoi(f[0]);
			mask[IPv4off+3] = 0;
			break;
		case 6:	/* rfc2317 */
			net[IPv4off] = atoi(f[3]);
			net[IPv4off+1] = atoi(f[2]);
			net[IPv4off+2] = atoi(f[1]);
			net[IPv4off+3] = atoi(f[0]);
			sprint(ipa, "%I", net);
			t = ndbipinfo(db, "ip", ipa, attribs, 1);
			if(t == nil) /* could be a reverse with no forward */
				continue;
			nt = look(t, t, "ipmask");
			if(nt == nil){	/* we're confused */
				ndbfree(t);
				continue;
			}
			parseipmask(mask, nt->val);
			n = 5;
			break;
		default:
			continue;
		}

		/* go through all domain entries looking for RR's in this network and create ptrs */
		dnptr(net, mask, s->soarr->owner->name, 6-n, 0);
	}
}
