#include "common.h"
#include "send.h"

#include "../smtp/smtp.h"
#include "../smtp/rfc822.tab.h"

/* global to this file */
static Reprog *rfprog;
static Reprog *fprog;

#define VMLIMIT (64*1024)
#define MSGLIMIT (128*1024*1024)

int received;	/* from rfc822.y */

static String*	getstring(Node *p);
static String*	getaddr(Node *p);

extern int
default_from(message *mp)
{
	char *cp, *lp;

	cp = getenv("upasname");
	lp = getlog();
	if(lp == nil)
		return -1;

	if(cp && *cp)
		s_append(mp->sender, cp);
	else
		s_append(mp->sender, lp);
	s_append(mp->date, thedate());
	return 0;
}

extern message *
m_new(void)
{
	message *mp;

	mp = (message *)mallocz(sizeof(message), 1);
	if (mp == 0) {
		perror("message:");
		exit(1);
	}
	mp->sender = s_new();
	mp->replyaddr = s_new();
	mp->date = s_new();
	mp->body = s_new();
	mp->size = 0;
	mp->fd = -1;
	return mp;
}

extern void
m_free(message *mp)
{
	if(mp->fd >= 0){
		close(mp->fd);
		sysremove(s_to_c(mp->tmp));
		s_free(mp->tmp);
	}
	s_free(mp->sender);
	s_free(mp->date);
	s_free(mp->body);
	s_free(mp->havefrom);
	s_free(mp->havesender);
	s_free(mp->havereplyto);
	s_free(mp->havesubject);
	free((char *)mp);
}

/* read a message into a temp file , return an open fd to it */
static int
m_read_to_file(Biobuf *fp, message *mp)
{
	int fd;
	int n;
	String *file;
	char buf[4*1024];

	file = s_new();
	/*
	 *  create temp file to be remove on close
	 */
	abspath("mtXXXXXX", UPASTMP, file);
	mktemp(s_to_c(file));
	if((fd = syscreate(s_to_c(file), ORDWR|ORCLOSE, 0600))<0){
		s_free(file);
		return -1;
	}
	mp->tmp = file;

	/*
	 *  read the rest into the temp file
	 */
	while((n = Bread(fp, buf, sizeof(buf))) > 0){
		if(write(fd, buf, n) != n){
			close(fd);
			return -1;
		}
		mp->size += n;
		if(mp->size > MSGLIMIT){
			mp->size = -1;
			break;
		}
	}

	mp->fd = fd;
	return 0;
}

/* get the first address from a node */
static String*
getaddr(Node *p)
{
	for(; p; p = p->next)
		if(p->s && p->addr)
			return s_copy(s_to_c(p->s));
	return nil;
}

/* get the text of a header line minus the field name */
static String*
getstring(Node *p)
{
	String *s;

	s = s_new();
	if(p == nil)
		return s;

	for(p = p->next; p; p = p->next){
		if(p->s){
			s_append(s, s_to_c(p->s));
		}else{
			s_putc(s, p->c);
			s_terminate(s);
		}
		if(p->white)
			s_append(s, s_to_c(p->white));
	}
	return s;
}

#if 0
static char *fieldname[] =
{
[WORD-WORD]	"WORD",
[DATE-WORD]	"DATE",
[RESENT_DATE-WORD]	"RESENT_DATE",
[RETURN_PATH-WORD]	"RETURN_PATH",
[FROM-WORD]	"FROM",
[SENDER-WORD]	"SENDER",
[REPLY_TO-WORD]	"REPLY_TO",
[RESENT_FROM-WORD]	"RESENT_FROM",
[RESENT_SENDER-WORD]	"RESENT_SENDER",
[RESENT_REPLY_TO-WORD]	"RESENT_REPLY_TO",
[SUBJECT-WORD]	"SUBJECT",
[TO-WORD]	"TO",
[CC-WORD]	"CC",
[BCC-WORD]	"BCC",
[RESENT_TO-WORD]	"RESENT_TO",
[RESENT_CC-WORD]	"RESENT_CC",
[RESENT_BCC-WORD]	"RESENT_BCC",
[REMOTE-WORD]	"REMOTE",
[PRECEDENCE-WORD]	"PRECEDENCE",
[MIMEVERSION-WORD]	"MIMEVERSION",
[CONTENTTYPE-WORD]	"CONTENTTYPE",
[MESSAGEID-WORD]	"MESSAGEID",
[RECEIVED-WORD]	"RECEIVED",
[MAILER-WORD]	"MAILER",
[BADTOKEN-WORD]	"BADTOKEN"
};
#endif

