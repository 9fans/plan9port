#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ndb.h>
#include <ip.h>

enum
{
	Ffound=	1<<0,
	Fignore=	1<<1,
	Faddr=	1<<2
};

static Ndbtuple*	filter(Ndb *db, Ndbtuple *t, Ndbtuple *f);
static Ndbtuple*	mkfilter(int argc, char **argv);
static int		filtercomplete(Ndbtuple *f);
static int		prefixlen(uchar *ip);
static Ndbtuple*	subnet(Ndb *db, uchar *net, Ndbtuple *f, int prefix);

/* make a filter to be used in filter */
static Ndbtuple*
mkfilter(int argc, char **argv)
{
	Ndbtuple *t, *first, *last;
	char *p;

	last = first = nil;
	while(argc-- > 0){
		t = ndbnew(0, 0);
		if(first)
			last->entry = t;
		else
			first = t;
		last = t;
		p = *argv++;
		if(*p == '@'){
			t->ptr |= Faddr;
			p++;
		}
		strncpy(t->attr, p, sizeof(t->attr)-1);
	}
	return first;
}

/* return true if every pair of filter has been used */
static int
filtercomplete(Ndbtuple *f)
{
	for(; f; f = f->entry)
		if((f->ptr & Fignore) == 0)
			return 0;
	return 1;
}

/* set the attribute of all entries in a tuple */
static Ndbtuple*
setattr(Ndbtuple *t, char *attr)
{
	Ndbtuple *nt;

	for(nt = t; nt; nt = nt->entry)
		strcpy(nt->attr, attr);
	return t;
}

/*
 *  return only the attr/value pairs in t maching the filter, f.
 *  others are freed.  line structure is preserved.
 */
static Ndbtuple*
filter(Ndb *db, Ndbtuple *t, Ndbtuple *f)
{
	Ndbtuple *nt, *nf, *next;

	/* filter out what we don't want */
	for(nt = t; nt; nt = next){
		next = nt->entry;

		/* look through filter */
		for(nf = f; nf != nil; nf = nf->entry){
			if(!(nf->ptr&Fignore) && strcmp(nt->attr, nf->attr) == 0)
				break;
		}
		if(nf == nil){
			/* remove nt from t */
			t = ndbdiscard(t, nt);
		} else {
			if(nf->ptr & Faddr)
				t = ndbsubstitute(t, nt, setattr(ndbgetipaddr(db, nt->val), nt->attr));
			nf->ptr |= Ffound;
		}
	}

	/* remember filter etnries that matched */
	for(nf = f; nf != nil; nf = nf->entry)
		if(nf->ptr & Ffound)
			nf->ptr = (nf->ptr & ~Ffound) | Fignore;

	return t;
}

static int
prefixlen(uchar *ip)
{
	int y, i;

	for(y = IPaddrlen-1; y >= 0; y--)
		for(i = 8; i > 0; i--)
			if(ip[y] & (1<<(8-i)))
				return y*8 + i;
	return 0;
}

/*
 *  look through a containing subset
 */
static Ndbtuple*
subnet(Ndb *db, uchar *net, Ndbtuple *f, int prefix)
{
	Ndbs s;
	Ndbtuple *t, *nt, *xt;
	char netstr[128];
	uchar mask[IPaddrlen];
	int masklen;

	t = nil;
	sprint(netstr, "%I", net);
	nt = ndbsearch(db, &s, "ip", netstr);
	while(nt != nil){
		xt = ndbfindattr(nt, nt, "ipnet");
		if(xt){
			xt = ndbfindattr(nt, nt, "ipmask");
			if(xt)
				parseipmask(mask, xt->val);
			else
				ipmove(mask, defmask(net));
			masklen = prefixlen(mask);
			if(masklen <= prefix)
				t = ndbconcatenate(t, filter(db, nt, f));
		} else
			ndbfree(nt);
		nt = ndbsnext(&s, "ip", netstr);
	}
	return t;
}

/*
 *  fill in all the requested attributes for a system.
 *  if the system's entry doesn't have all required,
 *  walk through successively more inclusive networks
 *  for inherited attributes.
 */
Ndbtuple*
ndbipinfo(Ndb *db, char *attr, char *val, char **alist, int n)
{
	Ndbtuple *t, *nt, *f;
	Ndbs s;
	char *ipstr;
	uchar net[IPaddrlen];
	uchar ip[IPaddrlen];
	int prefix, smallestprefix;
	int force;

	/* just in case */
	fmtinstall('I', eipfmt);
	fmtinstall('M', eipfmt);

	/* get needed attributes */
	f = mkfilter(n, alist);

	/*
	 *  first look for a matching entry with an ip address
	 */
	t = nil;
	ipstr = ndbgetvalue(db, &s, attr, val, "ip", &nt);
	if(ipstr == nil){
		/* none found, make one up */
		if(strcmp(attr, "ip") != 0)
			return nil;
		t = ndbnew("ip", val);
		t->line = t;
		t->entry = nil;
		parseip(net, val);
	} else {
		/* found one */
		while(nt != nil){
			nt = ndbreorder(nt, s.t);
			t = ndbconcatenate(t, nt);
			nt = ndbsnext(&s, attr, val);
		}
		parseip(net, ipstr);
		free(ipstr);
	}
	ipmove(ip, net);
	t = filter(db, t, f);

	/*
	 *  now go through subnets to fill in any missing attributes
	 */
	if(isv4(net)){
		prefix = 127;
		smallestprefix = 100;
		force = 0;
	} else {
		/* in v6, the last 8 bytes have no structure (we hope) */
		prefix = 64;
		smallestprefix = 2;
		memset(net+8, 0, 8);
		force = 1;
	}

	/*
	 *  to find a containing network, keep turning off
	 *  the lower bit and look for a network with
	 *  that address and a shorter mask.  tedius but
	 *  complete, we may need to find a trick to speed this up.
	 */
	for(; prefix >= smallestprefix; prefix--){
		if(filtercomplete(f))
			break;
		if(!force && (net[prefix/8] & (1<<(7-(prefix%8)))) == 0)
			continue;
		force = 0;
		net[prefix/8] &= ~(1<<(7-(prefix%8)));
		t = ndbconcatenate(t, subnet(db, net, f, prefix));
	}

	/*
	 *  if there's an unfulfilled ipmask, make one up
	 */
	nt = ndbfindattr(f, f, "ipmask");
	if(nt && !(nt->ptr & Fignore)){
		char x[64];

		snprint(x, sizeof(x), "%M", defmask(ip));
		t = ndbconcatenate(t, ndbnew("ipmask", x));
	}

	ndbfree(f);
	return t;
}
