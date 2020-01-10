#include "common.h"
#include <ctype.h>
#include <plumb.h>
#include <libsec.h>
#include <thread.h>
#include "dat.h"

extern char* dirtab[]; /* jpc */

typedef struct Header Header;

struct Header {
	char *type;
	void (*f)(Message*, Header*, char*);
	int len;
};

/* headers */
static	void	ctype(Message*, Header*, char*);
static	void	cencoding(Message*, Header*, char*);
static	void	cdisposition(Message*, Header*, char*);
static	void	date822(Message*, Header*, char*);
static	void	from822(Message*, Header*, char*);
static	void	to822(Message*, Header*, char*);
static	void	sender822(Message*, Header*, char*);
static	void	replyto822(Message*, Header*, char*);
static	void	subject822(Message*, Header*, char*);
static	void	inreplyto822(Message*, Header*, char*);
static	void	cc822(Message*, Header*, char*);
static	void	bcc822(Message*, Header*, char*);
static	void	messageid822(Message*, Header*, char*);
static	void	mimeversion(Message*, Header*, char*);
static	void	nullsqueeze(Message*);
enum
{
	Mhead=	11,	/* offset of first mime header */
};

Header head[] =
{
	{ "date:", date822, },
	{ "from:", from822, },
	{ "to:", to822, },
	{ "sender:", sender822, },
	{ "reply-to:", replyto822, },
	{ "subject:", subject822, },
	{ "cc:", cc822, },
	{ "bcc:", bcc822, },
	{ "in-reply-to:", inreplyto822, },
	{ "mime-version:", mimeversion, },
	{ "message-id:", messageid822, },

[Mhead]	{ "content-type:", ctype, },
	{ "content-transfer-encoding:", cencoding, },
	{ "content-disposition:", cdisposition, },
	{ 0, }
};

/* static	void	fatal(char *fmt, ...); jpc */
static	void	initquoted(void);
/* static	void	startheader(Message*);
static	void	startbody(Message*); jpc */
static	char*	skipwhite(char*);
static	char*	skiptosemi(char*);
static	char*	getstring(char*, String*, int);
static	void	setfilename(Message*, char*);
/* static	char*	lowercase(char*); jpc */
static	int	is8bit(Message*);
static	int	headerline(char**, String*);
static	void	initheaders(void);
static void	parseattachments(Message*, Mailbox*);

int		debug;

char *Enotme = "path not served by this file server";

enum
{
	Chunksize = 1024
};

Mailboxinit *boxinit[] = {
	imap4mbox,
	pop3mbox,
	plan9mbox
};

char*
syncmbox(Mailbox *mb, int doplumb)
{
	return (*mb->sync)(mb, doplumb);
}

/* create a new mailbox */
char*
newmbox(char *path, char *name, int std)
{
	Mailbox *mb, **l;
	char *p, *rv;
	int i;

	initheaders();

	mb = emalloc(sizeof(*mb));
	strncpy(mb->path, path, sizeof(mb->path)-1);
	if(name == nil){
		p = strrchr(path, '/');
		if(p == nil)
			p = path;
		else
			p++;
		if(*p == 0){
			free(mb);
			return "bad mbox name";
		}
		strncpy(mb->name, p, sizeof(mb->name)-1);
	} else {
		strncpy(mb->name, name, sizeof(mb->name)-1);
	}

	rv = nil;
	/* check for a mailbox type */
	for(i=0; i<nelem(boxinit); i++)
		if((rv = (*boxinit[i])(mb, path)) != Enotme)
			break;
	if(i == nelem(boxinit)){
		free(mb);
		return "bad path";
	}

	/* on error, give up */
	if(rv){
		free(mb);
		return rv;
	}

	/* make sure name isn't taken */
	qlock(&mbllock);
	for(l = &mbl; *l != nil; l = &(*l)->next){
		if(strcmp((*l)->name, mb->name) == 0){
			if(strcmp(path, (*l)->path) == 0)
				rv = nil;
			else
				rv = "mbox name in use";
			if(mb->close)
				(*mb->close)(mb);
			free(mb);
			qunlock(&mbllock);
			return rv;
		}
	}

	/* always try locking */
	mb->dolock = 1;

	mb->refs = 1;
	mb->next = nil;
	mb->id = newid();
	mb->root = newmessage(nil);
	mb->std = std;
	*l = mb;
	qunlock(&mbllock);

	qlock(&mb->ql);
	if(mb->ctl){
		henter(PATH(mb->id, Qmbox), "ctl",
			(Qid){PATH(mb->id, Qmboxctl), 0, QTFILE}, nil, mb);
	}
	rv = syncmbox(mb, 0);
	qunlock(&mb->ql);

	return rv;
}

/* close the named mailbox */
void
freembox(char *name)
{
	Mailbox **l, *mb;

	qlock(&mbllock);
	for(l=&mbl; *l != nil; l=&(*l)->next){
		if(strcmp(name, (*l)->name) == 0){
			mb = *l;
			*l = mb->next;
			mboxdecref(mb);
			break;
		}
	}
	hfree(PATH(0, Qtop), name);
	qunlock(&mbllock);
}

