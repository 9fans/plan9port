#define MAXSAME 16
#define MAXSAMECHAR 1024

/* status of a destination*/
typedef enum {
	d_undefined,	/* address has not been matched*/
	d_pipe,		/* repl1|repl2 == delivery command, rep*/
	d_cat,		/* repl1 == mail file */
	d_translate,	/* repl1 == translation command*/
	d_alias,	/* repl1 == translation*/
	d_auth,		/* repl1 == command to authorize*/
	d_syntax,	/* addr contains illegal characters*/
	d_unknown,	/* addr does not match a rewrite rule*/
	d_loop,		/* addressing loop*/
	d_eloop,	/* external addressing loop*/
	d_noforward,	/* forwarding not allowed*/
	d_badmbox,	/* mailbox badly formatted*/
	d_resource,	/* ran out of something we needed*/
	d_pipeto,	/* pipe to from a mailbox*/
} d_status;

/* a destination*/
typedef struct dest dest;
struct dest {
	dest	*next;		/* for chaining*/
	dest	*same;		/* dests with same cmd*/
	dest	*parent;	/* destination we're a translation of*/
	String	*addr;		/* destination address*/
	String	*repl1;		/* substitution field 1*/
	String	*repl2;		/* substitution field 2*/
	int	pstat;		/* process status*/
	d_status status;	/* delivery status*/
	int	authorized;	/* non-zero if we have been authorized*/
	int	nsame;		/* number of same dests chained to this entry*/
	int	nchar;		/* number of characters in the command*/
};

typedef struct message message;
struct message {
	String	*sender;
	String	*replyaddr;
	String	*date;
	String	*body;
	String	*tmp;		/* name of temp file */
	String	*to;
	int	size;
	int	fd;		/* if >= 0, the file the message is stored in*/
	char	haveto;
	String	*havefrom;
	String	*havesender;
	String	*havereplyto;
	char	havedate;
	char	havemime;
	String	*havesubject;
	char	bulk;		/* if Precedence: Bulk in header */
	char	rfc822headers;
	int	received;	/* number of received lines */
	char	*boundary;	/* bondary marker for attachments */
};

/*
 *  exported variables
 */
extern int rmail;
extern int onatty;
extern char *thissys, *altthissys;
extern int xflg;
extern int nflg;
extern int tflg;
extern int debug;
extern int nosummary;

/*
 *  exported procedures
 */
extern void	authorize(dest*);
extern int	cat_mail(dest*, message*);
extern dest	*up_bind(dest*, message*, int);
extern int	ok_to_forward(char*);
extern int	lookup(char*, char*, Biobuf**, char*, Biobuf**);
extern dest	*d_new(String*);
extern void	d_free(dest*);
extern dest	*d_rm(dest**);
extern void	d_insert(dest**, dest*);
extern dest	*d_rm_same(dest**);
extern void	d_same_insert(dest**, dest*);
extern String	*d_to(dest*);
extern dest	*s_to_dest(String*, dest*);
extern void	gateway(message*);
extern dest	*expand_local(dest*);
extern void	logdelivery(dest*, char*, message*);
extern void	loglist(dest*, message*, char*);
extern void	logrefusal(dest*, message*, char*);
extern int	default_from(message*);
extern message	*m_new(void);
extern void	m_free(message*);
extern message	*m_read(Biobuf*, int, int);
extern int	m_get(message*, long, char**);
extern int	m_print(message*, Biobuf*, char*, int);
extern int	m_bprint(message*, Biobuf*);
extern String	*rule_parse(String*, char*, int*);
extern int	getrules(void);
extern int	rewrite(dest*, message*);
extern void	dumprules(void);
extern void	regerror(char*);
extern dest	*translate(dest*);
extern char*	skipequiv(char*);
extern int	refuse(dest*, message*, char*, int, int);
