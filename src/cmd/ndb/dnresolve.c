#include <u.h>
#include <sys/time.h>
#include <libc.h>
#include <ip.h>
#include <bio.h>
#include <ndb.h>
#include "dns.h"

enum
{
	Maxdest=	24,	/* maximum destinations for a request message */
	Maxtrans=	3	/* maximum transmissions to a server */
};

static int	netquery(DN*, int, RR*, Request*, int);
static RR*	dnresolve1(char*, int, int, Request*, int, int);

char *LOG = "dns";

/*
 *  lookup 'type' info for domain name 'name'.  If it doesn't exist, try
 *  looking it up as a canonical name.
 */
RR*
dnresolve(char *name, int class, int type, Request *req, RR **cn, int depth, int recurse, int rooted, int *status)
{
	RR *rp, *nrp, *drp;
	DN *dp;
	int loops;
	char nname[Domlen];

	if(status)
		*status = 0;

	/*
	 *  hack for systems that don't have resolve search
	 *  lists.  Just look up the simple name in the database.
	 */
	if(!rooted && strchr(name, '.') == 0){
		rp = nil;
		drp = domainlist(class);
		for(nrp = drp; nrp != nil; nrp = nrp->next){
			snprint(nname, sizeof(nname), "%s.%s", name, nrp->ptr->name);
			rp = dnresolve(nname, class, type, req,cn, depth, recurse, rooted, status);
			rrfreelist(rrremneg(&rp));
			if(rp != nil)
				break;
		}
		if(drp != nil)
			rrfree(drp);
		return rp;
	}

	/*
	 *  try the name directly
	 */
	rp = dnresolve1(name, class, type, req, depth, recurse);
	if(rp)
		return randomize(rp);

	/* try it as a canonical name if we weren't told the name didn't exist */
	dp = dnlookup(name, class, 0);
	if(type != Tptr && dp->nonexistent != Rname){
		for(loops=0; rp == nil && loops < 32; loops++){
			rp = dnresolve1(name, class, Tcname, req, depth, recurse);
			if(rp == nil)
				break;

			if(rp->negative){
				rrfreelist(rp);
				rp = nil;
				break;
			}

			name = rp->host->name;
			if(cn)
				rrcat(cn, rp);
			else
				rrfreelist(rp);

			rp = dnresolve1(name, class, type, req, depth, recurse);
		}
	}

	/* distinction between not found and not good */
	if(rp == 0 && status != 0 && dp->nonexistent != 0)
		*status = dp->nonexistent;

	return randomize(rp);
}