static void
initheaders(void)
{
	Header *h;
	static int already;

	if(already)
		return;
	already = 1;

	for(h = head; h->type != nil; h++)
		h->len = strlen(h->type);
}

/*
 *  parse a Unix style header
 */
void
parseunix(Message *m)
{
	char *p;
	String *h;

	h = s_new();
	for(p = m->start + 5; *p && *p != '\r' && *p != '\n'; p++)
		s_putc(h, *p);
	s_terminate(h);
	s_restart(h);

	m->unixfrom = s_parse(h, s_reset(m->unixfrom));
	m->unixdate = s_append(s_reset(m->unixdate), h->ptr);

	s_free(h);
}

/*
 *  parse a message
 */
void
parseheaders(Message *m, int justmime, Mailbox *mb, int addfrom)
{
	String *hl;
	Header *h;
	char *p, *q;
	int i;

	if(m->whole == m->whole->whole){
		henter(PATH(mb->id, Qmbox), m->name,
			(Qid){PATH(m->id, Qdir), 0, QTDIR}, m, mb);
	} else {
		henter(PATH(m->whole->id, Qdir), m->name,
			(Qid){PATH(m->id, Qdir), 0, QTDIR}, m, mb);
	}
	for(i = 0; i < Qmax; i++)
		henter(PATH(m->id, Qdir), dirtab[i],
			(Qid){PATH(m->id, i), 0, QTFILE}, m, mb);

	/* parse mime headers */
	p = m->header;
	hl = s_new();
	while(headerline(&p, hl)){
		if(justmime)
			h = &head[Mhead];
		else
			h = head;
		for(; h->type; h++){
			if(cistrncmp(s_to_c(hl), h->type, h->len) == 0){
				(*h->f)(m, h, s_to_c(hl));
				break;
			}
		}
		s_reset(hl);
	}
	s_free(hl);

	/* the blank line isn't really part of the body or header */
	if(justmime){
		m->mhend = p;
		m->hend = m->header;
	} else {
		m->hend = p;
	}
	if(*p == '\n')
		p++;
	m->rbody = m->body = p;

	/* if type is text, get any nulls out of the body.  This is */
	/* for the two seans and imap clients that get confused. */
	if(strncmp(s_to_c(m->type), "text/", 5) == 0)
		nullsqueeze(m);

	/* */
	/* cobble together Unix-style from line */
	/* for local mailbox messages, we end up recreating the */
	/* original header. */
	/* for pop3 messages, the best we can do is  */
	/* use the From: information and the RFC822 date. */
	/* */
	if(m->unixdate == nil || strcmp(s_to_c(m->unixdate), "???") == 0
	|| strcmp(s_to_c(m->unixdate), "Thu Jan 1 00:00:00 GMT 1970") == 0){
		if(m->unixdate){
			s_free(m->unixdate);
			m->unixdate = nil;
		}
		/* look for the date in the first Received: line. */
		/* it's likely to be the right time zone (it's */
	 	/* the local system) and in a convenient format. */
		if(cistrncmp(m->header, "received:", 9)==0){
			if((q = strchr(m->header, ';')) != nil){
				p = q;
				while((p = strchr(p, '\n')) != nil){
					if(p[1] != ' ' && p[1] != '\t' && p[1] != '\n')
						break;
					p++;
				}
				if(p){
					*p = '\0';
					m->unixdate = date822tounix(q+1);
					*p = '\n';
				}
			}
		}

		/* fall back on the rfc822 date	 */
		if(m->unixdate==nil && m->date822)
			m->unixdate = date822tounix(s_to_c(m->date822));
	}

	if(m->unixheader != nil)
		s_free(m->unixheader);

	/* only fake header for top-level messages for pop3 and imap4 */
	/* clients (those protocols don't include the unix header). */
	/* adding the unix header all the time screws up mime-attached */
	/* rfc822 messages. */
	if(!addfrom && !m->unixfrom){
		m->unixheader = nil;
		return;
	}

	m->unixheader = s_copy("From ");
	if(m->unixfrom && strcmp(s_to_c(m->unixfrom), "???") != 0)
		s_append(m->unixheader, s_to_c(m->unixfrom));
	else if(m->from822)
		s_append(m->unixheader, s_to_c(m->from822));
	else
		s_append(m->unixheader, "???");

	s_append(m->unixheader, " ");
	if(m->unixdate)
		s_append(m->unixheader, s_to_c(m->unixdate));
	else
		s_append(m->unixheader, "Thu Jan  1 00:00:00 GMT 1970");

	s_append(m->unixheader, "\n");
}

String*
promote(String **sp)
{
	String *s;

	if(*sp != nil)
		s = s_clone(*sp);
	else
		s = nil;
	return s;
}

