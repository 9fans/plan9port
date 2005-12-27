#include <u.h>
#include <libc.h>
#include <ip.h>
#include "dns.h"

enum
{
	Maxpath=		128,
};

char	*logfile = "dns";
char	*dbfile;
int	debug;
int	cachedb = 1;
int	testing;
int traceactivity;
int	needrefresh;
int 	resolver;
char	mntpt[Maxpath];
char	*caller = "";
ulong	now;
int	maxage;
uchar	ipaddr[IPaddrlen];	/* my ip address */
char	*LOG;
char	*zonerefreshprogram;

static int	readmsg(int, uchar*, int);
static void	reply(int, DNSmsg*, Request*);
static void	dnzone(DNSmsg*, DNSmsg*, Request*);
static void	getcaller(char*);
static void	refreshmain(char*);

void
main(int argc, char *argv[])
{
	int len;
	Request req;
	DNSmsg reqmsg, repmsg;
	uchar buf[512];
	char tname[32];
	char *err;
	char *ext = "";

	ARGBEGIN{
	case 'd':
		debug++;
		break;
	case 'f':
		dbfile = ARGF();
		break;
	case 'r':
		resolver = 1;
		break;
	case 'x':
		ext = ARGF();
		break;
	}ARGEND

	if(debug < 2)
		debug = 0;

	if(argc > 0)
		getcaller(argv[0]);

	dninit();

	snprint(mntpt, sizeof(mntpt), "/net%s", ext);
	if(myipaddr(ipaddr, mntpt) < 0)
		sysfatal("can't read my ip address");
	syslog(0, logfile, "dnstcp call from %s to %I", caller, ipaddr);

	db2cache(1);

	setjmp(req.mret);
	req.isslave = 0;

	/* loop on requests */
	for(;; putactivity()){
		now = time(0);
		memset(&repmsg, 0, sizeof(repmsg));
		alarm(10*60*1000);
		len = readmsg(0, buf, sizeof(buf));
		alarm(0);
		if(len <= 0)
			break;
		getactivity(&req);
		req.aborttime = now + 15*Min;
		err = convM2DNS(buf, len, &reqmsg);
		if(err){
			syslog(0, logfile, "server: input error: %s from %I", err, buf);
			break;
		}
		if(reqmsg.qdcount < 1){
			syslog(0, logfile, "server: no questions from %I", buf);
			break;
		}
		if(reqmsg.flags & Fresp){
			syslog(0, logfile, "server: reply not request from %I", buf);
			break;
		}
		if((reqmsg.flags & Omask) != Oquery){
			syslog(0, logfile, "server: op %d from %I", reqmsg.flags & Omask, buf);
			break;
		}

		if(debug)
			syslog(0, logfile, "%d: serve (%s) %d %s %s",
				req.id, caller,
				reqmsg.id,
				reqmsg.qd->owner->name,
				rrname(reqmsg.qd->type, tname, sizeof tname));

		/* loop through each question */
		while(reqmsg.qd){
			if(reqmsg.qd->type == Taxfr){
				dnzone(&reqmsg, &repmsg, &req);
			} else {
				dnserver(&reqmsg, &repmsg, &req);
				reply(1, &repmsg, &req);
				rrfreelist(repmsg.qd);
				rrfreelist(repmsg.an);
				rrfreelist(repmsg.ns);
				rrfreelist(repmsg.ar);
			}
		}

		rrfreelist(reqmsg.qd);
		rrfreelist(reqmsg.an);
		rrfreelist(reqmsg.ns);
		rrfreelist(reqmsg.ar);

		if(req.isslave){
			putactivity();
			_exits(0);
		}
	}
	refreshmain(mntpt);
}

static int
readmsg(int fd, uchar *buf, int max)
{
	int n;
	uchar x[2];

	if(readn(fd, x, 2) != 2)
		return -1;
	n = (x[0]<<8) | x[1];
	if(n > max)
		return -1;
	if(readn(fd, buf, n) != n)
		return -1;
	return n;
}

static void
reply(int fd, DNSmsg *rep, Request *req)
{
	int len, rv;
	char tname[32];
	uchar buf[4096];
	RR *rp;

	if(debug){
		syslog(0, logfile, "%d: reply (%s) %s %s %ux",
			req->id, caller,
			rep->qd->owner->name,
			rrname(rep->qd->type, tname, sizeof tname),
			rep->flags);
		for(rp = rep->an; rp; rp = rp->next)
			syslog(0, logfile, "an %R", rp);
		for(rp = rep->ns; rp; rp = rp->next)
			syslog(0, logfile, "ns %R", rp);
		for(rp = rep->ar; rp; rp = rp->next)
			syslog(0, logfile, "ar %R", rp);
	}


	len = convDNS2M(rep, buf+2, sizeof(buf) - 2);
	if(len <= 0)
		abort(); /* "dnserver: converting reply" */;
	buf[0] = len>>8;
	buf[1] = len;
	rv = write(fd, buf, len+2);
	if(rv != len+2){
		syslog(0, logfile, "sending reply: %d instead of %d", rv, len+2);
		exits(0);
	}
}