/* fix 822 addresses */
static void
rfc822cruft(message *mp)
{
	Field *f;
	Node *p;
	String *body, *s;
	char *cp;

	/*
	 *  parse headers in in-core part
	 */
	yyinit(s_to_c(mp->body), s_len(mp->body));
	mp->rfc822headers = 0;
	yyparse();
	mp->rfc822headers = 1;
	mp->received = received;

	/*
	 *  remove equivalent systems in all addresses
	 */
	body = s_new();
	cp = s_to_c(mp->body);
	for(f = firstfield; f; f = f->next){
		if(f->node->c == MIMEVERSION)
			mp->havemime = 1;
		if(f->node->c == FROM)
			mp->havefrom = getaddr(f->node);
		if(f->node->c == SENDER)
			mp->havesender = getaddr(f->node);
		if(f->node->c == REPLY_TO)
			mp->havereplyto = getaddr(f->node);
		if(f->node->c == TO)
			mp->haveto = 1;
		if(f->node->c == DATE)
			mp->havedate = 1;
		if(f->node->c == SUBJECT)
			mp->havesubject = getstring(f->node);
		if(f->node->c == PRECEDENCE && f->node->next && f->node->next->next){
			s = f->node->next->next->s;
			if(s && (strcmp(s_to_c(s), "bulk") == 0
				|| strcmp(s_to_c(s), "Bulk") == 0))
					mp->bulk = 1;
		}
		for(p = f->node; p; p = p->next){
			if(p->s){
				if(p->addr){
					cp = skipequiv(s_to_c(p->s));
					s_append(body, cp);
				} else
					s_append(body, s_to_c(p->s));
			}else{
				s_putc(body, p->c);
				s_terminate(body);
			}
			if(p->white)
				s_append(body, s_to_c(p->white));
			cp = p->end+1;
		}
		s_append(body, "\n");
	}

	if(*s_to_c(body) == 0){
		s_free(body);
		return;
	}

	if(*cp != '\n')
		s_append(body, "\n");
	s_memappend(body, cp, s_len(mp->body) - (cp - s_to_c(mp->body)));
	s_terminate(body);

	firstfield = 0;
	mp->size += s_len(body) - s_len(mp->body);
	s_free(mp->body);
	mp->body = body;
}

/* read in a message, interpret the 'From' header */
extern message *
m_read(Biobuf *fp, int rmail, int interactive)
{
	message *mp;
	Resub subexp[10];
	char *line;
	int first;
	int n;

	mp = m_new();

	/* parse From lines if remote */
	if (rmail) {
		/* get remote address */
		String *sender=s_new();

		if (rfprog == 0)
			rfprog = regcomp(REMFROMRE);
		first = 1;
		while(s_read_line(fp, s_restart(mp->body)) != 0) {
			memset(subexp, 0, sizeof(subexp));
			if (regexec(rfprog, s_to_c(mp->body), subexp, 10) == 0){
				if(first == 0)
					break;
				if (fprog == 0)
					fprog = regcomp(FROMRE);
				memset(subexp, 0, sizeof(subexp));
				if(regexec(fprog, s_to_c(mp->body), subexp,10) == 0)
					break;
				s_restart(mp->body);
				append_match(subexp, s_restart(sender), SENDERMATCH);
				append_match(subexp, s_restart(mp->date), DATEMATCH);
				break;
			}
			append_match(subexp, s_restart(sender), REMSENDERMATCH);
			append_match(subexp, s_restart(mp->date), REMDATEMATCH);
			if(subexp[REMSYSMATCH].s.sp!=subexp[REMSYSMATCH].e.ep){
				append_match(subexp, mp->sender, REMSYSMATCH);
				s_append(mp->sender, "!");
			}
			first = 0;
		}
		s_append(mp->sender, s_to_c(sender));

		s_free(sender);
	}
	if(*s_to_c(mp->sender)=='\0')
		default_from(mp);

	/* if sender address is unreturnable, treat message as bulk mail */
	if(!returnable(s_to_c(mp->sender)))
		mp->bulk = 1;

	/* get body */
	if(interactive && !rmail){
		/* user typing on terminal: terminator == '.' or EOF */
		for(;;) {
			line = s_read_line(fp, mp->body);
			if (line == 0)
				break;
			if (strcmp(".\n", line)==0) {
				mp->body->ptr -= 2;
				*mp->body->ptr = '\0';
				break;
			}
		}
		mp->size = mp->body->ptr - mp->body->base;
	} else {
		/*
		 *  read up to VMLIMIT bytes (more or less) into main memory.
		 *  if message is longer put the rest in a tmp file.
		 */
		mp->size = mp->body->ptr - mp->body->base;
		n = s_read(fp, mp->body, VMLIMIT);
		if(n < 0){
			perror("m_read");
			exit(1);
		}
		mp->size += n;
		if(n == VMLIMIT){
			if(m_read_to_file(fp, mp) < 0){
				perror("m_read_to_file");
				exit(1);
			}
		}
	}

	/*
	 *  ignore 0 length messages from a terminal
	 */
	if (!rmail && mp->size == 0)
		return 0;

	rfc822cruft(mp);

	return mp;
}

/* return a piece of message starting at `offset' */
extern int
m_get(message *mp, long offset, char **pp)
{
	static char buf[4*1024];

	/*
	 *  are we past eof?
	 */
	if(offset >= mp->size)
		return 0;

	/*
	 *  are we in the virtual memory portion?
	 */
	if(offset < s_len(mp->body)){
		*pp = mp->body->base + offset;
		return mp->body->ptr - mp->body->base - offset;
	}

	/*
	 *  read it from the temp file
	 */
	offset -= s_len(mp->body);
	if(mp->fd < 0)
		return -1;
	if(seek(mp->fd, offset, 0)<0)
		return -1;
	*pp = buf;
	return read(mp->fd, buf, sizeof buf);
}