void
parsebody(Message *m, Mailbox *mb)
{
	Message *nm;

	/* recurse */
	if(strncmp(s_to_c(m->type), "multipart/", 10) == 0){
		parseattachments(m, mb);
	} else if(strcmp(s_to_c(m->type), "message/rfc822") == 0){
		decode(m);
		parseattachments(m, mb);
		nm = m->part;

		/* promote headers */
		if(m->replyto822 == nil && m->from822 == nil && m->sender822 == nil){
			m->from822 = promote(&nm->from822);
			m->to822 = promote(&nm->to822);
			m->date822 = promote(&nm->date822);
			m->sender822 = promote(&nm->sender822);
			m->replyto822 = promote(&nm->replyto822);
			m->subject822 = promote(&nm->subject822);
			m->unixdate = promote(&nm->unixdate);
		}
	}
}

void
parse(Message *m, int justmime, Mailbox *mb, int addfrom)
{
	parseheaders(m, justmime, mb, addfrom);
	parsebody(m, mb);
}

static void
parseattachments(Message *m, Mailbox *mb)
{
	Message *nm, **l;
	char *p, *x;

	/* if there's a boundary, recurse... */
	if(m->boundary != nil){
		p = m->body;
		nm = nil;
		l = &m->part;
		for(;;){
			x = strstr(p, s_to_c(m->boundary));

			/* no boundary, we're done */
			if(x == nil){
				if(nm != nil)
					nm->rbend = nm->bend = nm->end = m->bend;
				break;
			}

			/* boundary must be at the start of a line */
			if(x != m->body && *(x-1) != '\n'){
				p = x+1;
				continue;
			}

			if(nm != nil)
				nm->rbend = nm->bend = nm->end = x;
			x += strlen(s_to_c(m->boundary));

			/* is this the last part? ignore anything after it */
			if(strncmp(x, "--", 2) == 0)
				break;

			p = strchr(x, '\n');
			if(p == nil)
				break;
			nm = newmessage(m);
			nm->start = nm->header = nm->body = nm->rbody = ++p;
			nm->mheader = nm->header;
			*l = nm;
			l = &nm->next;
		}
		for(nm = m->part; nm != nil; nm = nm->next)
			parse(nm, 1, mb, 0);
		return;
	}

	/* if we've got an rfc822 message, recurse... */
	if(strcmp(s_to_c(m->type), "message/rfc822") == 0){
		nm = newmessage(m);
		m->part = nm;
		nm->start = nm->header = nm->body = nm->rbody = m->body;
		nm->end = nm->bend = nm->rbend = m->bend;
		parse(nm, 0, mb, 0);
	}
}

/*
 *  pick up a header line
 */
static int
headerline(char **pp, String *hl)
{
	char *p, *x;

	s_reset(hl);
	p = *pp;
	x = strpbrk(p, ":\n");
	if(x == nil || *x == '\n')
		return 0;
	for(;;){
		x = strchr(p, '\n');
		if(x == nil)
			x = p + strlen(p);
		s_nappend(hl, p, x-p);
		p = x;
		if(*p != '\n' || *++p != ' ' && *p != '\t')
			break;
		while(*p == ' ' || *p == '\t')
			p++;
		s_putc(hl, ' ');
	}
	*pp = p;
	return 1;
}

static String*
addr822(char *p)
{
	String *s, *list;
	int incomment, addrdone, inanticomment, quoted;
	int n;
	int c;

	list = s_new();
	s = s_new();
	quoted = incomment = addrdone = inanticomment = 0;
	n = 0;
	for(; *p; p++){
		c = *p;

		/* whitespace is ignored */
		if(!quoted && isspace(c) || c == '\r')
			continue;

		/* strings are always treated as atoms */
		if(!quoted && c == '"'){
			if(!addrdone && !incomment)
				s_putc(s, c);
			for(p++; *p; p++){
				if(!addrdone && !incomment)
					s_putc(s, *p);
				if(!quoted && *p == '"')
					break;
				if(*p == '\\')
					quoted = 1;
				else
					quoted = 0;
			}
			if(*p == 0)
				break;
			quoted = 0;
			continue;
		}

		/* ignore everything in an expicit comment */
		if(!quoted && c == '('){
			incomment = 1;
			continue;
		}
		if(incomment){
			if(!quoted && c == ')')
				incomment = 0;
			quoted = 0;
			continue;
		}

		/* anticomments makes everything outside of them comments */
		if(!quoted && c == '<' && !inanticomment){
			inanticomment = 1;
			s = s_reset(s);
			continue;
		}
		if(!quoted && c == '>' && inanticomment){
			addrdone = 1;
			inanticomment = 0;
			continue;
		}

		/* commas separate addresses */
		if(!quoted && c == ',' && !inanticomment){
			s_terminate(s);
			addrdone = 0;
			if(n++ != 0)
				s_append(list, " ");
			s_append(list, s_to_c(s));
			s = s_reset(s);
			continue;
		}

		/* what's left is part of the address */
		s_putc(s, c);

		/* quoted characters are recognized only as characters */
		if(c == '\\')
			quoted = 1;
		else
			quoted = 0;

	}

	if(*s_to_c(s) != 0){
		s_terminate(s);
		if(n++ != 0)
			s_append(list, " ");
		s_append(list, s_to_c(s));
	}
	s_free(s);

	if(n == 0){
		s_free(list);
		return nil;
	}
	return list;
}

