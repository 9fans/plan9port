#include <u.h>
#include <libc.h>
#include <ip.h>
#include <bio.h>
#include <ndb.h>

static uchar noether[6];

/*
 *  Look for a pair with the given attribute.  look first on the same line,
 *  then in the whole entry.
 */
static Ndbtuple*
lookval(Ndbtuple *entry, Ndbtuple *line, char *attr, char *to)
{
	Ndbtuple *nt;

	/* first look on same line (closer binding) */
	for(nt = line;;){
		if(strcmp(attr, nt->attr) == 0){
			strncpy(to, nt->val, Ndbvlen);
			return nt;
		}
		nt = nt->line;
		if(nt == line)
			break;
	}
	/* search whole tuple */
	for(nt = entry; nt; nt = nt->entry)
		if(strcmp(attr, nt->attr) == 0){
			strncpy(to, nt->val, Ndbvlen);
			return nt;
		}
	return 0;
}

/*
 *  lookup an ip address
 */
static uchar*
lookupip(Ndb *db, char *name, uchar *to, Ipinfo *iip)
{
	Ndbtuple *t, *nt;
	char buf[Ndbvlen];
	uchar subnet[IPaddrlen];
	Ndbs s;
	char *attr;

	attr = ipattr(name);
	if(strcmp(attr, "ip") == 0){
		parseip(to, name);
		return to;
	}

	t = ndbgetval(db, &s, attr, name, "ip", buf);
	if(t){
		/* first look for match on same subnet */
		for(nt = t; nt; nt = nt->entry){
			if(strcmp(nt->attr, "ip") != 0)
				continue;
			parseip(to, nt->val);
			maskip(to, iip->ipmask, subnet);
			if(memcmp(subnet, iip->ipnet, sizeof(subnet)) == 0)
				return to;
		}

		/* otherwise, just take what we have */
		ndbfree(t);
		parseip(to, buf);
		return to;
	}
	return 0;
}

/*
 *  lookup a subnet and fill in anything we can
 */
static void
recursesubnet(Ndb *db, uchar *mask, Ipinfo *iip, char *fs, char *gw, char *au)
{
	Ndbs s;
	Ndbtuple *t;
	uchar submask[IPaddrlen];
	char ip[Ndbvlen];

	memmove(iip->ipmask, mask, 4);
	maskip(iip->ipaddr, iip->ipmask, iip->ipnet);
	sprint(ip, "%I", iip->ipnet);
	t = ndbsearch(db, &s, "ip", ip);
print("%s->", ip);
	if(t){
		/* look for a further subnet */
		if(lookval(t, s.t, "ipmask", ip)){
			parseip(submask, ip);

			/* recurse only if it has changed */
			if(!equivip(submask, mask))
				recursesubnet(db, submask, iip, fs, gw, au);

		}

		/* fill in what we don't have */
		if(gw[0] == 0)
			lookval(t, s.t, "ipgw", gw);
		if(fs[0] == 0)
			lookval(t, s.t, "fs", fs);
		if(au[0] == 0)
			lookval(t, s.t, "auth", au);

		ndbfree(t);
	}
}
#ifdef foo
/*
 *  find out everything we can about a system from what has been
 *  specified.
 */
int
ipinfo(Ndb *db, char *etherin, char *ipin, char *name, Ipinfo *iip)
{
	Ndbtuple *t;
	Ndbs s;
	char ether[Ndbvlen];
	char ip[Ndbvlen];
	char fsname[Ndbvlen];
	char gwname[Ndbvlen];
	char auname[Ndbvlen];

	memset(iip, 0, sizeof(Ipinfo));
	fsname[0] = 0;
	gwname[0] = 0;
	auname[0] = 0;

	/*
	 *  look for a matching entry
	 */
	t = 0;
	if(etherin)
		t = ndbgetval(db, &s, "ether", etherin, "ip", ip);
	if(t == 0 && ipin)
		t = ndbsearch(db, &s, "ip", ipin);
	if(t == 0 && name)
		t = ndbgetval(db, &s, ipattr(name), name, "ip", ip);
	if(t){
		/*
		 *  copy in addresses and name
		 */
		if(lookval(t, s.t, "ip", ip))
			parseip(iip->ipaddr, ip);
		if(lookval(t, s.t, "ether", ether))
			parseether(iip->etheraddr, ether);
		lookval(t, s.t, "dom", iip->domain);

		/*
		 *  Look for bootfile, fs, and gateway.
		 *  If necessary, search through all entries for
		 *  this ip address.
		 */
		while(t){
			if(iip->bootf[0] == 0)
				lookval(t, s.t, "bootf", iip->bootf);
			if(fsname[0] == 0)
				lookval(t, s.t, "fs", fsname);
			if(gwname[0] == 0)
				lookval(t, s.t, "ipgw", gwname);
			if(auname[0] == 0)
				lookval(t, s.t, "auth", auname);
			ndbfree(t);
			if(iip->bootf[0] && fsname[0] && gwname[0] && auname[0])
				break;
			t = ndbsnext(&s, "ether", ether);
		}
	} else if(ipin) {
		/*
		 *  copy in addresses (all we know)
		 */
		parseip(iip->ipaddr, ipin);
		if(etherin)
			parseether(iip->etheraddr, etherin);
	} else
		return -1;

	/*
	 *  Look up the client's network and find a subnet mask for it.
	 *  Fill in from the subnet (or net) entry anything we can't figure
	 *  out from the client record.
	 */
	recursesubnet(db, classmask[CLASS(iip->ipaddr)], iip, fsname, gwname, auname);

	/* lookup fs's and gw's ip addresses */

	if(fsname[0])
		lookupip(db, fsname, iip->fsip, iip);
	if(gwname[0])
		lookupip(db, gwname, iip->gwip, iip);
	if(auname[0])
		lookupip(db, auname, iip->auip, iip);
	return 0;
}
#endif
void
main(int argc, char **argv)
{
	Ipinfo ii;
	Ndb *db;

	db = ndbopen(0);

	fmtinstall('E', eipconv);
	fmtinstall('I', eipconv);
	if(argc < 2)
		exits(0);
	if(strchr(argv[1], '.')){
		if(ipinfo(db, 0, argv[1], 0, &ii) < 0)
			exits(0);
	} else {
		if(ipinfo(db, argv[1], 0, 0, &ii) < 0)
			exits(0);
	}
	fprint(2, "a %I m %I n %I f %s e %E\n", ii.ipaddr,
		ii.ipmask, ii.ipnet, ii.bootf, ii.etheraddr);
}