/*
 *  Hash table for domain names.  The hash is based only on the
 *  first element of the domain name.
 */
extern DN	*ht[HTLEN];

static int
numelem(char *name)
{
	int i;

	i = 1;
	for(; *name; name++)
		if(*name == '.')
			i++;
	return i;
}

int
inzone(DN *dp, char *name, int namelen, int depth)
{
	int n;

	if(dp->name == 0)
		return 0;
	if(numelem(dp->name) != depth)
		return 0;
	n = strlen(dp->name);
	if(n < namelen)
		return 0;
	if(strcmp(name, dp->name + n - namelen) != 0)
		return 0;
	if(n > namelen && dp->name[n - namelen - 1] != '.')
		return 0;
	return 1;
}

static void
dnzone(DNSmsg *reqp, DNSmsg *repp, Request *req)
{
	DN *dp, *ndp;
	RR r, *rp;
	int h, depth, found, nlen;

	memset(repp, 0, sizeof(*repp));
	repp->id = reqp->id;
	repp->flags = Fauth | Fresp | Fcanrec | Oquery;
	repp->qd = reqp->qd;
	reqp->qd = reqp->qd->next;
	repp->qd->next = 0;
	dp = repp->qd->owner;

	/* send the soa */
	repp->an = rrlookup(dp, Tsoa, NOneg);
	reply(1, repp, req);
	if(repp->an == 0)
		goto out;
	rrfreelist(repp->an);

	nlen = strlen(dp->name);

	/* construct a breadth first search of the name space (hard with a hash) */
	repp->an = &r;
	for(depth = numelem(dp->name); ; depth++){
		found = 0;
		for(h = 0; h < HTLEN; h++)
			for(ndp = ht[h]; ndp; ndp = ndp->next)
				if(inzone(ndp, dp->name, nlen, depth)){
					for(rp = ndp->rr; rp; rp = rp->next){
						/* there shouldn't be negatives, but just in case */
						if(rp->negative)
							continue;

						/* don't send an soa's, ns's are enough */
						if(rp->type == Tsoa)
							continue;

						r = *rp;
						r.next = 0;
						reply(1, repp, req);
					}
					found = 1;
				}
		if(!found)
			break;
	}

	/* resend the soa */
	repp->an = rrlookup(dp, Tsoa, NOneg);
	reply(1, repp, req);
	rrfreelist(repp->an);
out:
	rrfree(repp->qd);
}

static void
getcaller(char *dir)
{
	int fd, n;
	static char remote[128];

	snprint(remote, sizeof(remote), "%s/remote", dir);
	fd = open(remote, OREAD);
	if(fd < 0)
		return;
	n = read(fd, remote, sizeof(remote)-1);
	close(fd);
	if(n <= 0)
		return;
	if(remote[n-1] == '\n')
		n--;
	remote[n] = 0;
	caller = remote;
}

static void
refreshmain(char *net)
{
	int fd;
	char file[128];

	snprint(file, sizeof(file), "%s/dns", net);
	if(debug)
		syslog(0, logfile, "refreshing %s", file);
	fd = open(file, ORDWR);
	if(fd < 0){
		syslog(0, logfile, "can't refresh %s", file);
		return;
	}
	fprint(fd, "refresh");
	close(fd);
}

/*
 *  the following varies between dnsdebug and dns
 */
void
logreply(int id, uchar *addr, DNSmsg *mp)
{
	RR *rp;

	syslog(0, LOG, "%d: rcvd %I flags:%s%s%s%s%s", id, addr,
		mp->flags & Fauth ? " auth" : "",
		mp->flags & Ftrunc ? " trunc" : "",
		mp->flags & Frecurse ? " rd" : "",
		mp->flags & Fcanrec ? " ra" : "",
		mp->flags & (Fauth|Rname) == (Fauth|Rname) ?
		" nx" : "");
	for(rp = mp->qd; rp != nil; rp = rp->next)
		syslog(0, LOG, "%d: rcvd %I qd %s", id, addr, rp->owner->name);
	for(rp = mp->an; rp != nil; rp = rp->next)
		syslog(0, LOG, "%d: rcvd %I an %R", id, addr, rp);
	for(rp = mp->ns; rp != nil; rp = rp->next)
		syslog(0, LOG, "%d: rcvd %I ns %R", id, addr, rp);
	for(rp = mp->ar; rp != nil; rp = rp->next)
		syslog(0, LOG, "%d: rcvd %I ar %R", id, addr, rp);
}

void
logsend(int id, int subid, uchar *addr, char *sname, char *rname, int type)
{
	char buf[12];

	syslog(0, LOG, "%d.%d: sending to %I/%s %s %s",
		id, subid, addr, sname, rname, rrname(type, buf, sizeof buf));
}

RR*
getdnsservers(int class)
{
	return dnsservers(class);
}