static void
to822(Message *m, Header *h, char *p)
{
	p += strlen(h->type);
	s_free(m->to822);
	m->to822 = addr822(p);
}

static void
cc822(Message *m, Header *h, char *p)
{
	p += strlen(h->type);
	s_free(m->cc822);
	m->cc822 = addr822(p);
}

static void
bcc822(Message *m, Header *h, char *p)
{
	p += strlen(h->type);
	s_free(m->bcc822);
	m->bcc822 = addr822(p);
}

static void
from822(Message *m, Header *h, char *p)
{
	p += strlen(h->type);
	s_free(m->from822);
	m->from822 = addr822(p);
}

static void
sender822(Message *m, Header *h, char *p)
{
	p += strlen(h->type);
	s_free(m->sender822);
	m->sender822 = addr822(p);
}

static void
replyto822(Message *m, Header *h, char *p)
{
	p += strlen(h->type);
	s_free(m->replyto822);
	m->replyto822 = addr822(p);
}

static void
mimeversion(Message *m, Header *h, char *p)
{
	p += strlen(h->type);
	s_free(m->mimeversion);
	m->mimeversion = addr822(p);
}

static void
killtrailingwhite(char *p)
{
	char *e;

	e = p + strlen(p) - 1;
	while(e > p && isspace(*e))
		*e-- = 0;
}

static void
date822(Message *m, Header *h, char *p)
{
	p += strlen(h->type);
	p = skipwhite(p);
	s_free(m->date822);
	m->date822 = s_copy(p);
	p = s_to_c(m->date822);
	killtrailingwhite(p);
}

static void
subject822(Message *m, Header *h, char *p)
{
	p += strlen(h->type);
	p = skipwhite(p);
	s_free(m->subject822);
	m->subject822 = s_copy(p);
	p = s_to_c(m->subject822);
	killtrailingwhite(p);
}

static void
inreplyto822(Message *m, Header *h, char *p)
{
	p += strlen(h->type);
	p = skipwhite(p);
	s_free(m->inreplyto822);
	m->inreplyto822 = s_copy(p);
	p = s_to_c(m->inreplyto822);
	killtrailingwhite(p);
}

static void
messageid822(Message *m, Header *h, char *p)
{
	p += strlen(h->type);
	p = skipwhite(p);
	s_free(m->messageid822);
	m->messageid822 = s_copy(p);
	p = s_to_c(m->messageid822);
	killtrailingwhite(p);
}

static int
isattribute(char **pp, char *attr)
{
	char *p;
	int n;

	n = strlen(attr);
	p = *pp;
	if(cistrncmp(p, attr, n) != 0)
		return 0;
	p += n;
	while(*p == ' ')
		p++;
	if(*p++ != '=')
		return 0;
	while(*p == ' ')
		p++;
	*pp = p;
	return 1;
}

static void
ctype(Message *m, Header *h, char *p)
{
	String *s;

	p += h->len;
	p = skipwhite(p);

	p = getstring(p, m->type, 1);

	while(*p){
		if(isattribute(&p, "boundary")){
			s = s_new();
			p = getstring(p, s, 0);
			m->boundary = s_reset(m->boundary);
			s_append(m->boundary, "--");
			s_append(m->boundary, s_to_c(s));
			s_free(s);
		} else if(cistrncmp(p, "multipart", 9) == 0){
			/*
			 *  the first unbounded part of a multipart message,
			 *  the preamble, is not displayed or saved
			 */
		} else if(isattribute(&p, "name")){
			if(m->filename == nil)
				setfilename(m, p);
		} else if(isattribute(&p, "charset")){
			p = getstring(p, s_reset(m->charset), 0);
		}

		p = skiptosemi(p);
	}
}

static void
cencoding(Message *m, Header *h, char *p)
{
	p += h->len;
	p = skipwhite(p);
	if(cistrncmp(p, "base64", 6) == 0)
		m->encoding = Ebase64;
	else if(cistrncmp(p, "quoted-printable", 16) == 0)
		m->encoding = Equoted;
}

static void
cdisposition(Message *m, Header *h, char *p)
{
	p += h->len;
	p = skipwhite(p);
	while(*p){
		if(cistrncmp(p, "inline", 6) == 0){
			m->disposition = Dinline;
		} else if(cistrncmp(p, "attachment", 10) == 0){
			m->disposition = Dfile;
		} else if(cistrncmp(p, "filename=", 9) == 0){
			p += 9;
			setfilename(m, p);
		}
		p = skiptosemi(p);
	}

}