/* output the message body without ^From escapes */
static int
m_noescape(message *mp, Biobuf *fp)
{
	long offset;
	int n;
	char *p;

	for(offset = 0; offset < mp->size; offset += n){
		n = m_get(mp, offset, &p);
		if(n <= 0){
			Bflush(fp);
			return -1;
		}
		if(Bwrite(fp, p, n) < 0)
			return -1;
	}
	return Bflush(fp);
}

/*
 *  Output the message body with '^From ' escapes.
 *  Ensures that any line starting with a 'From ' gets a ' ' stuck
 *  in front of it.
 */
static int
m_escape(message *mp, Biobuf *fp)
{
	char *p, *np;
	char *end;
	long offset;
	int m, n;
	char *start;

	for(offset = 0; offset < mp->size; offset += n){
		n = m_get(mp, offset, &start);
		if(n < 0){
			Bflush(fp);
			return -1;
		}

		p = start;
		for(end = p+n; p < end; p += m){
			np = memchr(p, '\n', end-p);
			if(np == 0){
				Bwrite(fp, p, end-p);
				break;
			}
			m = np - p + 1;
			if(m > 5 && strncmp(p, "From ", 5) == 0)
				Bputc(fp, ' ');
			Bwrite(fp, p, m);
		}
	}
	Bflush(fp);
	return 0;
}

static int
printfrom(message *mp, Biobuf *fp)
{
	String *s;
	int rv;

	if(!returnable(s_to_c(mp->sender)))
		return Bprint(fp, "From: Postmaster\n");

	s = username(mp->sender);
	if(s) {
		s_append(s, " <");
		s_append(s, s_to_c(mp->sender));
		s_append(s, ">");
	} else {
		s = s_copy(s_to_c(mp->sender));
	}
	s = unescapespecial(s);
	rv = Bprint(fp, "From: %s\n", s_to_c(s));
	s_free(s);
	return rv;
}

static char *
rewritezone(char *z)
{
	int mindiff;
	char s;
	Tm *tm;
	static char x[7];

	tm = localtime(time(0));
	mindiff = tm->tzoff/60;

	/* if not in my timezone, don't change anything */
	if(strcmp(tm->zone, z) != 0)
		return z;

	if(mindiff < 0){
		s = '-';
		mindiff = -mindiff;
	} else
		s = '+';

	sprint(x, "%c%.2d%.2d", s, mindiff/60, mindiff%60);
	return x;
}

int
isutf8(String *s)
{
	char *p;

	for(p = s_to_c(s);  *p; p++)
		if(*p&0x80)
			return 1;
	return 0;
}

void
printutf8mime(Biobuf *b)
{
	Bprint(b, "MIME-Version: 1.0\n");
	Bprint(b, "Content-Type: text/plain; charset=\"UTF-8\"\n");
	Bprint(b, "Content-Transfer-Encoding: 8bit\n");
}

/* output a message */
extern int
m_print(message *mp, Biobuf *fp, char *remote, int mbox)
{
	String *date, *sender;
	char *f[6];
	int n;

	sender = unescapespecial(s_clone(mp->sender));

	if (remote != 0){
		if(print_remote_header(fp,s_to_c(sender),s_to_c(mp->date),remote) < 0){
			s_free(sender);
			return -1;
		}
	} else {
		if(print_header(fp, s_to_c(sender), s_to_c(mp->date)) < 0){
			s_free(sender);
			return -1;
		}
	}
	s_free(sender);
	if(!rmail && !mp->havedate){
		/* add a date: line Date: Sun, 19 Apr 1998 12:27:52 -0400 */
		date = s_copy(s_to_c(mp->date));
		n = getfields(s_to_c(date), f, 6, 1, " \t");
		if(n == 6)
			Bprint(fp, "Date: %s, %s %s %s %s %s\n", f[0], f[2], f[1],
			 f[5], f[3], rewritezone(f[4]));
	}
	if(!rmail && !mp->havemime && isutf8(mp->body))
		printutf8mime(fp);
	if(mp->to){
		/* add the to: line */
		if (Bprint(fp, "%s\n", s_to_c(mp->to)) < 0)
			return -1;
		/* add the from: line */
		if (!mp->havefrom && printfrom(mp, fp) < 0)
			return -1;
		if(!mp->rfc822headers && *s_to_c(mp->body) != '\n')
			if (Bprint(fp, "\n") < 0)
				return -1;
	} else if(!rmail){
		/* add the from: line */
		if (!mp->havefrom && printfrom(mp, fp) < 0)
			return -1;
		if(!mp->rfc822headers && *s_to_c(mp->body) != '\n')
			if (Bprint(fp, "\n") < 0)
				return -1;
	}

	if (!mbox)
		return m_noescape(mp, fp);
	return m_escape(mp, fp);
}

/* print just the message body */
extern int
m_bprint(message *mp, Biobuf *fp)
{
	return m_noescape(mp, fp);
}
