#pragma	src	"/sys/src/libip"
#pragma	lib	"libip.a"

enum 
{
	IPaddrlen=	16,
	IPv4addrlen=	4,
	IPv4off=	12,
	IPllen=		4,
};

/*
 *  for reading /net/ipifc
 */
typedef struct Ipifc Ipifc;
typedef struct Ipifcs Ipifcs;

struct Ipifc
{
	char	dev[64];
	uchar	ip[IPaddrlen];
	uchar	mask[IPaddrlen];
	uchar	net[IPaddrlen];		/* ip & mask */
	Ipifc	*next;
};

struct Ipifcs
{
	Ipifc *first;
	Ipifc *last;
};

/*
 *  user level udp headers
 */
enum 
{
	Udphdrsize=	36,	/* size of a Udphdr */
};

typedef struct Udphdr Udphdr;
struct Udphdr
{
	uchar	raddr[IPaddrlen];	/* remote address and port */
	uchar	laddr[IPaddrlen];	/* local address and port */
	uchar	rport[2];
	uchar	lport[2];
};

uchar*	defmask(uchar*);
void	maskip(uchar*, uchar*, uchar*);
int	eipconv(va_list*, Fconv*);
ulong	parseip(uchar*, char*);
ulong	parseipmask(uchar*, char*);
int	parseether(uchar*, char*);
int	myipaddr(uchar*, char*);
int	myetheraddr(uchar*, char*);

void	readipifc(char*, Ipifcs*);

void	hnputl(void*, uint);
void	hnputs(void*, ushort);
uint	nhgetl(void*);
ushort	nhgets(void*);

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