ulong msgallocd, msgfreed;

Message*
newmessage(Message *parent)
{
	/* static int id; jpc */
	Message *m;

	msgallocd++;

	m = emalloc(sizeof(*m));
	memset(m, 0, sizeof(*m));
	m->disposition = Dnone;
	m->type = s_copy("text/plain");
	m->charset = s_copy("iso-8859-1");
	m->id = newid();
	if(parent)
		sprint(m->name, "%d", ++(parent->subname));
	if(parent == nil)
		parent = m;
	m->whole = parent;
	m->hlen = -1;
	return m;
}

/* delete a message from a mailbox */
void
delmessage(Mailbox *mb, Message *m)
{
	Message **l;
	int i;

	mb->vers++;
	msgfreed++;

	if(m->whole != m){
		/* unchain from parent */
		for(l = &m->whole->part; *l && *l != m; l = &(*l)->next)
			;
		if(*l != nil)
			*l = m->next;

		/* clear out of name lookup hash table */
		if(m->whole->whole == m->whole)
			hfree(PATH(mb->id, Qmbox), m->name);
		else
			hfree(PATH(m->whole->id, Qdir), m->name);
		for(i = 0; i < Qmax; i++)
			hfree(PATH(m->id, Qdir), dirtab[i]);
	}

	/* recurse through sub-parts */
	while(m->part)
		delmessage(mb, m->part);

	/* free memory */
	if(m->mallocd)
		free(m->start);
	if(m->hallocd)
		free(m->header);
	if(m->ballocd)
		free(m->body);
	s_free(m->unixfrom);
	s_free(m->unixdate);
	s_free(m->unixheader);
	s_free(m->from822);
	s_free(m->sender822);
	s_free(m->to822);
	s_free(m->bcc822);
	s_free(m->cc822);
	s_free(m->replyto822);
	s_free(m->date822);
	s_free(m->inreplyto822);
	s_free(m->subject822);
	s_free(m->messageid822);
	s_free(m->addrs);
	s_free(m->mimeversion);
	s_free(m->sdigest);
	s_free(m->boundary);
	s_free(m->type);
	s_free(m->charset);
	s_free(m->filename);

	free(m);
}

/* mark messages (identified by path) for deletion */
void
delmessages(int ac, char **av)
{
	Mailbox *mb;
	Message *m;
	int i, needwrite;

	qlock(&mbllock);
	for(mb = mbl; mb != nil; mb = mb->next)
		if(strcmp(av[0], mb->name) == 0){
			qlock(&mb->ql);
			break;
		}
	qunlock(&mbllock);
	if(mb == nil)
		return;

	needwrite = 0;
	for(i = 1; i < ac; i++){
		for(m = mb->root->part; m != nil; m = m->next)
			if(strcmp(m->name, av[i]) == 0){
				if(!m->deleted){
					mailplumb(mb, m, 1);
					needwrite = 1;
					m->deleted = 1;
					logmsg("deleting", m);
				}
				break;
			}
	}
	if(needwrite)
		syncmbox(mb, 1);
	qunlock(&mb->ql);
}

/*
 *  the following are called with the mailbox qlocked
 */
void
msgincref(Message *m)
{
	m->refs++;
}
void
msgdecref(Mailbox *mb, Message *m)
{
	m->refs--;
	if(m->refs == 0 && m->deleted)
		syncmbox(mb, 1);
}

/*
 *  the following are called with mbllock'd
 */
void
mboxincref(Mailbox *mb)
{
	assert(mb->refs > 0);
	mb->refs++;
}
void
mboxdecref(Mailbox *mb)
{
	assert(mb->refs > 0);
	qlock(&mb->ql);
	mb->refs--;
	if(mb->refs == 0){
		delmessage(mb, mb->root);
		if(mb->ctl)
			hfree(PATH(mb->id, Qmbox), "ctl");
		if(mb->close)
			(*mb->close)(mb);
		free(mb);
	} else
		qunlock(&mb->ql);
}

int
cistrncmp(char *a, char *b, int n)
{
	while(n-- > 0){
		if(tolower(*a++) != tolower(*b++))
			return -1;
	}
	return 0;
}

int
cistrcmp(char *a, char *b)
{
	for(;;){
		if(tolower(*a) != tolower(*b++))
			return -1;
		if(*a++ == 0)
			break;
	}
	return 0;
}

static char*
skipwhite(char *p)
{
	while(isspace(*p))
		p++;
	return p;
}

static char*
skiptosemi(char *p)
{
	while(*p && *p != ';')
		p++;
	while(*p == ';' || isspace(*p))
		p++;
	return p;
}

static char*
getstring(char *p, String *s, int dolower)
{
	s = s_reset(s);
	p = skipwhite(p);
	if(*p == '"'){
		p++;
		for(;*p && *p != '"'; p++)
			if(dolower)
				s_putc(s, tolower(*p));
			else
				s_putc(s, *p);
		if(*p == '"')
			p++;
		s_terminate(s);

		return p;
	}

	for(; *p && !isspace(*p) && *p != ';'; p++)
		if(dolower)
			s_putc(s, tolower(*p));
		else
			s_putc(s, *p);
	s_terminate(s);

	return p;
}