static RR*
dnresolve1(char *name, int class, int type, Request *req, int depth, int recurse)
{
	DN *dp, *nsdp;
	RR *rp, *nsrp, *dbnsrp;
	char *cp;

	if(debug)
		syslog(0, LOG, "dnresolve1 %s %d %d", name, type, class);

	/* only class Cin implemented so far */
	if(class != Cin)
		return 0;

	dp = dnlookup(name, class, 1);

	/*
	 *  Try the cache first
	 */
	rp = rrlookup(dp, type, OKneg);
	if(rp){
		if(rp->db){
			/* unauthenticated db entries are hints */
			if(rp->auth)
				return rp;
		} else {
			/* cached entry must still be valid */
			if(rp->ttl > now){
				/* but Tall entries are special */
				if(type != Tall || rp->query == Tall)
					return rp;
			}
		}
	}
	rrfreelist(rp);

	/*
	 * try the cache for a canonical name. if found punt
	 * since we'll find it during the canonical name search
	 * in dnresolve().
	 */
	if(type != Tcname){
		rp = rrlookup(dp, Tcname, NOneg);
		rrfreelist(rp);
		if(rp)
			return 0;
	}

	/*
	 *  if we're running as just a resolver, go to our
	 *  designated name servers
	 */
	if(resolver){
		nsrp = randomize(getdnsservers(class));
		if(nsrp != nil) {
			if(netquery(dp, type, nsrp, req, depth+1)){
				rrfreelist(nsrp);
				return rrlookup(dp, type, OKneg);
			}
			rrfreelist(nsrp);
		}
	}

	/*
 	 *  walk up the domain name looking for
	 *  a name server for the domain.
	 */
	for(cp = name; cp; cp = walkup(cp)){
		/*
		 *  if this is a local (served by us) domain,
		 *  return answer
		 */
		dbnsrp = randomize(dblookup(cp, class, Tns, 0, 0));
		if(dbnsrp && dbnsrp->local){
			rp = dblookup(name, class, type, 1, dbnsrp->ttl);
			rrfreelist(dbnsrp);
			return rp;
		}

		/*
		 *  if recursion isn't set, just accept local
		 *  entries
		 */
		if(recurse == Dontrecurse){
			if(dbnsrp)
				rrfreelist(dbnsrp);
			continue;
		}

		/* look for ns in cache */
		nsdp = dnlookup(cp, class, 0);
		nsrp = nil;
		if(nsdp)
			nsrp = randomize(rrlookup(nsdp, Tns, NOneg));

		/* if the entry timed out, ignore it */
		if(nsrp && nsrp->ttl < now){
			rrfreelist(nsrp);
			nsrp = nil;
		}

		if(nsrp){
			rrfreelist(dbnsrp);

			/* try the name servers found in cache */
			if(netquery(dp, type, nsrp, req, depth+1)){
				rrfreelist(nsrp);
				return rrlookup(dp, type, OKneg);
			}
			rrfreelist(nsrp);
			continue;
		}

		/* use ns from db */
		if(dbnsrp){
			/* try the name servers found in db */
			if(netquery(dp, type, dbnsrp, req, depth+1)){
				/* we got an answer */
				rrfreelist(dbnsrp);
				return rrlookup(dp, type, NOneg);
			}
			rrfreelist(dbnsrp);
		}
	}

	/* settle for a non-authoritative answer */
	rp = rrlookup(dp, type, OKneg);
	if(rp)
		return rp;

	/* noone answered.  try the database, we might have a chance. */
	return dblookup(name, class, type, 0, 0);
}

/*
 *  walk a domain name one element to the right.  return a pointer to that element.
 *  in other words, return a pointer to the parent domain name.
 */
char*
walkup(char *name)
{
	char *cp;

	cp = strchr(name, '.');
	if(cp)
		return cp+1;
	else if(*name)
		return "";
	else
		return 0;
}

/*
 *  Get a udpport for requests and replies.
 */
int
udpport(void)
{
	int fd;

	if((fd = dial("udp!0!53", nil, nil, nil)) < 0)
		warning("can't get udp port");
	return fd;
}

int
mkreq(DN *dp, int type, uchar *buf, int flags, ushort reqno)
{
	DNSmsg m;
	int len;
	Udphdr *uh = (Udphdr*)buf;

	/* stuff port number into output buffer */
	memset(uh, 0, sizeof(*uh));
	hnputs(uh->rport, 53);

	/* make request and convert it to output format */
	memset(&m, 0, sizeof(m));
	m.flags = flags;
	m.id = reqno;
	m.qd = rralloc(type);
	m.qd->owner = dp;
	m.qd->type = type;
	len = convDNS2M(&m, &buf[Udphdrsize], Maxudp);
	if(len < 0)
		abort(); /* "can't convert" */;
	rrfree(m.qd);
	return len;
}

static void
freeanswers(DNSmsg *mp)
{
	rrfreelist(mp->qd);
	rrfreelist(mp->an);
	rrfreelist(mp->ns);
	rrfreelist(mp->ar);
}

/*
 *  read replies to a request.  ignore any of the wrong type.  wait at most 5 seconds.
 */
