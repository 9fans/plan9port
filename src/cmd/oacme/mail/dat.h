typedef struct Event Event;
typedef struct Exec Exec;
typedef struct Message Message;
typedef struct Window Window;

enum
{
	STACK		= 8192,
	EVENTSIZE	= 256,
	NEVENT		= 5
};

struct Event
{
	int	c1;
	int	c2;
	int	q0;
	int	q1;
	int	flag;
	int	nb;
	int	nr;
	char	b[EVENTSIZE*UTFmax+1];
	Rune	r[EVENTSIZE+1];
};

struct Window
{
	/* coordinate wineventproc and window thread */
	QLock	lk;
	int		ref;

	/* file descriptors */
	CFid*		ctl;
	CFid*		event;
	CFid*		addr;
	CFid*		data;
	CFid*		body;

	/* event input */
	char		buf[512];
	char		*bufp;
	int		nbuf;
	Event	e[NEVENT];

	int		id;
	int		open;
	Channel	*cevent;
};

struct Message
{
	Window	*w;
	CFid*	ctlfd;
	char		*name;
	char		*replyname;
	uchar	opened;
	uchar	dirty;
	uchar	isreply;
	uchar	deleted;
	uchar	writebackdel;
	uchar	tagposted;
	uchar	recursed;
	uchar	level;
	uint		replywinid;

	/* header info */
	char		*from;
	char		*fromcolon;
	char		*to;
	char		*cc;
	char		*replyto;
	char		*sender;
	char		*date;
	char		*subject;
	char		*type;
	char		*disposition;
	char		*filename;
	char		*digest;

	Message	*next;	/* next in this mailbox */
	Message	*prev;	/* prev in this mailbox */
	Message	*head;	/* first subpart */
	Message	*tail;		/* last subpart */
};

enum
{
	NARGS		= 100,
	NARGCHAR	= 8*1024,
	EXECSTACK 	= STACK+(NARGS+1)*sizeof(char*)+NARGCHAR
};

struct Exec
{
	char		*prog;
	char		**argv;
	int		p[2];	/* p[1] is write to program; p[0] set to prog fd 0*/
	int		q[2];	/* q[0] is read from program; q[1] set to prog fd 1 */
	Channel	*sync;
};

extern	Window*	newwindow(void);
extern	CFid*	winopenfile(Window*, char*);
extern	void		winopenbody(Window*, int);
extern	void		winclosebody(Window*);
extern	void		wintagwrite(Window*, char*, int);
extern	void		winname(Window*, char*);
extern	void		winwriteevent(Window*, Event*);
extern	void		winread(Window*, uint, uint, char*);
extern	int		windel(Window*, int);
extern	void		wingetevent(Window*, Event*);
extern	void		wineventproc(void*);
extern	void		winwritebody(Window*, char*, int);
extern	void		winclean(Window*);
extern	int		winselect(Window*, char*, int);
extern	char*	winselection(Window*);
extern	int		winsetaddr(Window*, char*, int);
extern	char*	winreadbody(Window*, int*);
extern	void		windormant(Window*);
extern	void		winsetdump(Window*, char*, char*);
extern	void		winincref(Window*);
extern	void		windecref(Window*);

extern	void		readmbox(Message*, char*, char*);
extern	void		rewritembox(Window*, Message*);

extern	void		mkreply(Message*, char*, char*, Plumbattr*, char*);
extern	void		delreply(Message*);

extern	int		mesgadd(Message*, char*, Dir*, char*);
extern	void		mesgmenu(Window*, Message*);
extern	void		mesgmenunew(Window*, Message*);
extern	int		mesgopen(Message*, char*, char*, Message*, int, char*);
extern	void		mesgctl(void*);
extern	void		mesgsend(Message*);
extern	void		mesgdel(Message*, Message*);
extern	void		mesgmenudel(Window*, Message*, Message*);
extern	void		mesgmenumark(Window*, char*, char*);
extern	void		mesgmenumarkdel(Window*, Message*, Message*, int);
extern	Message*	mesglookup(Message*, char*, char*);
extern	Message*	mesglookupfile(Message*, char*, char*);
extern	void		mesgfreeparts(Message*);
extern	int		mesgcommand(Message*, char*);

extern	char*	info(Message*, int, int);
extern	char*	readfile(char*, char*, int*);
extern	char*	readbody(char*, char*, int*);
extern	void		ctlprint(CFid*, char*, ...);
extern	void*	emalloc(uint);
extern	void*	erealloc(void*, uint);
extern	char*	estrdup(char*);
extern	char*	estrstrdup(char*, char*);
extern	char*	egrow(char*, char*, char*);
extern	char*	eappend(char*, char*, char*);
extern	void		error(char*, ...);
extern	int		tokenizec(char*, char**, int, char*);
extern	void		execproc(void*);
extern	int		fsprint(CFid*, char*, ...);

#pragma	varargck	argpos	error	1
#pragma	varargck	argpos	ctlprint	2

extern	Window	*wbox;
extern	Message	mbox;
extern	Message	replies;
extern	char		*fsname;
extern	CFid		*plumbsendfd;
extern	CFid		*plumbseemailfd;
extern	char		*home;
extern	char		*outgoing;
extern	char		*mailboxdir;
extern	char		*mboxname;
extern	char		*user;
extern	char		*srvname;
extern	char		deleted[];
extern	int		wctlfd;
extern	int		shortmenu;

extern	CFsys	*mailfs;
extern	CFsys	*acmefs;
