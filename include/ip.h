#ifndef _IP_H_
#define _IP_H_ 1
#if defined(__cplusplus)
extern "C" { 
#endif

AUTOLIB(ip)
/*
#pragma	src	"/sys/src/libip"
#pragma	lib	"libip.a"
#pragma	varargck	type	"I"	uchar*
#pragma	varargck	type	"V"	uchar*
#pragma	varargck	type	"E"	uchar*
#pragma	varargck	type	"M"	uchar*
*/
enum 
{
	IPaddrlen=	16,
	IPv4addrlen=	4,
	IPv4off=	12,
	IPllen=		4
};

/*
 *  for reading /net/ipifc
 */
typedef struct Ipifc Ipifc;
typedef struct Iplifc Iplifc;
typedef struct Ipv6rp Ipv6rp;

/* local address */
struct Iplifc
{
	Iplifc	*next;

	/* per address on the ip interface */
	uchar	ip[IPaddrlen];
	uchar	mask[IPaddrlen];
	uchar	net[IPaddrlen];		/* ip & mask */
	ulong	preflt;			/* preferred lifetime */
	ulong	validlt;		/* valid lifetime */
};

/* default values, one per stack */
struct Ipv6rp
{
	int	mflag;
	int	oflag;
	int 	maxraint;
	int	minraint;
	int	linkmtu;
	int	reachtime;
	int	rxmitra;
	int	ttl;
	int	routerlt;	
};

/* actual interface */
struct Ipifc
{
	Ipifc	*next;
	Iplifc	*lifc;

	/* per ip interface */
	int	index;			/* number of interface in ipifc dir */
	char	dev[64];
	uchar	ether[6];
	uchar	sendra6;		/* on == send router adv */
	uchar	recvra6;		/* on == rcv router adv */
	int	mtu;
	ulong	pktin;
	ulong	pktout;
	ulong	errin;
	ulong	errout;
	Ipv6rp	rp;
};

/*
 *  user level udp headers
 */
enum 
{
	Udphdrsize=	52	/* size of a Udphdr */
};

typedef struct Udphdr Udphdr;
struct Udphdr
{
	uchar	raddr[IPaddrlen];	/* remote address and port */
	uchar	laddr[IPaddrlen];	/* local address and port */
	uchar	ifcaddr[IPaddrlen];	/* address of ifc message was received from */
	uchar	rport[2];
	uchar	lport[2];
};

uchar*	defmask(uchar*);
void	maskip(uchar*, uchar*, uchar*);
int	eipfmt(Fmt*);
int	isv4(uchar*);
ulong	parseip(uchar*, char*);
ulong	parseipmask(uchar*, char*);
char*	v4parseip(uchar*, char*);
char*	v4parsecidr(uchar*, uchar*, char*);
int	parseether(uchar*, char*);
int	myipaddr(uchar*, char*);
int	myetheraddr(uchar*, char*);
int	equivip(uchar*, uchar*);
long	udpread(int, Udphdr*, void*, long);
long	udpwrite(int, Udphdr*, void*, long);

Ipifc*	readipifc(char*, Ipifc*, int);
void	freeipifc(Ipifc*);

void	hnputv(void*, uvlong);
void	hnputl(void*, uint);
void	hnputs(void*, ushort);
uint	nhgetl(void*);
uvlong	nhgetv(void*);
ushort	nhgets(void*);
ushort	ptclbsum(uchar*, int);

int	v6tov4(uchar*, uchar*);
void	v4tov6(uchar*, uchar*);

#define	ipcmp(x, y) memcmp(x, y, IPaddrlen)
#define	ipmove(x, y) memmove(x, y, IPaddrlen)

extern uchar IPv4bcast[IPaddrlen];
extern uchar IPv4bcastobs[IPaddrlen];
extern uchar IPv4allsys[IPaddrlen];
extern uchar IPv4allrouter[IPaddrlen];
extern uchar IPnoaddr[IPaddrlen];
extern uchar v4prefix[IPaddrlen];
extern uchar IPallbits[IPaddrlen];

#define CLASS(p) ((*(uchar*)(p))>>6)

#if defined(__cplusplus)
}
#endif
#endif