static int udpreadtimeout(int, Udphdr*, void*, int, int);
static int
readreply(int fd, DN *dp, int type, ushort req,
	  uchar *ibuf, DNSmsg *mp, ulong endtime, Request *reqp)
{
	char *err;
	int len;
	ulong now;
	RR *rp;

	for(; ; freeanswers(mp)){
		now = time(0);
		if(now >= endtime)
			return -1;	/* timed out */

		/* timed read */
		len = udpreadtimeout(fd, (Udphdr*)ibuf, ibuf+Udphdrsize, Maxudpin, (endtime-now)*1000);
		if(len < 0)
			return -1;	/* timed out */

		/* convert into internal format  */
		memset(mp, 0, sizeof(*mp));
		err = convM2DNS(&ibuf[Udphdrsize], len, mp);
		if(err){
			syslog(0, LOG, "input err %s: %I", err, ibuf);
			continue;
		}
		if(debug)
			logreply(reqp->id, ibuf, mp);

		/* answering the right question? */
		if(mp->id != req){
			syslog(0, LOG, "%d: id %d instead of %d: %I", reqp->id,
					mp->id, req, ibuf);
			continue;
		}
		if(mp->qd == 0){
			syslog(0, LOG, "%d: no question RR: %I", reqp->id, ibuf);
			continue;
		}
		if(mp->qd->owner != dp){
			syslog(0, LOG, "%d: owner %s instead of %s: %I", reqp->id,
				mp->qd->owner->name, dp->name, ibuf);
			continue;
		}
		if(mp->qd->type != type){
			syslog(0, LOG, "%d: type %d instead of %d: %I", reqp->id,
				mp->qd->type, type, ibuf);
			continue;
		}

		/* remember what request this is in answer to */
		for(rp = mp->an; rp; rp = rp->next)
			rp->query = type;

		return 0;
	}

	return 0;	/* never reached */
}

static int
udpreadtimeout(int fd, Udphdr *h, void *data, int n, int ms)
{
	fd_set rd;
	struct timeval tv;

	FD_ZERO(&rd);
	FD_SET(fd, &rd);

	tv.tv_sec = ms/1000;
	tv.tv_usec = (ms%1000)*1000;

	if(select(fd+1, &rd, 0, 0, &tv) != 1)
		return -1;
	return udpread(fd, h, data, n);
}

/*
 *	return non-0 if first list includes second list
 */
int
contains(RR *rp1, RR *rp2)
{
	RR *trp1, *trp2;

	for(trp2 = rp2; trp2; trp2 = trp2->next){
		for(trp1 = rp1; trp1; trp1 = trp1->next){
			if(trp1->type == trp2->type)
			if(trp1->host == trp2->host)
			if(trp1->owner == trp2->owner)
				break;
		}
		if(trp1 == 0)
			return 0;
	}

	return 1;
}


typedef struct Dest	Dest;
struct Dest
{
	uchar	a[IPaddrlen];	/* ip address */
	DN	*s;		/* name server */
	int	nx;		/* number of transmissions */
	int	code;
};


/*
 *  return multicast version if any
 */
int
ipisbm(uchar *ip)
{
	if(isv4(ip)){
		if(ip[IPv4off] >= 0xe0 && ip[IPv4off] < 0xf0)
			return 4;
		if(ipcmp(ip, IPv4bcast) == 0)
			return 4;
	} else {
		if(ip[0] == 0xff)
			return 6;
	}
	return 0;
}

/*
 *  Get next server address
 */
