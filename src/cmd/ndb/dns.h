enum
{
	/* RR types */
	Ta=	1,
	Tns=	2,
	Tmd=	3,
	Tmf=	4,
	Tcname=	5,
	Tsoa=	6,
	Tmb=	7,
	Tmg=	8,
	Tmr=	9,
	Tnull=	10,
	Twks=	11,
	Tptr=	12,
	Thinfo=	13,
	Tminfo=	14,
	Tmx=	15,
	Ttxt=	16,
	Trp=	17,
	Tsig=	24,
	Tkey=	25,
	Taaaa=	28,
	Tcert=	37,

	/* query types (all RR types are also queries) */
	Tixfr=	251,	/* incremental zone transfer */
	Taxfr=	252,	/* zone transfer */
	Tmailb=	253,	/* { Tmb, Tmg, Tmr } */
	Tall=	255,	/* all records */

	/* classes */
	Csym=	0,	/* internal symbols */
	Cin=	1,	/* internet */
	Ccs,		/* CSNET (obsolete) */
	Cch,		/* Chaos net */
	Chs,		/* Hesiod (?) */

	/* class queries (all class types are also queries) */
	Call=	255,	/* all classes */

	/* opcodes */
	Oquery=		0<<11,		/* normal query */
	Oinverse=	1<<11,		/* inverse query */
	Ostatus=	2<<11,		/* status request */
	Onotify=	4<<11,		/* notify slaves of updates */
	Omask=		0xf<<11,	/* mask for opcode */

	/* response codes */
	Rok=		0,
	Rformat=	1,	/* format error */
	Rserver=	2,	/* server failure (e.g. no answer from something) */
	Rname=		3,	/* bad name */
	Runimplimented=	4,	/* unimplemented */
	Rrefused=	5,	/* we don't like you */
	Rmask=		0xf,	/* mask for response */
	Rtimeout=	0x10,	/* timeout sending (for internal use only) */

	/* bits in flag word (other than opcode and response) */
	Fresp=		1<<15,	/* message is a response */
	Fauth=		1<<10,	/* true if an authoritative response */
	Ftrunc=		1<<9,	/* truncated message */
	Frecurse=	1<<8,	/* request recursion */
	Fcanrec=	1<<7,	/* server can recurse */

	Domlen=		256,	/* max domain name length (with NULL) */
	Labellen=	256,	/* max domain label length (with NULL) */
	Strlen=		256,	/* max string length (with NULL) */
	Iplen=		32,	/* max ascii ip address length (with NULL) */

	/* time to live values (in seconds) */
	Min=		60,
	Hour=		60*Min,		/* */
	Day=		24*Hour,	/* Ta, Tmx */
	Week=		7*Day,		/* Tsoa, Tns */
	Year=		52*Week,
	DEFTTL=		Day,

	/* reserved time (can't be timed out earlier) */
	Reserved=	5*Min,

	/* packet sizes */
	Maxudp=		512,	/* maximum bytes per udp message */
	Maxudpin=	2048,	/* maximum bytes per udp message */

	/* length of domain name hash table */
	HTLEN= 		4*1024,

#define 	RRmagic	0xdeadbabe
#define	DNmagic	0xa110a110

	/* parallelism */
	Maxactive=	32
};

typedef struct DN	DN;
typedef struct DNSmsg	DNSmsg;
typedef struct RR	RR;
typedef struct SOA	SOA;
typedef struct Area	Area;
typedef struct Request	Request;
typedef struct Key	Key;
typedef struct Cert	Cert;
typedef struct Sig	Sig;
typedef struct Null	Null;
typedef struct Server	Server;
typedef struct Txt	Txt;

/*
 *  a structure to track a request and any slave process handling it
 */
struct Request
{
	ulong	aborttime;	/* time at which we give up */
	int	id;
};

/*
 *  a domain name
 */
