#include <u.h>
#include <libc.h>
#include <ip.h>
#include <bio.h>
#include <ndb.h>
#include <thread.h>
#include "dns.h"

enum
{
	Maxpath=		128
};

char	*logfile = "dns";
char	*dbfile;
int	debug;
int	cachedb = 1;
int	testing;
int	traceactivity;
int	needrefresh;
int 	resolver;
char	mntpt[Maxpath];
ulong	now;
int	maxage;
uchar	ipaddr[IPaddrlen];	/* my ip address */
char	*LOG;
char	*zonerefreshprogram;
char	*tcpaddr;
char	*udpaddr;

void
usage(void)
{
	fprint(2, "usage: dnstcp [-dr] [-f dbfile]\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	ARGBEGIN{
	default:
		usage();
	case 'd':
		debug++;
		break;
	case 'f':
		dbfile = EARGF(usage());
		break;
	case 'r':
		resolver = 1;
		break;
	}ARGEND

	if(argc)
		usage();

	if(debug < 2)
		debug = 0;

	dninit();

	db2cache(1);
	tcpproc(0);
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