static void
setfilename(Message *m, char *p)
{
	m->filename = s_reset(m->filename);
	getstring(p, m->filename, 0);
	for(p = s_to_c(m->filename); *p; p++)
		if(*p == ' ' || *p == '\t' || *p == ';')
			*p = '_';
}

/* */
/* undecode message body */
/* */
void
decode(Message *m)
{
	int i, len;
	char *x;

	if(m->decoded)
		return;
	switch(m->encoding){
	case Ebase64:
		len = m->bend - m->body;
		i = (len*3)/4+1;	/* room for max chars + null */
		x = emalloc(i);
		len = dec64((uchar*)x, i, m->body, len);
		if(m->ballocd)
			free(m->body);
		m->body = x;
		m->bend = x + len;
		m->ballocd = 1;
		break;
	case Equoted:
		len = m->bend - m->body;
		x = emalloc(len+2);	/* room for null and possible extra nl */
		len = decquoted(x, m->body, m->bend);
		if(m->ballocd)
			free(m->body);
		m->body = x;
		m->bend = x + len;
		m->ballocd = 1;
		break;
	default:
		break;
	}
	m->decoded = 1;
}

/* convert latin1 to utf */
void
convert(Message *m)
{
	int len;
	char *x;

	/* don't convert if we're not a leaf, not text, or already converted */
	if(m->converted)
		return;
	if(m->part != nil)
		return;
	if(cistrncmp(s_to_c(m->type), "text", 4) != 0)
		return;

	if(cistrcmp(s_to_c(m->charset), "us-ascii") == 0 ||
	   cistrcmp(s_to_c(m->charset), "iso-8859-1") == 0){
		len = is8bit(m);
		if(len > 0){
			len = 2*len + m->bend - m->body + 1;
			x = emalloc(len);
			len = latin1toutf(x, m->body, m->bend);
			if(m->ballocd)
				free(m->body);
			m->body = x;
			m->bend = x + len;
			m->ballocd = 1;
		}
	} else if(cistrcmp(s_to_c(m->charset), "iso-8859-2") == 0){
		len = xtoutf("8859-2", &x, m->body, m->bend);
		if(len != 0){
			if(m->ballocd)
				free(m->body);
			m->body = x;
			m->bend = x + len;
			m->ballocd = 1;
		}
	} else if(cistrcmp(s_to_c(m->charset), "iso-8859-15") == 0){
		len = xtoutf("8859-15", &x, m->body, m->bend);
		if(len != 0){
			if(m->ballocd)
				free(m->body);
			m->body = x;
			m->bend = x + len;
			m->ballocd = 1;
		}
	} else if(cistrcmp(s_to_c(m->charset), "big5") == 0){
		len = xtoutf("big5", &x, m->body, m->bend);
		if(len != 0){
			if(m->ballocd)
				free(m->body);
			m->body = x;
			m->bend = x + len;
			m->ballocd = 1;
		}
	} else if(cistrcmp(s_to_c(m->charset), "iso-2022-jp") == 0){
		len = xtoutf("jis", &x, m->body, m->bend);
		if(len != 0){
			if(m->ballocd)
				free(m->body);
			m->body = x;
			m->bend = x + len;
			m->ballocd = 1;
		}
	} else if(cistrcmp(s_to_c(m->charset), "windows-1257") == 0
			|| cistrcmp(s_to_c(m->charset), "windows-1252") == 0){
		len = is8bit(m);
		if(len > 0){
			len = 2*len + m->bend - m->body + 1;
			x = emalloc(len);
			len = windows1257toutf(x, m->body, m->bend);
			if(m->ballocd)
				free(m->body);
			m->body = x;
			m->bend = x + len;
			m->ballocd = 1;
		}
	} else if(cistrcmp(s_to_c(m->charset), "windows-1251") == 0){
		len = xtoutf("cp1251", &x, m->body, m->bend);
		if(len != 0){
			if(m->ballocd)
				free(m->body);
			m->body = x;
			m->bend = x + len;
			m->ballocd = 1;
		}
	} else if(cistrcmp(s_to_c(m->charset), "koi8-r") == 0){
		len = xtoutf("koi8", &x, m->body, m->bend);
		if(len != 0){
			if(m->ballocd)
				free(m->body);
			m->body = x;
			m->bend = x + len;
			m->ballocd = 1;
		}
	}

	m->converted = 1;
}

enum
{
	Self=	1,
	Hex=	2
};
uchar	tableqp[256];

static void
initquoted(void)
{
	int c;

	memset(tableqp, 0, 256);
	for(c = ' '; c <= '<'; c++)
		tableqp[c] = Self;
	for(c = '>'; c <= '~'; c++)
		tableqp[c] = Self;
	tableqp['\t'] = Self;
	tableqp['='] = Hex;
}