static int
serveraddrs(RR *nsrp, Dest *dest, int nd, int depth, Request *reqp)
{
	RR *rp, *arp, *trp;
	Dest *cur;

	if(nd >= Maxdest)
		return 0;

	/*
	 *  look for a server whose address we already know.
	 *  if we find one, mark it so we ignore this on
	 *  subsequent passes.
	 */
	arp = 0;
	for(rp = nsrp; rp; rp = rp->next){
		assert(rp->magic == RRmagic);
		if(rp->marker)
			continue;
		arp = rrlookup(rp->host, Ta, NOneg);
		if(arp){
			rp->marker = 1;
			break;
		}
		arp = dblookup(rp->host->name, Cin, Ta, 0, 0);
		if(arp){
			rp->marker = 1;
			break;
		}
	}

	/*
	 *  if the cache and database lookup didn't find any new
	 *  server addresses, try resolving one via the network.
	 *  Mark any we try to resolve so we don't try a second time.
	 */
	if(arp == 0){
		for(rp = nsrp; rp; rp = rp->next){
			if(rp->marker)
				continue;
			rp->marker = 1;

			/*
			 *  avoid loops looking up a server under itself
			 */
			if(subsume(rp->owner->name, rp->host->name))
				continue;

			arp = dnresolve(rp->host->name, Cin, Ta, reqp, 0, depth+1, Recurse, 1, 0);
			rrfreelist(rrremneg(&arp));
			if(arp)
				break;
		}
	}

	/* use any addresses that we found */
	for(trp = arp; trp; trp = trp->next){
		if(nd >= Maxdest)
			break;
		cur = &dest[nd];
		parseip(cur->a, trp->ip->name);
		if(ipisbm(cur->a))
			continue;
		cur->nx = 0;
		cur->s = trp->owner;
		cur->code = Rtimeout;
		nd++;
	}
	rrfreelist(arp);
	return nd;
}

/*
 *  cache negative responses
 */
static void
cacheneg(DN *dp, int type, int rcode, RR *soarr)
{
	RR *rp;
	DN *soaowner;
	ulong ttl;

	/* no cache time specified, don' make anything up */
	if(soarr != nil){
		if(soarr->next != nil){
			rrfreelist(soarr->next);
			soarr->next = nil;
		}
		soaowner = soarr->owner;
	} else
		soaowner = nil;

	/* the attach can cause soarr to be freed so mine it now */
	if(soarr != nil && soarr->soa != nil)
		ttl = soarr->soa->minttl+now;
	else
		ttl = 5*Min;

	/* add soa and negative RR to the database */
	rrattach(soarr, 1);

	rp = rralloc(type);
	rp->owner = dp;
	rp->negative = 1;
	rp->negsoaowner = soaowner;
	rp->negrcode = rcode;
	rp->ttl = ttl;
	rrattach(rp, 1);
}

/*
 *  query name servers.  If the name server returns a pointer to another
 *  name server, recurse.
 */
