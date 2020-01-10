#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include <ip.h>
#include <ndb.h>
#include <thread.h>
#include "dns.h"

enum
{
	Maxrequest=		128,
	Ncache=			8,
	Maxpath=		128,
	Maxreply=		512,
	Maxrrr=			16
};

static char *servername;
static RR *serveraddrs;

int	debug;
int	cachedb;
ulong	now;
int	testing;
int traceactivity;
char	*trace;
int	needrefresh;
int	resolver;
uchar	ipaddr[IPaddrlen];	/* my ip address */
int	maxage;
char	*logfile = "dns";
char	*dbfile;
char	mntpt[Maxpath];
char	*zonerefreshprogram;
char *tcpaddr;
char *udpaddr;

int prettyrrfmt(Fmt*);
void preloadserveraddrs(void);
void squirrelserveraddrs(void);
int setserver(char*);
void doquery(char*, char*);
void docmd(int, char**);

void
usage(void)
{
	fprint(2, "usage: dnsdebug [-fr] [query ...]\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	int n;
	Biobuf in;
	char *p;
	char *f[4];

	strcpy(mntpt, "/net");

	ARGBEGIN{
	case 'r':
		resolver = 1;
		break;
	case 'f':
		dbfile = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	now = time(0);
	dninit();
	fmtinstall('R', prettyrrfmt);
	if(myipaddr(ipaddr, mntpt) < 0)
		sysfatal("can't read my ip address");
	opendatabase();

	if(resolver)
		squirrelserveraddrs();

	debug = 1;

	if(argc > 0){
		docmd(argc, argv);
		threadexitsall(0);
	}

	Binit(&in, 0, OREAD);
	for(print("> "); p = Brdline(&in, '\n'); print("> ")){
		p[Blinelen(&in)-1] = 0;
		n = tokenize(p, f, 3);
		if(n<1)
			continue;

		/* flush the cache */
		dnpurge();

		docmd(n, f);

	}
	threadexitsall(0);
}

static char*
longtime(long t)
{
	int d, h, m, n;
	static char x[128];

	for(d = 0; t >= 24*60*60; t -= 24*60*60)
		d++;
	for(h = 0; t >= 60*60; t -= 60*60)
		h++;
	for(m = 0; t >= 60; t -= 60)
		m++;
	n = 0;
	if(d)
		n += sprint(x, "%d day ", d);
	if(h)
		n += sprint(x+n, "%d hr ", h);
	if(m)
		n += sprint(x+n, "%d min ", m);
	if(t || n == 0)
		sprint(x+n, "%ld sec", t);
	return x;
}

int
prettyrrfmt(Fmt *f)
{
	RR *rp;
	char buf[3*Domlen];
	char *p, *e;
	Txt *t;

	rp = va_arg(f->args, RR*);
	if(rp == 0){
		strcpy(buf, "<null>");
		goto out;
	}

	p = buf;
	e = buf + sizeof(buf);
	p = seprint(p, e, "%-32.32s %-15.15s %-5.5s", rp->owner->name,
		longtime(rp->db ? rp->ttl : (rp->ttl-now)),
		rrname(rp->type, buf, sizeof buf));

	if(rp->negative){
		seprint(p, e, "negative rcode %d\n", rp->negrcode);
		goto out;
	}

	switch(rp->type){
	case Thinfo:
		seprint(p, e, "\t%s %s", rp->cpu->name, rp->os->name);
		break;
	case Tcname:
	case Tmb:
	case Tmd:
	case Tmf:
	case Tns:
		seprint(p, e, "\t%s", rp->host->name);
		break;
	case Tmg:
	case Tmr:
		seprint(p, e, "\t%s", rp->mb->name);
		break;
	case Tminfo:
		seprint(p, e, "\t%s %s", rp->mb->name, rp->rmb->name);
		break;
	case Tmx:
		seprint(p, e, "\t%lud %s", rp->pref, rp->host->name);
		break;
	case Ta:
	case Taaaa:
		seprint(p, e, "\t%s", rp->ip->name);
		break;
	case Tptr:
		seprint(p, e, "\t%s", rp->ptr->name);
		break;
	case Tsoa:
		seprint(p, e, "\t%s %s %lud %lud %lud %lud %lud", rp->host->name,
			rp->rmb->name, rp->soa->serial, rp->soa->refresh, rp->soa->retry,
			rp->soa->expire, rp->soa->minttl);
		break;
	case Tnull:
		seprint(p, e, "\t%.*H", rp->null->dlen, rp->null->data);
		break;
	case Ttxt:
		p = seprint(p, e, "\t");
		for(t = rp->txt; t != nil; t = t->next)
			p = seprint(p, e, "%s", t->p);
		break;
	case Trp:
		seprint(p, e, "\t%s %s", rp->rmb->name, rp->rp->name);
		break;
	case Tkey:
		seprint(p, e, "\t%d %d %d", rp->key->flags, rp->key->proto,
			rp->key->alg);
		break;
	case Tsig:
		seprint(p, e, "\t%d %d %d %lud %lud %lud %d %s",
			rp->sig->type, rp->sig->alg, rp->sig->labels, rp->sig->ttl,
			rp->sig->exp, rp->sig->incep, rp->sig->tag, rp->sig->signer->name);
		break;
	case Tcert:
		seprint(p, e, "\t%d %d %d",
			rp->sig->type, rp->sig->tag, rp->sig->alg);
		break;
	default:
		break;
	}
out:
	return fmtstrcpy(f, buf);
}

void
logsection(char *flag, RR *rp)
{
	if(rp == nil)
		return;
	print("\t%s%R\n", flag, rp);
	for(rp = rp->next; rp != nil; rp = rp->next)
		print("\t      %R\n", rp);
}

void
logreply(int id, uchar *addr, DNSmsg *mp)
{
	RR *rp;
	char buf[12];
	char resp[32];

	switch(mp->flags & Rmask){
	case Rok:
		strcpy(resp, "OK");
		break;
	case Rformat:
		strcpy(resp, "Format error");
		break;
	case Rserver:
		strcpy(resp, "Server failed");
		break;
	case Rname:
		strcpy(resp, "Nonexistent");
		break;
	case Runimplimented:
		strcpy(resp, "Unimplemented");
		break;
	case Rrefused:
		strcpy(resp, "Refused");
		break;
	default:
		sprint(resp, "%d", mp->flags & Rmask);
		break;
	}

	print("%d: rcvd %s from %I (%s%s%s%s%s)\n", id, resp, addr,
		mp->flags & Fauth ? "authoritative" : "",
		mp->flags & Ftrunc ? " truncated" : "",
		mp->flags & Frecurse ? " recurse" : "",
		mp->flags & Fcanrec ? " can_recurse" : "",
		mp->flags & (Fauth|Rname) == (Fauth|Rname) ?
		" nx" : "");
	for(rp = mp->qd; rp != nil; rp = rp->next)
		print("\tQ:    %s %s\n", rp->owner->name, rrname(rp->type, buf, sizeof buf));
	logsection("Ans:  ", mp->an);
	logsection("Auth: ", mp->ns);
	logsection("Hint: ", mp->ar);
}

void
logsend(int id, int subid, uchar *addr, char *sname, char *rname, int type)
{
	char buf[12];

	print("%d.%d: sending to %I/%s %s %s\n", id, subid,
		addr, sname, rname, rrname(type, buf, sizeof buf));
}

RR*
getdnsservers(int class)
{
	RR *rr;

	if(servername == nil)
		return dnsservers(class);

	rr = rralloc(Tns);
	rr->owner = dnlookup("local#dns#servers", class, 1);
	rr->host = dnlookup(servername, class, 1);

	return rr;
}

void
squirrelserveraddrs(void)
{
	RR *rr, *rp, **l;
	Request req;

	/* look up the resolver address first */
	resolver = 0;
	debug = 0;
	if(serveraddrs)
		rrfreelist(serveraddrs);
	serveraddrs = nil;
	rr = getdnsservers(Cin);
	l = &serveraddrs;
	for(rp = rr; rp != nil; rp = rp->next){
		if(strcmp(ipattr(rp->host->name), "ip") == 0){
			*l = rralloc(Ta);
			(*l)->owner = rp->host;
			(*l)->ip = rp->host;
			l = &(*l)->next;
			continue;
		}
		req.aborttime = now + 60;	/* don't spend more than 60 seconds */
		*l = dnresolve(rp->host->name, Cin, Ta, &req, 0, 0, Recurse, 0, 0);
		while(*l != nil)
			l = &(*l)->next;
	}
	resolver = 1;
	debug = 1;
}

void
preloadserveraddrs(void)
{
	RR *rp, **l, *first;

	l = &first;
	for(rp = serveraddrs; rp != nil; rp = rp->next){
		rrcopy(rp, l);
		rrattach(first, 1);
	}
}

int
setserver(char *server)
{
	if(servername != nil){
		free(servername);
		servername = nil;
		resolver = 0;
	}
	if(server == nil || *server == 0)
		return 0;
	servername = strdup(server);
	squirrelserveraddrs();
	if(serveraddrs == nil){
		print("can't resolve %s\n", servername);
		resolver = 0;
	} else {
		resolver = 1;
	}
	return resolver ? 0 : -1;
}

void
doquery(char *name, char *tstr)
{
	Request req;
	RR *rr, *rp;
	int len, type;
	char *p, *np;
	int rooted;
	char buf[1024];

	if(resolver)
		preloadserveraddrs();

	/* default to an "ip" request if alpha, "ptr" if numeric */
	if(tstr == nil || *tstr == 0) {
		if(strcmp(ipattr(name), "ip") == 0)
			tstr = "ptr";
		else
			tstr = "ip";
	}

	/* if name end in '.', remove it */
	len = strlen(name);
	if(len > 0 && name[len-1] == '.'){
		rooted = 1;
		name[len-1] = 0;
	} else
		rooted = 0;

	/* inverse queries may need to be permuted */
	strncpy(buf, name, sizeof buf);
	if(strcmp("ptr", tstr) == 0
	&& strstr(name, "IN-ADDR") == 0
	&& strstr(name, "in-addr") == 0){
		for(p = name; *p; p++)
			;
		*p = '.';
		np = buf;
		len = 0;
		while(p >= name){
			len++;
			p--;
			if(*p == '.'){
				memmove(np, p+1, len);
				np += len;
				len = 0;
			}
		}
		memmove(np, p+1, len);
		np += len;
		strcpy(np, "in-addr.arpa");
	}

	/* look it up */
	type = rrtype(tstr);
	if(type < 0){
		print("!unknown type %s\n", tstr);
		return;
	}

	getactivity(&req);
	req.aborttime = now + 60;	/* don't spend more than 60 seconds */
	rr = dnresolve(buf, Cin, type, &req, 0, 0, Recurse, rooted, 0);
	if(rr){
		print("----------------------------\n");
		for(rp = rr; rp; rp = rp->next)
			print("answer %R\n", rp);
		print("----------------------------\n");
	}
	rrfreelist(rr);

	putactivity();
}

void
docmd(int n, char **f)
{
	int tmpsrv;
	char *name, *type;

	name = nil;
	type = nil;
	tmpsrv = 0;

	if(*f[0] == '@') {
		if(setserver(f[0]+1) < 0)
			return;

		switch(n){
		case 3:
			type = f[2];
			/* fall through */
		case 2:
			name = f[1];
			tmpsrv = 1;
			break;
		}
	} else {
		switch(n){
		case 2:
			type = f[1];
			/* fall through */
		case 1:
			name = f[0];
			break;
		}
	}

	if(name == nil)
		return;

	doquery(name, type);

	if(tmpsrv)
		setserver("");
}