static int
hex2int(int x)
{
	if(x >= '0' && x <= '9')
		return x - '0';
	if(x >= 'A' && x <= 'F')
		return (x - 'A') + 10;
	if(x >= 'a' && x <= 'f')
		return (x - 'a') + 10;
	return 0;
}

static char*
decquotedline(char *out, char *in, char *e)
{
	int c, soft;

	/* dump trailing white space */
	while(e >= in && (*e == ' ' || *e == '\t' || *e == '\r' || *e == '\n'))
		e--;

	/* trailing '=' means no newline */
	if(*e == '='){
		soft = 1;
		e--;
	} else
		soft = 0;

	while(in <= e){
		c = (*in++) & 0xff;
		switch(tableqp[c]){
		case Self:
			*out++ = c;
			break;
		case Hex:
			c = hex2int(*in++)<<4;
			c |= hex2int(*in++);
			*out++ = c;
			break;
		}
	}
	if(!soft)
		*out++ = '\n';
	*out = 0;

	return out;
}

int
decquoted(char *out, char *in, char *e)
{
	char *p, *nl;

	if(tableqp[' '] == 0)
		initquoted();

	p = out;
	while((nl = strchr(in, '\n')) != nil && nl < e){
		p = decquotedline(p, in, nl);
		in = nl + 1;
	}
	if(in < e)
		p = decquotedline(p, in, e-1);

	/* make sure we end with a new line */
	if(*(p-1) != '\n'){
		*p++ = '\n';
		*p = 0;
	}

	return p - out;
}

#if 0 /* jpc */
static char*
lowercase(char *p)
{
	char *op;
	int c;

	for(op = p; c = *p; p++)
		if(isupper(c))
			*p = tolower(c);
	return op;
}
#endif

/*
 *  return number of 8 bit characters
 */
static int
is8bit(Message *m)
{
	int count = 0;
	char *p;

	for(p = m->body; p < m->bend; p++)
		if(*p & 0x80)
			count++;
	return count;
}

/* translate latin1 directly since it fits neatly in utf */
int
latin1toutf(char *out, char *in, char *e)
{
	Rune r;
	char *p;

	p = out;
	for(; in < e; in++){
		r = (*in) & 0xff;
		p += runetochar(p, &r);
	}
	*p = 0;
	return p - out;
}

/* translate any thing else using the tcs program */
int
xtoutf(char *charset, char **out, char *in, char *e)
{
	char *av[4];
	int totcs[2];
	int fromtcs[2];
	int n, len, sofar;
	char *p;

	len = e-in+1;
	sofar = 0;
	*out = p = malloc(len+1);
	if(p == nil)
		return 0;

	av[0] = charset;
	av[1] = "-f";
	av[2] = charset;
	av[3] = 0;
	if(pipe(totcs) < 0)
		return 0;
	if(pipe(fromtcs) < 0){
		close(totcs[0]); close(totcs[1]);
		return 0;
	}
	switch(rfork(RFPROC|RFFDG|RFNOWAIT)){
	case -1:
		close(fromtcs[0]); close(fromtcs[1]);
		close(totcs[0]); close(totcs[1]);
		return 0;
	case 0:
		close(fromtcs[0]); close(totcs[1]);
		dup(fromtcs[1], 1);
		dup(totcs[0], 0);
		close(fromtcs[1]); close(totcs[0]);
		dup(open("/dev/null", OWRITE), 2);
		/*jpc exec("/bin/tcs", av); */
		exec(unsharp("#9/bin/tcs"), av);
		/* _exits(0); */
		threadexits(nil);
	default:
		close(fromtcs[1]); close(totcs[0]);
		switch(rfork(RFPROC|RFFDG|RFNOWAIT)){
		case -1:
			close(fromtcs[0]); close(totcs[1]);
			return 0;
		case 0:
			close(fromtcs[0]);
			while(in < e){
				n = write(totcs[1], in, e-in);
				if(n <= 0)
					break;
				in += n;
			}
			close(totcs[1]);
			/* _exits(0); */
			threadexits(nil);
		default:
			close(totcs[1]);
			for(;;){
				n = read(fromtcs[0], &p[sofar], len-sofar);
				if(n <= 0)
					break;
				sofar += n;
				p[sofar] = 0;
				if(sofar == len){
					len += 1024;
					*out = p = realloc(p, len+1);
					if(p == nil)
						return 0;
				}
			}
			close(fromtcs[0]);
			break;
		}
		break;
	}
	return sofar;
}

enum {
	Winstart= 0x7f,
	Winend= 0x9f
};

Rune winchars[] = {
	L'•',
	L'•', L'•', L'‚', L'ƒ', L'„', L'…', L'†', L'‡',
	L'ˆ', L'‰', L'Š', L'‹', L'Œ', L'•', L'•', L'•',
	L'•', L'‘', L'’', L'“', L'”', L'•', L'–', L'—',
	L'˜', L'™', L'š', L'›', L'œ', L'•', L'•', L'Ÿ'
};