static int
netquery1(int fd, DN *dp, int type, RR *nsrp, Request *reqp, int depth, uchar *ibuf, uchar *obuf)
{
	int ndest, j, len, replywaits, rv;
	ushort req;
	RR *tp, *soarr;
	Dest *p, *l, *np;
	DN *ndp;
	Dest dest[Maxdest];
	DNSmsg m;
	ulong endtime;
	Udphdr *uh;

	/* pack request into a message */
	req = rand();
	len = mkreq(dp, type, obuf, Frecurse|Oquery, req);

	/* no server addresses yet */
	l = dest;

	/*
	 *  transmit requests and wait for answers.
	 *  at most Maxtrans attempts to each address.
	 *  each cycle send one more message than the previous.
	 */
	for(ndest = 1; ndest < Maxdest; ndest++){
		p = dest;

		endtime = time(0);
		if(endtime >= reqp->aborttime)
			break;

		/* get a server address if we need one */
		if(ndest > l - p){
			j = serveraddrs(nsrp, dest, l - p, depth, reqp);
			l = &dest[j];
		}

		/* no servers, punt */
		if(l == dest)
			break;

		/* send to first 'ndest' destinations */
		j = 0;
		for(; p < &dest[ndest] && p < l; p++){
			/* skip destinations we've finished with */
			if(p->nx >= Maxtrans)
				continue;

			j++;

			/* exponential backoff of requests */
			if((1<<p->nx) > ndest)
				continue;

			memmove(obuf, p->a, sizeof(p->a));
			if(debug)
				logsend(reqp->id, depth, obuf, p->s->name,
					dp->name, type);
			uh = (Udphdr*)obuf;
			fprint(2, "send %I %I %d %d\n", uh->raddr, uh->laddr, nhgets(uh->rport), nhgets(uh->lport));
			if(udpwrite(fd, uh, obuf+Udphdrsize, len) < 0)
				warning("sending udp msg %r");
			p->nx++;
		}
		if(j == 0)
			break;		/* no destinations left */

		/* wait up to 5 seconds for replies */
		endtime = time(0) + 5;
		if(endtime > reqp->aborttime)
			endtime = reqp->aborttime;

		for(replywaits = 0; replywaits < ndest; replywaits++){
			if(readreply(fd, dp, type, req, ibuf, &m, endtime, reqp) < 0)
				break;		/* timed out */

			/* find responder */
			for(p = dest; p < l; p++)
				if(memcmp(p->a, ibuf, sizeof(p->a)) == 0)
					break;

			/* remove all addrs of responding server from list */
			for(np = dest; np < l; np++)
				if(np->s == p->s)
					p->nx = Maxtrans;

			/* ignore any error replies */
			if((m.flags & Rmask) == Rserver){
				rrfreelist(m.qd);
				rrfreelist(m.an);
				rrfreelist(m.ar);
				rrfreelist(m.ns);
				if(p != l)
					p->code = Rserver;
				continue;
			}

			/* ignore any bad delegations */
			if(m.ns && baddelegation(m.ns, nsrp, ibuf)){
				rrfreelist(m.ns);
				m.ns = nil;
				if(m.an == nil){
					rrfreelist(m.qd);
					rrfreelist(m.ar);
					if(p != l)
						p->code = Rserver;
					continue;
				}
			}


			/* remove any soa's from the authority section */
			soarr = rrremtype(&m.ns, Tsoa);

			/* incorporate answers */
			if(m.an)
				rrattach(m.an, (m.flags & Fauth) ? 1 : 0);
			if(m.ar)
				rrattach(m.ar, 0);
			if(m.ns){
				ndp = m.ns->owner;
				rrattach(m.ns, 0);
			} else
				ndp = 0;

			/* free the question */
			if(m.qd)
				rrfreelist(m.qd);

			/*
			 *  Any reply from an authoritative server,
			 *  or a positive reply terminates the search
			 */
			if(m.an != nil || (m.flags & Fauth)){
				if(m.an == nil && (m.flags & Rmask) == Rname)
					dp->nonexistent = Rname;
				else
					dp->nonexistent = 0;

				/*
				 *  cache any negative responses, free soarr
				 */
				if((m.flags & Fauth) && m.an == nil)
					cacheneg(dp, type, (m.flags & Rmask), soarr);
				else
					rrfreelist(soarr);
				return 1;
			}
			rrfreelist(soarr);

			/*
			 *  if we've been given better name servers
			 *  recurse
			 */
			if(m.ns){
				tp = rrlookup(ndp, Tns, NOneg);
				if(!contains(nsrp, tp)){
					rv = netquery(dp, type, tp, reqp, depth+1);
					rrfreelist(tp);
					return rv;
				} else
					rrfreelist(tp);
			}
		}
	}

	/* if all servers returned failure, propogate it */
	dp->nonexistent = Rserver;
	for(p = dest; p < l; p++)
		if(p->code != Rserver)
			dp->nonexistent = 0;

	return 0;
}

typedef struct Qarg Qarg;
struct Qarg
{
	DN *dp;
	int type;
	RR *nsrp;
	Request *reqp;
	int depth;
};


static int
netquery(DN *dp, int type, RR *nsrp, Request *reqp, int depth)
{
	uchar *obuf;
	uchar *ibuf;
	RR *rp;
	int fd, rv;

	if(depth > 12)
		return 0;

	/* use alloced buffers rather than ones from the stack */
	ibuf = emalloc(Maxudpin+Udphdrsize);
	obuf = emalloc(Maxudp+Udphdrsize);

	/* prepare server RR's for incremental lookup */
	for(rp = nsrp; rp; rp = rp->next)
		rp->marker = 0;

	fd = udpport();
	if(fd < 0)
		return 0;
	rv = netquery1(fd, dp, type, nsrp, reqp, depth, ibuf, obuf);
	close(fd);
	free(ibuf);
	free(obuf);

	return rv;
}
