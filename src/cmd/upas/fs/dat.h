typedef struct Message Message;
struct Message
{
	int	id;
	int	refs;
	int	subname;
	char	name[Elemlen];

	// pointers into message
	char	*start;		// start of message
	char	*end;		// end of message
	char	*header;	// start of header
	char	*hend;		// end of header
	int	hlen;		// length of header minus ignored fields
	char	*mheader;	// start of mime header
	char	*mhend;		// end of mime header
	char	*body;		// start of body
	char	*bend;		// end of body
	char	*rbody;		// raw (unprocessed) body
	char	*rbend;		// end of raw (unprocessed) body
	char	*lim;
	char	deleted;
	char	inmbox;
	char	mallocd;	// message is malloc'd
	char	ballocd;	// body is malloc'd
	char	hallocd;	// header is malloce'd

	// mail info
	String	*unixheader;
	String	*unixfrom;
	String	*unixdate;
	String	*from822;
	String	*sender822;
	String	*to822;
	String	*bcc822;
	String	*cc822;
	String	*replyto822;
	String	*date822;
	String	*inreplyto822;
	String	*subject822;
	String	*messageid822;
	String	*addrs;
	String	*mimeversion;
	String	*sdigest;

	// mime info
	String	*boundary;
	String	*type;
	int	encoding;
	int	disposition;
	String	*charset;
	String	*filename;
	int	converted;
	int	decoded;
	char	lines[10];	// number of lines in rawbody

	Message	*next;		// same level
	Message	*part;		// down a level
	Message	*whole;		// up a level

	uchar	digest[SHA1dlen];

	vlong	imapuid;	// used by imap4

	char		uidl[80];	// used by pop3
	int		mesgno;
};

enum
{
	// encodings
	Enone=	0,
	Ebase64,
	Equoted,

	// disposition possibilities
	Dnone=	0,
	Dinline,
	Dfile,
	Dignore,

	PAD64=	'='
};

typedef struct Mailbox Mailbox;
struct Mailbox
{
	QLock ql;      /* jpc named Qlock */
	int	refs;
	Mailbox	*next;
	int	id;
	int	dolock;		// lock when syncing?
	int	std;
	char	name[Elemlen];
	char	path[Pathlen];
	Dir	*d;
	Message	*root;
	int	vers;		// goes up each time mailbox is read

	ulong waketime;
	char	*(*sync)(Mailbox*, int);
	void	(*close)(Mailbox*);
	char	*(*fetch)(Mailbox*, Message*);
	char	*(*ctl)(Mailbox*, int, char**);
	void	*aux;		// private to Mailbox implementation
};

typedef char *Mailboxinit(Mailbox*, char*);

extern Message	*root;
extern Mailboxinit	plan9mbox;
extern Mailboxinit	pop3mbox;
extern Mailboxinit	imap4mbox;

char*		syncmbox(Mailbox*, int);
char*		geterrstr(void);
void*		emalloc(ulong);
void*		erealloc(void*, ulong);
Message*	newmessage(Message*);
void		delmessage(Mailbox*, Message*);
void		delmessages(int, char**);
int		newid(void);
void		mailplumb(Mailbox*, Message*, int);
char*		newmbox(char*, char*, int);
void		freembox(char*);
void		logmsg(char*, Message*);
void		msgincref(Message*);
void		msgdecref(Mailbox*, Message*);
void		mboxincref(Mailbox*);
void		mboxdecref(Mailbox*);
void		convert(Message*);
void		decode(Message*);
int		cistrncmp(char*, char*, int);
int		cistrcmp(char*, char*);
int		latin1toutf(char*, char*, char*);
int		windows1257toutf(char*, char*, char*);
int		decquoted(char*, char*, char*);
int		xtoutf(char*, char**, char*, char*);
void		countlines(Message*);
int		headerlen(Message*);
void		parse(Message*, int, Mailbox*, int);
void		parseheaders(Message*, int, Mailbox*, int);
void		parsebody(Message*, Mailbox*);
void		parseunix(Message*);
String*	date822tounix(char*);
int		fidmboxrefs(Mailbox*);
int		hashmboxrefs(Mailbox*);
void		checkmboxrefs(void);

extern int	debug;
extern int	fflag;
extern int	logging;
extern char	user[Elemlen];
extern char	stdmbox[Pathlen];
extern QLock	mbllock;
extern Mailbox	*mbl;
extern char	*mntpt;
extern int	biffing;
extern int	plumbing;
extern char*	Enotme;

enum
{
	/* mail subobjects */
	Qbody,
	Qbcc,
	Qcc,
	Qdate,
	Qdigest,
	Qdisposition,
	Qfilename,
	Qfrom,
	Qheader,
	Qinreplyto,
	Qlines,
	Qmimeheader,
	Qmessageid,
	Qraw,
	Qrawbody,
	Qrawheader,
	Qrawunix,
	Qreplyto,
	Qsender,
	Qsubject,
	Qto,
	Qtype,
	Qunixheader,
	Qinfo,
	Qunixdate,
	Qmax,

	/* other files */
	Qtop,
	Qmbox,
	Qdir,
	Qctl,
	Qmboxctl
};

#define PATH(id, f)	((((id)&0xfffff)<<10) | (f))
#define FILE(p)		((p) & 0x3ff)

/* char *dirtab[]; jpc */

// hash table to aid in name lookup, all files have an entry
typedef struct Hash Hash;
struct Hash {
	Hash	*next;
	char	*name;
	ulong	ppath;
	Qid	qid;
	Mailbox	*mb;
	Message	*m;
};

Hash	*hlook(ulong, char*);
void	henter(ulong, char*, Qid, Message*, Mailbox*);
void	hfree(ulong, char*);

ulong msgallocd, msgfreed;
