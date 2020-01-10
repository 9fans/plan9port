enum
{
	FlagJunk = 1<<0,
	FlagNonJunk = 1<<1,
	FlagReplied = 1<<2,
	FlagFlagged = 1<<3,
	FlagDeleted = 1<<4,
	FlagDraft = 1<<5,
	FlagSeen = 1<<6,
	FlagNoInferiors = 1<<7,
	FlagMarked = 1<<8,
	FlagNoSelect = 1<<9,
	FlagUnMarked = 1<<10,
	FlagRecent = 1<<11
};

typedef struct Box Box;
typedef struct Hdr Hdr;
typedef struct Msg Msg;
typedef struct Part Part;

struct Box
{
	char*	name;		/* name of mailbox */
	char*	elem;		/* last element in name */
	uint		ix;			/* index in box[] array */
	uint		id;			/* id shown in file system */
	uint		flags;		/* FlagNoInferiors, etc. */
	uint		time;			/* last update time */
	uint		msgid;		/* last message id used */

	Msg**	msg;			/* array of messages (can have nils) */
	uint		nmsg;

	char*	imapname;	/* name on IMAP server */
	u32int	validity;		/* IMAP validity number */
	uint		uidnext;		/* IMAP expected next uid */
	uint		recent;		/* IMAP first recent message */
	uint		exists;		/* IMAP last message in box */
	uint		maxseen;		/* maximum IMAP uid seen */
	int		mark;
	uint		imapinit;		/* up-to-date w.r.t. IMAP */

	Box*		parent;		/* in tree */
	Box**	sub;
	uint		nsub;
};

struct Hdr
{
	/* LATER: store date as int, reformat for programs */
	/* order known by fs.c */
	char*	date;
	char*	subject;
	char*	from;
	char*	sender;
	char*	replyto;
	char*	to;
	char*	cc;
	char*	bcc;
	char*	inreplyto;
	char*	messageid;
	char*	digest;
};

struct Msg
{
	Box*		box;			/* mailbox containing msg */
	uint		ix;			/* index in box->msg[] array */
	uint		id;			/* id shown in file system */
	uint		imapuid;		/* IMAP uid */
	uint		imapid;		/* IMAP id */
	uint		flags;		/* FlagDeleted etc. */
	uint		date;			/* smtp envelope date */
	uint		size;

	Part**	part;			/* message subparts - part[0] is root */
	uint		npart;
};

struct Part
{
	Msg*	msg;			/* msg containing part */
	uint		ix;			/* index in msg->part[] */
	uint		pix;			/* id in parent->sub[] */
	Part*		parent;		/* parent in structure */
	Part**	sub;			/* children in structure */
	uint		nsub;

	/* order known by fs.c */
	char*	type;	/* e.g., "text/plain" */
	char*	idstr;
	char*	desc;
	char*	encoding;
	char*	charset;
	char*	filename;
	char*	raw;
	char*	rawheader;
	char*	rawbody;
	char*	mimeheader;

	/* order known by fs.c */
	uint		size;
	uint		lines;

	char*	body;
	uint		nbody;
	Hdr*		hdr;			/* RFC822 envelope for message/rfc822 */
};

void		boxinit(void);
Box*		boxbyname(char*);
Box*		boxbyid(uint);
Box*		boxcreate(char*);
void		boxfree(Box*);
Box*		subbox(Box*, char*);
Msg*	msgcreate(Box*);
Part*	partcreate(Msg*, Part*);

void		hdrfree(Hdr*);

Msg*	msgbyid(Box*, uint);
Msg*	msgbyimapuid(Box*, uint, int);
void		msgfree(Msg*);
void		msgplumb(Msg*, int);

Part*		partbyid(Msg*, uint);
Part*		subpart(Part*, uint);
void			partfree(Part*);

extern	Box**	boxes;
extern	uint		nboxes;

extern	Box*		rootbox;
