#include <u.h>
#include <libc.h>
#include <ip.h>
#include "dat.h"
#include "protos.h"

enum
{
	Maxoptlen=	312-4,

	/* dhcp types */
	Discover=	1,
	Offer=		2,
	Request=	3,
	Decline=	4,
	Ack=		5,
	Nak=		6,
	Release=	7,
	Inform=		8,

	/* bootp option types */
	OBend=			255,
	OBpad=			0,
	OBmask=			1,
	OBtimeoff=		2,
	OBrouter=		3,
	OBtimeserver=		4,
	OBnameserver=		5,
	OBdnserver=		6,
	OBlogserver=		7,
	OBcookieserver=		8,
	OBlprserver=		9,
	OBimpressserver=	10,
	OBrlserver=		11,
	OBhostname=		12,	/* 0xc0 */
	OBbflen=		13,
	OBdumpfile=		14,
	OBdomainname=		15,
	OBswapserver=		16,	/* 0x10 */
	OBrootpath=		17,
	OBextpath=		18,
	OBipforward=		19,
	OBnonlocal=		20,
	OBpolicyfilter=		21,
	OBmaxdatagram=		22,
	OBttl=			23,
	OBpathtimeout=		24,
	OBpathplateau=		25,
	OBmtu=			26,
	OBsubnetslocal=		27,
	OBbaddr=		28,
	OBdiscovermask=		29,
	OBsupplymask=		30,
	OBdiscoverrouter=	31,
	OBrsserver=		32,	/* 0x20 */
	OBstaticroutes=		33,
	OBtrailerencap=		34,
	OBarptimeout=		35,
	OBetherencap=		36,
	OBtcpttl=		37,
	OBtcpka=		38,
	OBtcpkag=		39,
	OBnisdomain=		40,
	OBniserver=		41,
	OBntpserver=		42,
	OBvendorinfo=		43,	/* 0x2b */
	OBnetbiosns=		44,
	OBnetbiosdds=		45,
	OBnetbiostype=		46,
	OBnetbiosscope=		47,
	OBxfontserver=		48,	/* 0x30 */
	OBxdispmanager=		49,
	OBnisplusdomain=	64,	/* 0x40 */
	OBnisplusserver=	65,
	OBhomeagent=		68,
	OBsmtpserver=		69,
	OBpop3server=		70,
	OBnntpserver=		71,
	OBwwwserver=		72,
	OBfingerserver=		73,
	OBircserver=		74,
	OBstserver=		75,
	OBstdaserver=		76,

	/* dhcp options */
	ODipaddr=		50,	/* 0x32 */
	ODlease=		51,
	ODoverload=		52,
	ODtype=			53,	/* 0x35 */
	ODserverid=		54,	/* 0x36 */
	ODparams=		55,	/* 0x37 */
	ODmessage=		56,
	ODmaxmsg=		57,
	ODrenewaltime=		58,
	ODrebindingtime=	59,
	ODvendorclass=		60,
	ODclientid=		61,	/* 0x3d */
	ODtftpserver=		66,
	ODbootfile=		67,

	/* plan9 vendor info options */
	OP9fs=			128,	/* plan9 file servers */
	OP9auth=		129,	/* plan9 auth servers */
};

static void
p_compile(Filter *f)
{
	sysfatal("unknown dhcp field: %s", f->s);
}

/*
 *  convert a byte array to hex
 */
static char
hex(int x)
{
	if(x < 10)
		return x + '0';
	return x - 10 + 'a';
}
static char*
phex(char *p, char *e, char *tag, uchar *o, int n)
{
	p = seprint(p, e, "%s=", tag);

	for(; p+2 < e && n > 0; n--){
		*p++ = hex(*o>>4);
		*p++ = hex(*o & 0xf);
		o++;
	}
	return p;
}

static char*
pstring(char *p, char *e, char *tag, uchar *o, int n)
{
	char msg[256];

	if(n > sizeof msg - 1)
		n = sizeof msg - 1;
	memmove(msg, o, n);
	msg[n] = 0;
	return seprint(p, e, "%s=%s", tag, msg);
}

static char*
pint(char *p, char *e, char *tag, uchar *o, int n)
{
	int x;

	x = *(char*)o++;
	for(; n > 1; n--)
		x = (x<<8)|*o++;
	return seprint(p, e, "%s=%d", tag, x);
}

