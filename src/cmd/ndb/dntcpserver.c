#include <u.h>
#include <libc.h>
#include <ip.h>
#include <bio.h>
#include <ndb.h>
#include <thread.h>
#include "dns.h"

static char adir[40];

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

static int
connreadmsg(int tfd, int *fd, uchar *buf, int max)
{
	int n;
	int lfd;
	char ldir[40];

	lfd = listen(adir, ldir);
	if (lfd < 0)
		return -1;
	*fd = accept(lfd, ldir);
	if (*fd >= 0)
		n = readmsg(*fd, buf, max);
	else
		n = -1;
	close(lfd);
	return n;
}

static int
reply(int fd, DNSmsg *rep, Request *req, NetConnInfo *caller)
{
	int len;
	char tname[32];
	uchar buf[4096];
	RR *rp;

	if(debug){
		syslog(0, logfile, "%d: reply (%s) %s %s %ux",
			req->id, caller ? caller->raddr : "unk",
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
	if(write(fd, buf, len+2) < 0){
		syslog(0, logfile, "sending reply: %r");
		return -1;
	}
	return 0;
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

static int
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

static int
dnzone(DNSmsg *reqp, DNSmsg *repp, Request *req, int rfd, NetConnInfo *caller)
{
	DN *dp, *ndp;
	RR r, *rp;
	int h, depth, found, nlen, rv;

	rv = 0;
	memset(repp, 0, sizeof(*repp));
	repp->id = reqp->id;
	repp->flags = Fauth | Fresp | Fcanrec | Oquery;
	repp->qd = reqp->qd;
	reqp->qd = reqp->qd->next;
	repp->qd->next = 0;
	dp = repp->qd->owner;

	/* send the soa */
	repp->an = rrlookup(dp, Tsoa, NOneg);
	rv = reply(rfd, repp, req, caller);
	if(repp->an == 0 || rv < 0)
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
						rv = reply(rfd, repp, req, caller);
						if(rv < 0)
							goto out;
					}
					found = 1;
				}
		if(!found)
			break;
	}

	/* resend the soa */
	repp->an = rrlookup(dp, Tsoa, NOneg);
	rv = reply(rfd, repp, req, caller);
out:
	if (repp->an)
		rrfreelist(repp->an);
	rrfree(repp->qd);
	return rv;
}

void
tcpproc(void *v)
{
	int len, rv;
	Request req;
	DNSmsg reqmsg, repmsg;
	char *err;
	uchar buf[512];
	char tname[32];
	int fd, rfd;
	NetConnInfo *caller;

	rfd = -1;
	fd = (uintptr)v;
	caller = 0;
	/* loop on requests */
	for(;; putactivity()){
		if (rfd == 1)
			return;
		close(rfd);
		now = time(0);
		memset(&repmsg, 0, sizeof(repmsg));
		if (fd == 0) {
			len = readmsg(fd, buf, sizeof buf);
			rfd = 1;
		} else {
			len = connreadmsg(fd, &rfd, buf, sizeof buf);
		}
		if(len <= 0)
			continue;
		freenetconninfo(caller);
		caller = getnetconninfo(0, fd);
		getactivity(&req);
		req.aborttime = now + 15*Min;
		err = convM2DNS(buf, len, &reqmsg);
		if(err){
			syslog(0, logfile, "server: input error: %s from %I", err, buf);
			continue;
		}
		if(reqmsg.qdcount < 1){
			syslog(0, logfile, "server: no questions from %I", buf);
			continue;
		}
		if(reqmsg.flags & Fresp){
			syslog(0, logfile, "server: reply not request from %I", buf);
			continue;
		}
		if((reqmsg.flags & Omask) != Oquery){
			syslog(0, logfile, "server: op %d from %I", reqmsg.flags & Omask, buf);
			continue;
		}

		if(debug)
			syslog(0, logfile, "%d: serve (%s) %d %s %s",
				req.id, caller ? caller->raddr : 0,
				reqmsg.id,
				reqmsg.qd->owner->name,
				rrname(reqmsg.qd->type, tname, sizeof tname));

		/* loop through each question */
		while(reqmsg.qd){
			if(reqmsg.qd->type == Taxfr){
				if(dnzone(&reqmsg, &repmsg, &req, rfd, caller) < 0)
					break;
			} else {
				dnserver(&reqmsg, &repmsg, &req);
				rv = reply(rfd, &repmsg, &req, caller);
				rrfreelist(repmsg.qd);
				rrfreelist(repmsg.an);
				rrfreelist(repmsg.ns);
				rrfreelist(repmsg.ar);
				if(rv < 0)
					break;
			}
		}

		rrfreelist(reqmsg.qd);
		rrfreelist(reqmsg.an);
		rrfreelist(reqmsg.ns);
		rrfreelist(reqmsg.ar);
	}
}

enum {
	Maxactivetcp = 4
};

static int
tcpannounce(char *mntpt)
{
	int fd;

	USED(mntpt);
	if((fd=announce(tcpaddr, adir)) < 0)
		warning("announce %s: %r", tcpaddr);
	return fd;
}

void
dntcpserver(void *v)
{
	int i, fd;

	while((fd = tcpannounce(v)) < 0)
		sleep(5*1000);

	for(i=0; i<Maxactivetcp; i++)
		proccreate(tcpproc, (void*)(uintptr)fd, STACK);
}