struct DN
{
	DN	*next;		/* hash collision list */
	ulong	magic;
	char	*name;		/* owner */
	RR	*rr;		/* resource records off this name */
	ulong	referenced;	/* time last referenced */
	ulong	lookuptime;	/* last time we tried to get a better value */
	ushort	class;		/* RR class */
	char	refs;		/* for mark and sweep */
	char	nonexistent;	/* true if we get an authoritative nx for this domain */
	ulong	ordinal;
};

/*
 *  security info
 */
struct Key
{
	int	flags;
	int	proto;
	int	alg;
	int	dlen;
	uchar	*data;
};
struct Cert
{
	int	type;
	int	tag;
	int	alg;
	int	dlen;
	uchar	*data;
};
struct Sig
{
	int	type;
	int	alg;
	int	labels;
	ulong	ttl;
	ulong	exp;
	ulong	incep;
	int	tag;
	DN	*signer;
	int	dlen;
	uchar	*data;
};
struct Null
{
	int	dlen;
	uchar	*data;
};

/*
 *  text strings
 */
struct Txt
{
	Txt	*next;
	char	*p;
};

/*
 *  an unpacked resource record
 */
struct RR
{
	RR	*next;
	ulong	magic;
	DN	*owner;		/* domain that owns this resource record */
	uchar	negative;	/* this is a cached negative response */
	ulong	pc;
	ulong	ttl;		/* time to live to be passed on */
	ulong	expire;		/* time this entry expires locally */
	ushort	type;		/* RR type */
	ushort	query;		/* query tyis is in response to */
	uchar	auth;		/* authoritative */
	uchar	db;		/* from database */
	uchar	cached;		/* rr in cache */
	ulong	marker;		/* used locally when scanning rrlists */
/*	union { */
		DN	*negsoaowner;	/* soa for cached negative response */
		DN	*host;	/* hostname - soa, cname, mb, md, mf, mx, ns */
		DN	*cpu;	/* cpu type - hinfo */
		DN	*mb;	/* mailbox - mg, minfo */
		DN	*ip;	/* ip addrss - a */
		DN	*rp;	/* rp arg - rp */
		int	cruftlen;
		ulong	arg0;
/*	}; */
/*	union { */
		int	negrcode;	/* response code for cached negative response */
		DN	*rmb;	/* responsible maibox - minfo, soa, rp */
		DN	*ptr;	/* pointer to domain name - ptr */
		DN	*os;	/* operating system - hinfo */
		ulong	pref;	/* preference value - mx */
		ulong	local;	/* ns served from local database - ns */
		ulong	arg1;
/*	}; */
/*	union { */
		SOA	*soa;	/* soa timers - soa */
		Key	*key;
		Cert	*cert;
		Sig	*sig;
		Null	*null;
		Txt	*txt;
/*	}; */
};

/*
 *  list of servers
 */
struct Server
{
	Server	*next;
	char	*name;
};

/*
 *  timers for a start of authenticated record
 */
struct SOA
{
	ulong	serial;		/* zone serial # (sec) - soa */
	ulong	refresh;	/* zone refresh interval (sec) - soa */
	ulong	retry;		/* zone retry interval (sec) - soa */
	ulong	expire;		/* time to expiration (sec) - soa */
	ulong	minttl;		/* minimum time to live for any entry (sec) - soa */
	Server	*slaves;	/* slave servers */
};

/*
 *  domain messages
 */
struct DNSmsg
{
	ushort	id;
	int	flags;
	int	qdcount;	/* questions */
	RR 	*qd;
	int	ancount;	/* answers */
	RR	*an;
	int	nscount;	/* name servers */
	RR	*ns;
	int	arcount;	/* hints */
	RR	*ar;
};

/*
 *  definition of local area for dblookup
 */
struct Area
{
	Area		*next;

	int		len;		/* strlen(area->soarr->owner->name) */
	RR		*soarr;		/* soa defining this area */
	int		neednotify;
	int		needrefresh;
};

enum
{
	Recurse,
	Dontrecurse,
	NOneg,
	OKneg
};

enum
{
	STACK = 32*1024
};

