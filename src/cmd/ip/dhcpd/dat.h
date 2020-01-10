#include "../dhcp.h"

enum
{
	Maxstr=	256
};

typedef struct Binding Binding;
struct Binding
{
	Binding *next;
	uchar	ip[IPaddrlen];

	char	*boundto;	/* id last bound to */
	char	*offeredto;	/* id we've offered this to */

	long	lease;		/* absolute time at which binding expires */
	long	expoffer;	/* absolute time at which offer times out */
	long	offer;		/* lease offered */
	long	lasttouched;	/* time this entry last assigned/unassigned */
	long	lastcomplained;	/* last time we complained about a used but not leased */
	long	tried;		/* last time we tried this entry */

	Qid	q;		/* qid at the last syncbinding */
};

typedef struct Info	Info;
struct Info
{
	int	indb;			/* true if found in database */
	char	domain[Maxstr];	/* system domain name */
	char	bootf[Maxstr];		/* boot file */
	char	bootf2[Maxstr];	/* alternative boot file */
	uchar	tftp[NDB_IPlen];	/* ip addr of tftp server */
	uchar	tftp2[NDB_IPlen];	/* ip addr of alternate server */
	uchar	ipaddr[NDB_IPlen];	/* ip address of system */
	uchar	ipmask[NDB_IPlen];	/* ip network mask */
	uchar	ipnet[NDB_IPlen];	/* ip network address (ipaddr & ipmask) */
	uchar	etheraddr[6];		/* ethernet address */
	uchar	gwip[NDB_IPlen];	/* gateway ip address */
	uchar	fsip[NDB_IPlen];	/* file system ip address */
	uchar	auip[NDB_IPlen];	/* authentication server ip address */
	char	rootpath[Maxstr];	/* rootfs for diskless nfs clients */
	char	dhcpgroup[Maxstr];
	char	vendor[Maxstr];	/* vendor info */
};


/* from dhcp.c */
extern int	validip(uchar*);
extern void	warning(int, char*, ...);
extern int	minlease;

/* from db.c */
extern char*	tohex(char*, uchar*, int);
extern char*	toid(uchar*, int);
extern void	initbinding(uchar*, int);
extern Binding*	iptobinding(uchar*, int);
extern Binding*	idtobinding(char*, Info*, int);
extern Binding*	idtooffer(char*, Info*);
extern int	commitbinding(Binding*);
extern int	releasebinding(Binding*, char*);
extern int	samenet(uchar *ip, Info *iip);
extern void	mkoffer(Binding*, char*, long);
extern int	syncbinding(Binding*, int);

/* from ndb.c */
extern int	lookup(Bootp*, Info*, Info*);
extern int	lookupip(uchar*, Info*, int);
extern void	lookupname(char*, Ndbtuple*);
extern Iplifc*	findlifc(uchar*);
extern int	forme(uchar*);
extern int	lookupserver(char*, uchar**, Ndbtuple *t);
extern Ndbtuple* lookupinfo(uchar *ipaddr, char **attr, int n);

/* from icmp.c */
extern int	icmpecho(uchar*);

extern char	*binddir;
extern int	debug;
extern char	*blog;
extern Ipifc	*ipifcs;
extern long	now;
extern char	*ndbfile;
