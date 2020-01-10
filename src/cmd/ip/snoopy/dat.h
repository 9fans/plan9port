typedef struct Field Field;
typedef struct Filter Filter;
typedef struct Msg Msg;
typedef struct Mux Mux;
typedef struct Proto Proto;

#define NetS(x) ((((uchar*)x)[0]<<8) | ((uchar*)x)[1])
#define Net3(x) ((((uchar*)x)[0]<<16) | (((uchar*)x)[1]<<8) | ((uchar*)x)[2])
#define NetL(x) (((ulong)((((uchar*)x)[0]<<24) | (((uchar*)x)[1]<<16) | (((uchar*)x)[2]<<8) | ((uchar*)x)[3]))&0xFFFFFFFFU)

#define LittleS(x) ((((uchar*)x)[1]<<8) | ((uchar*)x)[0])
#define LittleL(x) (((ulong)((((uchar*)x)[3]<<24) | (((uchar*)x)[2]<<16) | (((uchar*)x)[1]<<8) | ((uchar*)x)[0]))&0xFFFFFFFFU)

/*
 *  one per protocol module
 */
struct Proto
{
	char*	name;
	void	(*compile)(Filter*);
	int	(*filter)(Filter*, Msg*);
	int	(*seprint)(Msg*);
	Mux*	mux;
	char*	valfmt;
	Field*	field;
	int	(*framer)(int, uchar*, int);
};
extern Proto *protos[];

/*
 *  one per protocol module, pointed to by Proto.mux
 */
struct Mux
{
	char*	name;
	ulong	val;
	Proto*	pr;
};

/*
 *  a field defining a comparison filter
 */
struct Field
{
	char*	name;
	int	ftype;
	int	subop;
	char*	help;
};

/*
 *  the status of the current message walk
 */
struct Msg
{
	uchar	*ps;	/* packet ptr */
	uchar	*pe;	/* packet end */

	char	*p;	/* buffer start */
	char	*e;	/* buffer end */

	int	needroot;	/* pr is root, need to see in expression */
	Proto	*pr;	/* current/next protocol */
};

enum
{
	Fnum,		/* just a number */
	Fether,		/* ethernet address */
	Fv4ip,		/* v4 ip address */
	Fv6ip,		/* v6 ip address */
	Fba,		/* byte array */
};

/*
 *  a node in the filter tree
 */
struct Filter {
	int	op;	/* token type */
	char	*s;	/* string */
	Filter	*l;
	Filter	*r;

	Proto	*pr;	/* next protocol;

	/* protocol specific */
	int	subop;
	ulong	param;
	union {
		ulong	ulv;
		vlong	vlv;
		uchar	a[32];
	};
};

extern void	yyinit(char*);
extern int	yyparse(void);
extern Filter*	newfilter(void);
extern void	compile_cmp(char*, Filter*, Field*);
extern void	demux(Mux*, ulong, ulong, Msg*, Proto*);
extern int	defaultframer(int, uchar*, int);
extern int	opendevice(char*, int);

extern int Nflag;
extern int dflag;
extern int Cflag;

typedef Filter *Filterptr;
#define YYSTYPE Filterptr
extern Filter *filter;
