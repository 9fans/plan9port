typedef struct Addr Addr;
typedef struct Cmd Cmd;
struct Addr
{
	char	type;	/* # (char addr), l (line addr), / ? . $ + - , ; */
	union{
		String	*re;
		Addr	*aleft;		/* left side of , and ; */
	} g;
	Posn	num;
	Addr	*next;			/* or right side of , and ; */
};

#define	are	g.re
#define	left	g.aleft

struct Cmd
{
	Addr	*addr;			/* address (range of text) */
	String	*re;			/* regular expression for e.g. 'x' */
	union{
		Cmd	*cmd;		/* target of x, g, {, etc. */
		String	*text;		/* text of a, c, i; rhs of s */
		Addr	*addr;		/* address for m, t */
	} g;
	Cmd	*next;			/* pointer to next element in {} */
	short	num;
	ushort	flag;			/* whatever */
	ushort	cmdc;			/* command character; 'x' etc. */
};

#define	ccmd	g.cmd
#define	ctext	g.text
#define	caddr	g.addr

typedef struct Cmdtab Cmdtab;
struct Cmdtab {
	ushort	cmdc;		/* command character */
	uchar	text;		/* takes a textual argument? */
	uchar	regexp;		/* takes a regular expression? */
	uchar	addr;		/* takes an address (m or t)? */
	uchar	defcmd;		/* default command; 0==>none */
	uchar	defaddr;	/* default address */
	uchar	count;		/* takes a count e.g. s2/// */
	char	*token;		/* takes text terminated by one of these */
	int	(*fn)(File*, Cmd*);	/* function to call with parse tree */
};
extern Cmdtab cmdtab[];

enum Defaddr{	/* default addresses */
	aNo,
	aDot,
	aAll
};

int	nl_cmd(File*, Cmd*), a_cmd(File*, Cmd*), b_cmd(File*, Cmd*);
int	c_cmd(File*, Cmd*), cd_cmd(File*, Cmd*), d_cmd(File*, Cmd*);
int	D_cmd(File*, Cmd*), e_cmd(File*, Cmd*);
int	f_cmd(File*, Cmd*), g_cmd(File*, Cmd*), i_cmd(File*, Cmd*);
int	k_cmd(File*, Cmd*), m_cmd(File*, Cmd*), n_cmd(File*, Cmd*);
int	p_cmd(File*, Cmd*), q_cmd(File*, Cmd*);
int	s_cmd(File*, Cmd*), u_cmd(File*, Cmd*), w_cmd(File*, Cmd*);
int	x_cmd(File*, Cmd*), X_cmd(File*, Cmd*), plan9_cmd(File*, Cmd*);
int	eq_cmd(File*, Cmd*);


String	*getregexp(int);
Addr	*newaddr(void);
Address	address(Addr*, Address, int);
int	cmdexec(File*, Cmd*);