int
windows1257toutf(char *out, char *in, char *e)
{
	Rune r;
	char *p;

	p = out;
	for(; in < e; in++){
		r = (*in) & 0xff;
		if(r >= 0x7f && r <= 0x9f)
			r = winchars[r-0x7f];
		p += runetochar(p, &r);
	}
	*p = 0;
	return p - out;
}

void *
emalloc(ulong n)
{
	void *p;

	p = mallocz(n, 1);
	if(!p){
		fprint(2, "%s: out of memory alloc %lud\n", argv0, n);
		threadexits("out of memory");
	}
	setmalloctag(p, getcallerpc(&n));
	return p;
}

void *
erealloc(void *p, ulong n)
{
	if(n == 0)
		n = 1;
	p = realloc(p, n);
	if(!p){
		fprint(2, "%s: out of memory realloc %lud\n", argv0, n);
		threadexits("out of memory");
	}
	setrealloctag(p, getcallerpc(&p));
	return p;
}

void
mailplumb(Mailbox *mb, Message *m, int delete)
{
	Plumbmsg p;
	Plumbattr a[7];
	char buf[256];
	int ai;
	char lenstr[10], *from, *subject, *date;
	static int fd = -1;

	if(m->subject822 == nil)
		subject = "";
	else
		subject = s_to_c(m->subject822);

	if(m->from822 != nil)
		from = s_to_c(m->from822);
	else if(m->unixfrom != nil)
		from = s_to_c(m->unixfrom);
	else
		from = "";

	if(m->unixdate != nil)
		date = s_to_c(m->unixdate);
	else
		date = "";

	sprint(lenstr, "%ld", m->end-m->start);

	if(biffing && !delete)
		print("[ %s / %s / %s ]\n", from, subject, lenstr);

	if(!plumbing)
		return;

	if(fd < 0)
		fd = plumbopen("send", OWRITE);
	if(fd < 0)
		return;

	p.src = "mailfs";
	p.dst = "seemail";
	p.wdir = "/mail/fs";
	p.type = "text";

	ai = 0;
	a[ai].name = "filetype";
	a[ai].value = "mail";

	a[++ai].name = "sender";
	a[ai].value = from;
	a[ai-1].next = &a[ai];

	a[++ai].name = "length";
	a[ai].value = lenstr;
	a[ai-1].next = &a[ai];

	a[++ai].name = "mailtype";
	a[ai].value = delete?"delete":"new";
	a[ai-1].next = &a[ai];

	a[++ai].name = "date";
	a[ai].value = date;
	a[ai-1].next = &a[ai];

	if(m->sdigest){
		a[++ai].name = "digest";
		a[ai].value = s_to_c(m->sdigest);
		a[ai-1].next = &a[ai];
	}

	a[ai].next = nil;

	p.attr = a;
	snprint(buf, sizeof(buf), "%s/%s/%s",
		mntpt, mb->name, m->name);
	p.ndata = strlen(buf);
	p.data = buf;

	plumbsend(fd, &p);
}

/* */
/* count the number of lines in the body (for imap4) */
/* */
void
countlines(Message *m)
{
	int i;
	char *p;

	i = 0;
	for(p = strchr(m->rbody, '\n'); p != nil && p < m->rbend; p = strchr(p+1, '\n'))
		i++;
	sprint(m->lines, "%d", i);
}

char *LOG = "fs";

void
logmsg(char *s, Message *m)
{
	int pid;

	if(!logging)
		return;
	pid = getpid();
	if(m == nil)
		syslog(0, LOG, "%s.%d: %s", user, pid, s);
	else
		syslog(0, LOG, "%s.%d: %s msg from %s digest %s",
			user, pid, s,
			m->from822 ? s_to_c(m->from822) : "?",
			s_to_c(m->sdigest));
}

/*
 *  squeeze nulls out of the body
 */
static void
nullsqueeze(Message *m)
{
	char *p, *q;

	q = memchr(m->body, 0, m->end-m->body);
	if(q == nil)
		return;

	for(p = m->body; q < m->end; q++){
		if(*q == 0)
			continue;
		*p++ = *q;
	}
	m->bend = m->rbend = m->end = p;
}


/* */
/* convert an RFC822 date into a Unix style date */
/* for when the Unix From line isn't there (e.g. POP3). */
/* enough client programs depend on having a Unix date */
/* that it's easiest to write this conversion code once, right here. */
/* */
/* people don't follow RFC822 particularly closely, */
/* so we use strtotm, which is a bunch of heuristics. */
/* */

extern int strtotm(char*, Tm*);
String*
date822tounix(char *s)
{
	char *p, *q;
	Tm tm;

	if(strtotm(s, &tm) < 0)
		return nil;

	p = asctime(&tm);
	if(q = strchr(p, '\n'))
		*q = '\0';
	return s_copy(p);
}