/* dn.c */
extern char	*rrtname[];
extern char	*rname[];
extern void	db2cache(int);
extern void	dninit(void);
extern DN*	dnlookup(char*, int, int);
extern void	dnage(DN*);
extern void	dnageall(int);
extern void	dnagedb(void);
extern void	dnauthdb(void);
extern void	dnget(void);
extern void	dnpurge(void);
extern void	dnput(void);
extern Area*	inmyarea(char*);
extern void	rrattach(RR*, int);
extern RR*	rralloc(int);
extern void	rrfree(RR*);
extern void	rrfreelist(RR*);
extern RR*	rrlookup(DN*, int, int);
extern RR*	rrcat(RR**, RR*);
extern RR**	rrcopy(RR*, RR**);
extern RR*	rrremneg(RR**);
extern RR*	rrremtype(RR**, int);
extern int	rrfmt(Fmt*);
extern int	rravfmt(Fmt*);
extern int	rrsupported(int);
extern int	rrtype(char*);
extern char*	rrname(int, char*, int);
extern int	tsame(int, int);
extern void	dndump(char*);
extern int	getactivity(Request*);
extern void	putactivity(void);
extern void	warning(char*, ...);
extern void	dncheck(void*, int);
extern void	unique(RR*);
extern int	subsume(char*, char*);
extern RR*	randomize(RR*);
extern void*	emalloc(int);
extern char*	estrdup(char*);
extern void	dnptr(uchar*, uchar*, char*, int, int);
extern void	addserver(Server**, char*);
extern Server*	copyserverlist(Server*);
extern void	freeserverlist(Server*);

/* dnarea.c */
extern void	refresh_areas(Area*);
extern void	freearea(Area**);
extern void	addarea(DN *dp, RR *rp, Ndbtuple *t);

/* dblookup.c */
extern RR*	dblookup(char*, int, int, int, int);
extern RR*	dbinaddr(DN*, int);
extern int	baddelegation(RR*, RR*, uchar*);
extern RR*	dnsservers(int);
extern RR*	domainlist(int);
extern int	opendatabase(void);

/* dns.c */
extern char*	walkup(char*);
extern RR*	getdnsservers(int);
extern void	logreply(int, uchar*, DNSmsg*);
extern void	logsend(int, int, uchar*, char*, char*, int);

/* dnresolve.c */
extern RR*	dnresolve(char*, int, int, Request*, RR**, int, int, int, int*);
extern int	udpport(void);
extern int	mkreq(DN *dp, int type, uchar *buf, int flags, ushort reqno);

/* dnserver.c */
extern void	dnserver(DNSmsg*, DNSmsg*, Request*);
extern void	dnudpserver(void*);
extern void	dntcpserver(void*);
extern void	tcpproc(void*);

/* dnnotify.c */
extern void	dnnotify(DNSmsg*, DNSmsg*, Request*);
extern void	notifyproc(void*);

/* convDNS2M.c */
extern int	convDNS2M(DNSmsg*, uchar*, int);

/* convM2DNS.c */
extern char*	convM2DNS(uchar*, int, DNSmsg*);

/* malloc.c */
extern void	mallocsanity(void*);
extern void	lasthist(void*, int, ulong);

/* runproc.c */
extern Waitmsg*	runproc(char*, char**, int);
extern Waitmsg*	runprocfd(char*, char**, int[3]);

extern int	debug;
extern int	traceactivity;
extern char	*trace;
extern int	testing;	/* test cache whenever removing a DN */
extern int	cachedb;
extern int	needrefresh;
extern char	*dbfile;
extern char	mntpt[];
extern char	*logfile;
extern int	resolver;
extern int	maxage;		/* age of oldest entry in cache (secs) */
extern char	*zonerefreshprogram;
extern int	sendnotifies;
extern ulong	now;		/* time base */
extern Area	*owned;
extern Area	*delegated;

extern char	*udpaddr;
extern char	*tcpaddr;

#ifdef VARARGCK
#pragma	varargck	type	"R"	RR*
#pragma	varargck	type	"Q"	RR*
#endif
