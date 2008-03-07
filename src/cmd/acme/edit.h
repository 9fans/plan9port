/*#pragma	varargck	argpos	editerror	1*/

typedef struct Addr	Addr;
typedef struct Address	Address;
typedef struct Cmd	Cmd;
typedef struct List	List;
typedef struct String	String;

struct String
{
	int	n;		/* excludes NUL */
	Rune	*r;		/* includes NUL */
	int	nalloc;
};

struct Addr
{
	char	type;	/* # (char addr), l (line addr), / ? . $ + - , ; */
	union{
		String	*re;
		Addr	*left;		/* left side of , and ; */
	} u;
	ulong	num;
	Addr	*next;			/* or right side of , and ; */
};

struct Address
{
	Range	r;
	File	*f;
};

struct Cmd
{
	Addr	*addr;			/* address (range of text) */
	String	*re;			/* regular expression for e.g. 'x' */
	union{
		Cmd	*cmd;		/* target of x, g, {, etc. */
		String	*text;		/* text of a, c, i; rhs of s */
		Addr	*mtaddr;		/* address for m, t */
	} u;
	Cmd	*next;			/* pointer to next element in {} */
	short	num;
	ushort	flag;			/* whatever */
	ushort	cmdc;			/* command character; 'x' etc. */
};

extern struct cmdtab{
	ushort	cmdc;		/* command character */
	uchar	text;		/* takes a textual argument? */
	uchar	regexp;		/* takes a regular expression? */
	uchar	addr;		/* takes an address (m or t)? */
	uchar	defcmd;		/* default command; 0==>none */
	uchar	defaddr;	/* default address */
	uchar	count;		/* takes a count e.g. s2/// */
	char	*token;		/* takes text terminated by one of these */
	int	(*fn)(Text*, Cmd*);	/* function to call with parse tree */
}cmdtab[];

#define	INCR	25	/* delta when growing list */

struct List	/* code depends on a long being able to hold a pointer */
{
	int	nalloc;
	int	nused;
	union{
		void	*listptr;
		void*	*ptr;
		uchar*	*ucharptr;
		String*	*stringptr;
	} u;
};

enum Defaddr{	/* default addresses */
	aNo,
	aDot,
	aAll
};

int	nl_cmd(Text*, Cmd*), a_cmd(Text*, Cmd*), b_cmd(Text*, Cmd*);
int	c_cmd(Text*, Cmd*), d_cmd(Text*, Cmd*);
int	B_cmd(Text*, Cmd*), D_cmd(Text*, Cmd*), e_cmd(Text*, Cmd*);
int	f_cmd(Text*, Cmd*), g_cmd(Text*, Cmd*), i_cmd(Text*, Cmd*);
int	k_cmd(Text*, Cmd*), m_cmd(Text*, Cmd*), n_cmd(Text*, Cmd*);
int	p_cmd(Text*, Cmd*);
int	s_cmd(Text*, Cmd*), u_cmd(Text*, Cmd*), w_cmd(Text*, Cmd*);
int	x_cmd(Text*, Cmd*), X_cmd(Text*, Cmd*), pipe_cmd(Text*, Cmd*);
int	eq_cmd(Text*, Cmd*);

String	*allocstring(int);
void		freestring(String*);
String	*getregexp(int);
Addr	*newaddr(void);
Address	cmdaddress(Addr*, Address, int);
int	cmdexec(Text*, Cmd*);
void	editerror(char*, ...);
int	cmdlookup(int);
void	resetxec(void);
void	Straddc(String*, int);