static char*
puint(char *p, char *e, char *tag, uchar *o, int n)
{
	uint x;

	x = *o++;
	for(; n > 1; n--)
		x = (x<<8)|*o++;
	return seprint(p, e, "%s=%ud", tag, x);
}

static char*
pserver(char *p, char *e, char *tag, uchar *o, int n)
{
	p = seprint(p, e, "%s=(", tag);
	while(n >= 4){
		p = seprint(p, e, " %V", o);
		n -= 4;
		o += 4;
	}
	p = seprint(p, e, ")");
	return p;
}

static char *dhcptype[256] =
{
[Discover]	"Discover",
[Offer]		"Offer",
[Request]	"Request",
[Decline]	"Decline",
[Ack]		"Ack",
[Nak]		"Nak",
[Release]	"Release",
[Inform]	"Inform"
};


static char*
ptype(char *p, char *e, uchar val)
{
	char *x;

	x = dhcptype[val];
	if(x != nil)
		return seprint(p, e, "t=%s", x);
	else
		return seprint(p, e, "t=%d", val);
}

static int
p_seprint(Msg *m)
{
	int i, n, code;
	uchar *o, *ps;
	char *p, *e;
	char msg[64];

	/* no next proto */
	m->pr = nil;

	p = m->p;
	e = m->e;
	ps = m->ps;

	while(ps < m->pe){
		code = *ps++;
		if(code == 255)
			break;
		if(code == 0)
			continue;

		/* ignore anything that's too long */
		n = *ps++;
		o = ps;
		ps += n;
		if(ps > m->pe)
			break;

		switch(code){
		case ODipaddr:	/* requested ip address */
			p = pserver(p, e, "ipaddr", o, n);
			break;
		case ODlease:	/* requested lease time */
			p = pint(p, e, "lease", o, n);
			break;
		case ODtype:
			p = ptype(p, e, *o);
			break;
		case ODserverid:
			p = pserver(p, e, "serverid", o, n);
			break;
		case ODmessage:
			p = pstring(p, e, "message", o, n);
			break;
		case ODmaxmsg:
			p = puint(p, e, "maxmsg", o, n);
			break;
		case ODclientid:
			p = phex(p, e, "clientid", o, n);
			break;
		case ODparams:
			p = seprint(p, e, " requested=(");
			for(i = 0; i < n; i++){
				if(i != 0)
					p = seprint(p, e, " ");
				p = seprint(p, e, "%ud", o[i]);
			}
			p = seprint(p, e, ")");
			break;
		case ODvendorclass:
			p = pstring(p, e, "vendorclass", o, n);
			break;
		case OBmask:
			p = pserver(p, e, "mask", o, n);
			break;
		case OBtimeoff:
			p = pint(p, e, "timeoff", o, n);
			break;
		case OBrouter:
			p = pserver(p, e, "router", o, n);
			break;
		case OBtimeserver:
			p = pserver(p, e, "timesrv", o, n);
			break;
		case OBnameserver:
			p = pserver(p, e, "namesrv", o, n);
			break;
		case OBdnserver:
			p = pserver(p, e, "dnssrv", o, n);
			break;
		case OBlogserver:
			p = pserver(p, e, "logsrv", o, n);
			break;
		case OBcookieserver:
			p = pserver(p, e, "cookiesrv", o, n);
			break;
		case OBlprserver:
			p = pserver(p, e, "lprsrv", o, n);
			break;
		case OBimpressserver:
			p = pserver(p, e, "impresssrv", o, n);
			break;
		case OBrlserver:
			p = pserver(p, e, "rlsrv", o, n);
			break;
		case OBhostname:
			p = pstring(p, e, "hostname", o, n);
			break;
		case OBbflen:
			break;
		case OBdumpfile:
			p = pstring(p, e, "dumpfile", o, n);
			break;
		case OBdomainname:
			p = pstring(p, e, "domname", o, n);
			break;
		case OBswapserver:
			p = pserver(p, e, "swapsrv", o, n);
			break;
		case OBrootpath:
			p = pstring(p, e, "rootpath", o, n);
			break;
		case OBextpath:
			p = pstring(p, e, "extpath", o, n);
			break;
		case OBipforward:
			p = phex(p, e, "ipforward", o, n);
			break;
		case OBnonlocal:
			p = phex(p, e, "nonlocal", o, n);
			break;
		case OBpolicyfilter:
			p = phex(p, e, "policyfilter", o, n);
			break;
		case OBmaxdatagram:
			p = phex(p, e, "maxdatagram", o, n);
			break;
		case OBttl:
			p = puint(p, e, "ttl", o, n);
			break;
		case OBpathtimeout:
			p = puint(p, e, "pathtimeout", o, n);
			break;
		case OBpathplateau:
			p = phex(p, e, "pathplateau", o, n);
			break;
		case OBmtu:
			p = puint(p, e, "mtu", o, n);
			break;
		case OBsubnetslocal:
			p = pserver(p, e, "subnet", o, n);
			break;
		case OBbaddr:
			p = pserver(p, e, "baddr", o, n);
			break;
		case OBdiscovermask:
			p = pserver(p, e, "discovermsak", o, n);
			break;
		case OBsupplymask:
			p = pserver(p, e, "rousupplymaskter", o, n);
			break;
		case OBdiscoverrouter:
			p = pserver(p, e, "discoverrouter", o, n);
			break;
		case OBrsserver:
			p = pserver(p, e, "rsrouter", o, n);
			break;
		case OBstaticroutes:
			p = phex(p, e, "staticroutes", o, n);
			break;
		case OBtrailerencap:
			p = phex(p, e, "trailerencap", o, n);
			break;
		case OBarptimeout:
			p = puint(p, e, "arptimeout", o, n);
			break;
		case OBetherencap:
			p = phex(p, e, "etherencap", o, n);
			break;
		case OBtcpttl:
			p = puint(p, e, "tcpttl", o, n);
			break;
		case OBtcpka:
			p = puint(p, e, "tcpka", o, n);
			break;
		case OBtcpkag:
			p = phex(p, e, "tcpkag", o, n);
			break;
		case OBnisdomain:
			p = pstring(p, e, "nisdomain", o, n);
			break;
		case OBniserver:
			p = pserver(p, e, "nisrv", o, n);
			break;
		case OBntpserver:
			p = pserver(p, e, "ntpsrv", o, n);
			break;
		case OBvendorinfo:
			p = phex(p, e, "vendorinfo", o, n);
			break;
		case OBnetbiosns:
			p = pserver(p, e, "biosns", o, n);
			break;
		case OBnetbiosdds:
			p = phex(p, e, "biosdds", o, n);
			break;
		case OBnetbiostype:
			p = phex(p, e, "biostype", o, n);
			break;
		case OBnetbiosscope:
			p = phex(p, e, "biosscope", o, n);
			break;
		case OBxfontserver:
			p = pserver(p, e, "fontsrv", o, n);
			break;
		case OBxdispmanager:
			p = pserver(p, e, "xdispmgr", o, n);
			break;
		case OBnisplusdomain:
			p = pstring(p, e, "nisplusdomain", o, n);
			break;
		case OBnisplusserver:
			p = pserver(p, e, "nisplussrv", o, n);
			break;
		case OBhomeagent:
			p = pserver(p, e, "homeagent", o, n);
			break;
		case OBsmtpserver:
			p = pserver(p, e, "smtpsrv", o, n);
			break;
		case OBpop3server:
			p = pserver(p, e, "pop3srv", o, n);
			break;
		case OBnntpserver:
			p = pserver(p, e, "ntpsrv", o, n);
			break;
		case OBwwwserver:
			p = pserver(p, e, "wwwsrv", o, n);
			break;
		case OBfingerserver:
			p = pserver(p, e, "fingersrv", o, n);
			break;
		case OBircserver:
			p = pserver(p, e, "ircsrv", o, n);
			break;
		case OBstserver:
			p = pserver(p, e, "stsrv", o, n);
			break;
		case OBstdaserver:
			p = pserver(p, e, "stdasrv", o, n);
			break;
		case OBend:
			goto out;
		default:
			snprint(msg, sizeof msg, " T%ud", code);
			p = phex(p, e, msg, o, n);
			break;
		}
		if(*ps != OBend)
			p = seprint(p, e, " ");
	}
out:
	m->p = p;
	m->ps = ps;
	return 0;
}

Proto dhcp =
{
	"dhcp",
	p_compile,
	nil,
	p_seprint,
	nil,
	nil,
	nil,
	defaultframer
};
