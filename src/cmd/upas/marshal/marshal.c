#include "common.h"
#include <thread.h>
#include <9pclient.h>
#include <ctype.h>

enum
{
	STACK = 32768
};

#define inline _inline

typedef struct Attach Attach;
typedef struct Alias Alias;
typedef struct Addr Addr;
typedef struct Ctype Ctype;

struct Attach {
	Attach	*next;
	char	*path;
	int	fd;
	char	*type;
	int	inline;
	Ctype	*ctype;
};

struct Alias
{
	Alias	*next;
	int	n;
	Addr	*addr;
};

struct Addr
{
	Addr	*next;
	char	*v;
};

enum {
	Hfrom,
	Hto,
	Hcc,
	Hbcc,
	Hsender,
	Hreplyto,
	Hinreplyto,
	Hdate,
	Hsubject,
	Hmime,
	Hpriority,
	Hmsgid,
	Hcontent,
	Hx,
	Hprecedence,
	Nhdr
};

enum {
	PGPsign = 1,
	PGPencrypt = 2
};

char *hdrs[Nhdr] = {
[Hfrom]		"from:",
[Hto]		"to:",
[Hcc]		"cc:",
[Hbcc]		"bcc:",
[Hreplyto]	"reply-to:",
[Hinreplyto]	"in-reply-to:",
[Hsender]	"sender:",
[Hdate]		"date:",
[Hsubject]	"subject:",
[Hpriority]	"priority:",
[Hmsgid]	"message-id:",
[Hmime]		"mime-",
[Hcontent]	"content-",
[Hx]		"x-",
[Hprecedence]	"precedence"
};

struct Ctype {
	char	*type;
	char 	*ext;
	int	display;
};

Ctype ctype[] = {
	{ "text/plain",			"txt",	1,	},
	{ "text/html",			"html",	1,	},
	{ "text/html",			"htm",	1,	},
	{ "text/tab-separated-values",	"tsv",	1,	},
	{ "text/richtext",		"rtx",	1,	},
	{ "message/rfc822",		"txt",	1,	},
	{ "", 				0,	0,	}
};

Ctype *mimetypes;

int pid = -1;
int pgppid = -1;

Attach*	mkattach(char*, char*, int);
int	readheaders(Biobuf*, int*, String**, Addr**, int);
void	body(Biobuf*, Biobuf*, int);
char*	mkboundary(void);
int	printdate(Biobuf*);
int	printfrom(Biobuf*);
int	printto(Biobuf*, Addr*);
int	printcc(Biobuf*, Addr*);
int	printsubject(Biobuf*, char*);
int	printinreplyto(Biobuf*, char*);
int	sendmail(Addr*, Addr*, int*, char*);
void	attachment(Attach*, Biobuf*);
int	cistrncmp(char*, char*, int);
int	cistrcmp(char*, char*);
char*	waitforsubprocs(void);
int	enc64(char*, int, uchar*, int);
Addr*	expand(int, char**);
Alias*	readaliases(void);
Addr*	expandline(String**, Addr*);
void	Bdrain(Biobuf*);
void	freeaddr(Addr *);
int	pgpopts(char*);
int	pgpfilter(int*, int, int);
void	readmimetypes(void);
char*	estrdup(char*);
void*	emalloc(int);
void*	erealloc(void*, int);
void	freeaddr(Addr*);
void	freeaddrs(Addr*);
void	freealias(Alias*);
void	freealiases(Alias*);
int	doublequote(Fmt*);
int	mountmail(void);
int	nprocexec;
int	rfc2047fmt(Fmt*);
char*	mksubject(char*);

int rflag, lbflag, xflag, holding, nflag, Fflag, eightflag, dflag;
int pgpflag = 0;
char *user;
char *login;
Alias *aliases;
int rfc822syntaxerror;
char lastchar;
char *replymsg;

CFsys *mailfs;

enum
{
	Ok = 0,
	Nomessage = 1,
	Nobody = 2,
	Error = -1
};

#pragma varargck	type	"Z"	char*

void
usage(void)
{
	fprint(2, "usage: %s [-Fr#xn] [-s subject] [-c ccrecipient] [-t type] [-aA attachment] [-p[es]] [-R replymsg] -8 | recipient-list\n",
		argv0);
	threadexitsall("usage");
}

void
fatal(char *fmt, ...)
{
	char buf[1024];
	va_list arg;

	if(pid >= 0)
		postnote(PNPROC, pid, "die");
	if(pgppid >= 0)
		postnote(PNPROC, pgppid, "die");

	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	fprint(2, "%s: %s\n", argv0, buf);
	holdoff(holding);
	threadexitsall(buf);
}

void
threadmain(int argc, char **argv)
{
	Attach *first, **l, *a;
	char *subject, *type, *boundary;
	int flags, fd;
	Biobuf in, out, *b;
	Addr *to;
	Addr *cc;
	String *file, *hdrstring;
	int noinput, headersrv;
	int ccargc;
	char *ccargv[32];

	noinput = 0;
	subject = nil;
	first = nil;
	l = &first;
	type = nil;
	hdrstring = nil;
	ccargc = 0;

	quotefmtinstall();
	fmtinstall('Z', doublequote);
	fmtinstall('U', rfc2047fmt);
	threadwaitchan();

	ARGBEGIN{
	case 't':
		type = EARGF(usage());
		break;
	case 'a':
		flags = 0;
		goto aflag;
	case 'A':
		flags = 1;
	aflag:
		a = mkattach(EARGF(usage()), type, flags);
		if(a == nil)
			threadexitsall("bad args");
		type = nil;
		*l = a;
		l = &a->next;
		break;
	case 'C':
		if(ccargc >= nelem(ccargv)-1)
			sysfatal("too many cc's");
		ccargv[ccargc] = ARGF();
		if(ccargv[ccargc] == nil)
			usage();
		ccargc++;
		break;
	case 'R':
		replymsg = EARGF(usage());
		break;
	case 's':
		subject = EARGF(usage());
		break;
	case 'F':
		Fflag = 1;		/* file message */
		break;
	case 'r':
		rflag = 1;		/* for sendmail */
		break;
	case 'd':
		dflag = 1;		/* for sendmail */
		break;
	case '#':
		lbflag = 1;		/* for sendmail */
		break;
	case 'x':
		xflag = 1;		/* for sendmail */
		break;
	case 'n':			/* no standard input */
		nflag = 1;
		break;
	case '8':			/* read recipients from rfc822 header */
		eightflag = 1;
		break;
	case 'p':			/* pgp flag: encrypt, sign, or both */
		if(pgpopts(EARGF(usage())) < 0)
			sysfatal("bad pgp options");
		break;
	default:
		usage();
		break;
	}ARGEND;

	login = getlog();
	user = getenv("upasname");
	if(user == nil || *user == 0)
		user = login;
	if(user == nil || *user == 0)
		sysfatal("can't read user name");

	if(Binit(&in, 0, OREAD) < 0)
		sysfatal("can't Binit 0: %r");

	if(nflag && eightflag)
		sysfatal("can't use both -n and -8");
	if(eightflag && argc >= 1)
		usage();
	else if(!eightflag && argc < 1)
		usage();

	aliases = readaliases();
	if(!eightflag){
		to = expand(argc, argv);
		cc = expand(ccargc, ccargv);
	} else {
		to = nil;
		cc = nil;
	}

	flags = 0;
	headersrv = Nomessage;
	if(!nflag && !xflag && !lbflag &&!dflag) {
		/* pass through headers, keeping track of which we've seen, */
		/* perhaps building to list. */
		holding = holdon();
		headersrv = readheaders(&in, &flags, &hdrstring, eightflag ? &to : nil, 1);
		if(rfc822syntaxerror){
			Bdrain(&in);
			fatal("rfc822 syntax error, message not sent");
		}
		if(to == nil){
			Bdrain(&in);
			fatal("no addresses found, message not sent");
		}

		switch(headersrv){
		case Error:		/* error */
			fatal("reading");
			break;
		case Nomessage:		/* no message, just exit mimicking old behavior */
			noinput = 1;
			if(first == nil)
				threadexitsall(0);
			break;
		}
	}

	fd = sendmail(to, cc, &pid, Fflag ? argv[0] : nil);
	if(fd < 0)
		sysfatal("execing sendmail: %r\n:");
	if(xflag || lbflag || dflag){
		close(fd);
		threadexitsall(waitforsubprocs());
	}

	if(Binit(&out, fd, OWRITE) < 0)
		fatal("can't Binit 1: %r");

	if(!nflag){
		if(Bwrite(&out, s_to_c(hdrstring), s_len(hdrstring)) != s_len(hdrstring))
			fatal("write error");
		s_free(hdrstring);
		hdrstring = nil;

		/* read user's standard headers */
		file = s_new();
		mboxpath("headers", user, file, 0);
		b = Bopen(s_to_c(file), OREAD);
		if(b != nil){
			switch(readheaders(b, &flags, &hdrstring, nil, 0)){
			case Error:	/* error */
				fatal("reading");
			}
			Bterm(b);
			if(Bwrite(&out, s_to_c(hdrstring), s_len(hdrstring)) != s_len(hdrstring))
				fatal("write error");
			s_free(hdrstring);
			hdrstring = nil;
		}
	}

	/* add any headers we need */
	if((flags & (1<<Hdate)) == 0)
		if(printdate(&out) < 0)
			fatal("writing");
	if((flags & (1<<Hfrom)) == 0)
		if(printfrom(&out) < 0)
			fatal("writing");
	if((flags & (1<<Hto)) == 0)
		if(printto(&out, to) < 0)
			fatal("writing");
	if((flags & (1<<Hcc)) == 0)
		if(printcc(&out, cc) < 0)
			fatal("writing");
	if((flags & (1<<Hsubject)) == 0 && subject != nil)
		if(printsubject(&out, subject) < 0)
			fatal("writing");
	if(replymsg != nil)
		printinreplyto(&out, replymsg);	/* ignore errors */
	Bprint(&out, "MIME-Version: 1.0\n");

	if(pgpflag){	/* interpose pgp process between us and sendmail to handle body */
		Bflush(&out);
		Bterm(&out);
		fd = pgpfilter(&pgppid, fd, pgpflag);
		if(Binit(&out, fd, OWRITE) < 0)
			fatal("can't Binit 1: %r");
	}

	/* if attachments, stick in multipart headers */
	boundary = nil;
	if(first != nil){
		boundary = mkboundary();
		Bprint(&out, "Content-Type: multipart/mixed;\n");
		Bprint(&out, "\tboundary=\"%s\"\n\n", boundary);
		Bprint(&out, "This is a multi-part message in MIME format.\n");
		Bprint(&out, "--%s\n", boundary);
		Bprint(&out, "Content-Disposition: inline\n");
	}

	if(!nflag){
		if(!noinput && headersrv == Ok){
			body(&in, &out, 1);
		}
	} else
		Bprint(&out, "\n");
	holdoff(holding);

	Bflush(&out);
	for(a = first; a != nil; a = a->next){
		if(lastchar != '\n')
			Bprint(&out, "\n");
		Bprint(&out, "--%s\n", boundary);
		attachment(a, &out);
	}

	if(first != nil){
		if(lastchar != '\n')
			Bprint(&out, "\n");
		Bprint(&out, "--%s--\n", boundary);
	}

	Bterm(&out);
	close(fd);
	threadexitsall(waitforsubprocs());
}

/* evaluate pgp option string */
int
pgpopts(char *s)
{
	if(s == nil || s[0] == '\0')
		return -1;
	while(*s){
		switch(*s++){
		case 's':  case 'S':
			pgpflag |= PGPsign;
			break;
		case 'e': case 'E':
			pgpflag |= PGPencrypt;
			break;
		default:
			return -1;
		}
	}
	return 0;
}

/* read headers from stdin into a String, expanding local aliases, */
/* keep track of which headers are there, which addresses we have */
/* remove Bcc: line. */
int
readheaders(Biobuf *in, int *fp, String **sp, Addr **top, int strict)
{
	Addr *to;
	String *s, *sline;
	char *p;
	int i, seen, hdrtype;

	s = s_new();
	sline = nil;
	to = nil;
	hdrtype = -1;
	seen = 0;
	for(;;) {
		if((p = Brdline(in, '\n')) != nil) {
			seen = 1;
			p[Blinelen(in)-1] = 0;

			/* coalesce multiline headers */
			if((*p == ' ' || *p == '\t') && sline){
				s_append(sline, "\n");
				s_append(sline, p);
				p[Blinelen(in)-1] = '\n';
				continue;
			}
		}

		/* process the current header, it's all been read */
		if(sline) {
			assert(hdrtype != -1);
			if(top){
				switch(hdrtype){
				case Hto:
				case Hcc:
				case Hbcc:
					to = expandline(&sline, to);
					break;
				}
			}
			if(hdrtype == Hsubject){
				s_append(s, mksubject(s_to_c(sline)));
				s_append(s, "\n");
			}else if(top==nil || hdrtype!=Hbcc){
				s_append(s, s_to_c(sline));
				s_append(s, "\n");
			}
			s_free(sline);
			sline = nil;
		}

		if(p == nil)
			break;

		/* if no :, it's not a header, seek back and break */
		if(strchr(p, ':') == nil){
			p[Blinelen(in)-1] = '\n';
			Bseek(in, -Blinelen(in), 1);
			break;
		}

		sline = s_copy(p);

		/* classify the header.  If we don't recognize it, break.  This is */
		/* to take care of user's that start messages with lines that contain */
		/* ':'s but that aren't headers.  This is a bit hokey.  Since I decided */
		/* to let users type headers, I need some way to distinguish.  Therefore, */
		/* marshal tries to know all likely headers and will indeed screw up if */
		/* the user types an unlikely one. -- presotto */
		hdrtype = -1;
		for(i = 0; i < nelem(hdrs); i++){
			if(cistrncmp(hdrs[i], p, strlen(hdrs[i])) == 0){
				*fp |= 1<<i;
				hdrtype = i;
				break;
			}
		}
		if(strict){
			if(hdrtype == -1){
				p[Blinelen(in)-1] = '\n';
				Bseek(in, -Blinelen(in), 1);
				break;
			}
		} else
			hdrtype = 0;
		p[Blinelen(in)-1] = '\n';
	}

	*sp = s;
	if(top)
		*top = to;

	if(seen == 0){
		if(Blinelen(in) == 0)
			return Nomessage;
		else
			return Ok;
	}
	if(p == nil)
		return Nobody;
	return Ok;
}

/* pass the body to sendmail, make sure body starts and ends with a newline */
void
body(Biobuf *in, Biobuf *out, int docontenttype)
{
	char *buf, *p;
	int i, n, len;

	n = 0;
	len = 16*1024;
	buf = emalloc(len);

	/* first char must be newline */
	i = Bgetc(in);
	if(i > 0){
		if(i != '\n')
			buf[n++] = '\n';
		buf[n++] = i;
	} else {
		buf[n++] = '\n';
	}

	/* read into memory */
	if(docontenttype){
		while(docontenttype){
			if(n == len){
				len += len>>2;
				buf = realloc(buf, len);
				if(buf == nil)
					sysfatal("%r");
			}
			p = buf+n;
			i = Bread(in, p, len - n);
			if(i < 0)
				fatal("input error2");
			if(i == 0)
				break;
			n += i;
			for(; i > 0; i--)
				if((*p++ & 0x80) && docontenttype){
					Bprint(out, "Content-Type: text/plain; charset=\"UTF-8\"\n");
					Bprint(out, "Content-Transfer-Encoding: 8bit\n");
					docontenttype = 0;
					break;
				}
		}
		if(docontenttype){
			Bprint(out, "Content-Type: text/plain; charset=\"US-ASCII\"\n");
			Bprint(out, "Content-Transfer-Encoding: 7bit\n");
		}
	}

	/* write what we already read */
	if(Bwrite(out, buf, n) < 0)
		fatal("output error");
	if(n > 0)
		lastchar = buf[n-1];
	else
		lastchar = '\n';


	/* pass the rest */
	for(;;){
		n = Bread(in, buf, len);
		if(n < 0)
			fatal("input error2");
		if(n == 0)
			break;
		if(Bwrite(out, buf, n) < 0)
			fatal("output error");
		lastchar = buf[n-1];
	}
}

/* pass the body to sendmail encoding with base64 */
/* */
/*  the size of buf is very important to enc64.  Anything other than */
/*  a multiple of 3 will cause enc64 to output a termination sequence. */
/*  To ensure that a full buf corresponds to a multiple of complete lines, */
/*  we make buf a multiple of 3*18 since that's how many enc64 sticks on */
/*  a single line.  This avoids short lines in the output which is pleasing */
/*  but not necessary. */
/* */
void
body64(Biobuf *in, Biobuf *out)
{
	uchar buf[3*18*54];
	char obuf[3*18*54*2];
	int m, n;

	Bprint(out, "\n");
	for(;;){
		n = Bread(in, buf, sizeof(buf));
		if(n < 0)
			fatal("input error");
		if(n == 0)
			break;
		m = enc64(obuf, sizeof(obuf), buf, n);
		if((n=Bwrite(out, obuf, m)) < 0)
			fatal("output error");
	}
	lastchar = '\n';
}

/* pass message to sendmail, make sure body starts with a newline */
void
copy(Biobuf *in, Biobuf *out)
{
	char buf[4*1024];
	int n;

	for(;;){
		n = Bread(in, buf, sizeof(buf));
		if(n < 0)
			fatal("input error");
		if(n == 0)
			break;
		if(Bwrite(out, buf, n) < 0)
			fatal("output error");
	}
}

void
attachment(Attach *a, Biobuf *out)
{
	Biobuf *f;
	char *p;

	f = emalloc(sizeof *f);
	Binit(f, a->fd, OREAD);
	/* if it's already mime encoded, just copy */
	if(strcmp(a->type, "mime") == 0){
		copy(f, out);
		Bterm(f);
		free(f);
		return;
	}

	/* if it's not already mime encoded ... */
	if(strcmp(a->type, "text/plain") != 0)
		Bprint(out, "Content-Type: %s\n", a->type);

	if(a->inline){
		Bprint(out, "Content-Disposition: inline\n");
	} else {
		p = strrchr(a->path, '/');
		if(p == nil)
			p = a->path;
		else
			p++;
		Bprint(out, "Content-Disposition: attachment; filename=%Z\n", p);
	}

	/* dump our local 'From ' line when passing along mail messages */
	if(strcmp(a->type, "message/rfc822") == 0){
		p = Brdline(f, '\n');
		if(strncmp(p, "From ", 5) != 0)
			Bseek(f, 0, 0);
	}
	if(a->ctype->display){
		body(f, out, strcmp(a->type, "text/plain") == 0);
	} else {
		Bprint(out, "Content-Transfer-Encoding: base64\n");
		body64(f, out);
	}
	Bterm(f);
	free(f);
}

char *ascwday[] =
{
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

char *ascmon[] =
{
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

int
printdate(Biobuf *b)
{
	Tm *tm;
	int tz;

	tm = localtime(time(0));
	tz = (tm->tzoff/3600)*100 + ((tm->tzoff/60)%60);

	return Bprint(b, "Date: %s, %d %s %d %2.2d:%2.2d:%2.2d %s%.4d\n",
		ascwday[tm->wday], tm->mday, ascmon[tm->mon], 1900+tm->year,
		tm->hour, tm->min, tm->sec, tz>=0?"+":"", tz);
}

int
printfrom(Biobuf *b)
{
	return Bprint(b, "From: %s\n", user);
}

int
printto(Biobuf *b, Addr *a)
{
	int i;

	if(Bprint(b, "To: %s", a->v) < 0)
		return -1;
	i = 0;
	for(a = a->next; a != nil; a = a->next)
		if(Bprint(b, "%s%s", ((i++ & 7) == 7)?",\n\t":", ", a->v) < 0)
			return -1;
	if(Bprint(b, "\n") < 0)
		return -1;
	return 0;
}

int
printcc(Biobuf *b, Addr *a)
{
	int i;

	if(a == nil)
		return 0;
	if(Bprint(b, "CC: %s", a->v) < 0)
		return -1;
	i = 0;
	for(a = a->next; a != nil; a = a->next)
		if(Bprint(b, "%s%s", ((i++ & 7) == 7)?",\n\t":", ", a->v) < 0)
			return -1;
	if(Bprint(b, "\n") < 0)
		return -1;
	return 0;
}

int
printsubject(Biobuf *b, char *subject)
{
	return Bprint(b, "Subject: %s\n", subject);
}

int
printinreplyto(Biobuf *out, char *dir)
{
	String *s;
	char buf[256];
	int fd;
	int n;

	if(mountmail() < 0)
		return -1;
	if(strncmp(dir, "Mail/", 5) != 0)
		return -1;
	s = s_copy(dir+5);
	s_append(s, "/messageid");
	fd = fsopenfd(mailfs, s_to_c(s), OREAD);
	s_free(s);
	if(fd < 0)
		return -1;
	n = readn(fd, buf, sizeof(buf)-1);
	close(fd);
	if(n <= 0)
		return -1;
	buf[n] = 0;
	return Bprint(out, "In-Reply-To: %s\n", buf);
}

int
mopen(char *file, int mode)
{
	int fd;

	if((fd = open(file, mode)) >= 0)
		return fd;
	if(strncmp(file, "Mail/", 5) == 0 && mountmail() >= 0 && (fd = fsopenfd(mailfs, file+5, mode)) >= 0)
		return fd;
	return -1;
}

Attach*
mkattach(char *file, char *type, int inline)
{
	Ctype *c;
	Attach *a;
	char ftype[64];
	char *p;
	int fd, n, pfd[2], xfd[3];

	if(file == nil)
		return nil;
	if((fd = mopen(file, OREAD)) < 0)
		return nil;
	a = emalloc(sizeof(*a));
	a->fd = fd;
	a->path = file;
	a->next = nil;
	a->type = type;
	a->inline = inline;
	a->ctype = nil;
	if(type != nil){
		for(c = ctype; ; c++)
			if(strncmp(type, c->type, strlen(c->type)) == 0){
				a->ctype = c;
				break;
			}
		return a;
	}

	/* pick a type depending on extension */
	p = strchr(file, '.');
	if(p != nil)
		p++;

	/* check the builtin extensions */
	if(p != nil){
		for(c = ctype; c->ext != nil; c++)
			if(strcmp(p, c->ext) == 0){
				a->type = c->type;
				a->ctype = c;
				return a;
			}
	}

	/* try the mime types file */
	if(p != nil){
		if(mimetypes == nil)
			readmimetypes();
		for(c = mimetypes; c != nil && c->ext != nil; c++)
			if(strcmp(p, c->ext) == 0){
				a->type = c->type;
				a->ctype = c;
				return a;
			}
	}

	/* run file to figure out the type */
	a->type = "application/octet-stream";		/* safest default */
	if(pipe(pfd) < 0)
		return a;

	xfd[0] = mopen(file, OREAD);
	xfd[1] = pfd[0];
	xfd[2] = dup(2, -1);
	if((pid=threadspawnl(xfd, unsharp("#9/bin/file"), "file", "-m", nil)) < 0){
		close(xfd[0]);
		close(xfd[1]);
		close(xfd[2]);
		return a;
	}
	/* threadspawnl closed pfd[0] */

	n = readn(pfd[1], ftype, sizeof(ftype));
	if(n > 0){
		ftype[n-1] = 0;
		a->type = estrdup(ftype);
	}
	close(pfd[1]);
	procwait(pid);

	for(c = ctype; ; c++)
		if(strncmp(a->type, c->type, strlen(c->type)) == 0){
			a->ctype = c;
			break;
		}

	return a;
}

char*
mkboundary(void)
{
	char buf[32];
	int i;

	srand((time(0)<<16)|getpid());
	strcpy(buf, "upas-");
	for(i = 5; i < sizeof(buf)-1; i++)
		buf[i] = 'a' + nrand(26);
	buf[i] = 0;
	return estrdup(buf);
}

/* copy types to two fd's */
static void
tee(int in, int out1, int out2)
{
	char buf[8*1024];
	int n;

	for(;;){
		n = read(in, buf, sizeof(buf));
		if(n <= 0)
			break;
		if(write(out1, buf, n) < 0)
			break;
		if(write(out2, buf, n) < 0)
			break;
	}
}

static void
teeproc(void *v)
{
	int *a;

	a = v;
	tee(a[0], a[1], a[2]);
	write(a[2], "\n", 1);
}

/* print the unix from line */
int
printunixfrom(int fd)
{
	Tm *tm;
	int tz;

	tm = localtime(time(0));
	tz = (tm->tzoff/3600)*100 + ((tm->tzoff/60)%60);

	return fprint(fd, "From %s %s %s %d %2.2d:%2.2d:%2.2d %s%.4d %d\n",
		user,
		ascwday[tm->wday], ascmon[tm->mon], tm->mday,
		tm->hour, tm->min, tm->sec, tz>=0?"+":"", tz, 1900+tm->year);
}

char *specialfile[] =
{
	"pipeto",
	"pipefrom",
	"L.mbox",
	"forward",
	"names"
};

/* return 1 if this is a special file */
static int
special(String *s)
{
	char *p;
	int i;

	p = strrchr(s_to_c(s), '/');
	if(p == nil)
		p = s_to_c(s);
	else
		p++;
	for(i = 0; i < nelem(specialfile); i++)
		if(strcmp(p, specialfile[i]) == 0)
			return 1;
	return 0;
}

/* open the folder using the recipients account name */
static int
openfolder(char *rcvr)
{
	char *p;
	int c;
	String *file;
	Dir *d;
	int fd;
	int scarey;

	file = s_new();
	mboxpath("f", user, file, 0);

	/* if $mail/f exists, store there, otherwise in $mail */
	d = dirstat(s_to_c(file));
	if(d == nil || d->qid.type != QTDIR){
		scarey = 1;
		file->ptr -= 1;
	} else {
		s_putc(file, '/');
		scarey = 0;
	}
	free(d);

	p = strrchr(rcvr, '!');
	if(p != nil)
		rcvr = p+1;

	while(*rcvr && *rcvr != '@'){
		c = *rcvr++;
		if(c == '/')
			c = '_';
		s_putc(file, c);
	}
	s_terminate(file);

	if(scarey && special(file)){
		fprint(2, "%s: won't overwrite %s\n", argv0, s_to_c(file));
		s_free(file);
		return -1;
	}

	fd = open(s_to_c(file), OWRITE);
	if(fd < 0)
		fd = create(s_to_c(file), OWRITE, 0660);

	s_free(file);
	return fd;
}

/* start up sendmail and return an fd to talk to it with */
int
sendmail(Addr *to, Addr *cc, int *pid, char *rcvr)
{
	char **av, **v;
	int ac, fd, *targ;
	int pfd[2], sfd, xfd[3];
	String *cmd;
	char *x;
	Addr *a;

	fd = -1;
	if(rcvr != nil)
		fd = openfolder(rcvr);

	ac = 0;
	for(a = to; a != nil; a = a->next)
		ac++;
	for(a = cc; a != nil; a = a->next)
		ac++;
	v = av = emalloc(sizeof(char*)*(ac+20));
	ac = 0;
	v[ac++] = "sendmail";
	if(xflag)
		v[ac++] = "-x";
	if(rflag)
		v[ac++] = "-r";
	if(lbflag)
		v[ac++] = "-#";
	if(dflag)
		v[ac++] = "-d";
	for(a = to; a != nil; a = a->next)
		v[ac++] = a->v;
	for(a = cc; a != nil; a = a->next)
		v[ac++] = a->v;
	v[ac] = 0;

	if(pipe(pfd) < 0)
		fatal("pipe: %r");

	xfd[0] = pfd[0];
	xfd[1] = dup(1, -1);
	xfd[2] = dup(2, -1);

	if(replymsg != nil)
		putenv("replymsg", replymsg);
	cmd = mboxpath("pipefrom", login, s_new(), 0);

	if((*pid = threadspawn(xfd, x=s_to_c(cmd), av)) < 0
	&& (*pid = threadspawn(xfd, x="myupassend", av)) < 0
	&& (*pid = threadspawn(xfd, x=unsharp("#9/bin/upas/send"), av)) < 0)
		fatal("exec: %r");
	/* threadspawn closed pfd[0] (== xfd[0]) */
	sfd = pfd[1];

	if(rcvr != nil){
		if(pipe(pfd) < 0)
			fatal("pipe: %r");
		seek(fd, 0, 2);
		printunixfrom(fd);
		targ = emalloc(3*sizeof targ[0]);
		targ[0] = sfd;
		targ[1] = pfd[0];
		targ[2] = fd;
		proccreate(teeproc, targ, STACK);
		sfd = pfd[1];
	}

	return sfd;
}

/* start up pgp process and return an fd to talk to it with. */
/* its standard output will be the original fd, which goes to sendmail. */
int
pgpfilter(int *pid, int fd, int pgpflag)
{
	char **av, **v;
	int ac;
	int pfd[2];

	v = av = emalloc(sizeof(char*)*8);
	ac = 0;
	v[ac++] = "pgp";
	v[ac++] = "-fat";		/* operate as a filter, generate text */
	if(pgpflag & PGPsign)
		v[ac++] = "-s";
	if(pgpflag & PGPencrypt)
		v[ac++] = "-e";
	v[ac] = 0;

	if(pipe(pfd) < 0)
		fatal("%r");
	switch(*pid = fork()){
	case -1:
		fatal("%r");
		break;
	case 0:
		close(pfd[1]);
		dup(pfd[0], 0);
		close(pfd[0]);
		dup(fd, 1);
		close(fd);
		/* add newline to avoid confusing pgp output with 822 headers */
		write(1, "\n", 1);

		exec("pgp", av);
		fatal("execing: %r");
		break;
	default:
		close(pfd[0]);
		break;
	}
	close(fd);
	return pfd[1];
}

/* wait for sendmail and pgp to exit; exit here if either failed */
char*
waitforsubprocs(void)
{
	Waitmsg *w;
	char *err;

	err = nil;
	if(pgppid >= 0 && (w=procwait(pgppid)) && w->msg[0])
		err = w->msg;
	if(pid >= 0 && (w=procwait(pid)) && w->msg[0])
		err = w->msg;
	return err;
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

static uchar t64d[256];
static char t64e[64];

static void
init64(void)
{
	int c, i;

	memset(t64d, 255, 256);
	memset(t64e, '=', 64);
	i = 0;
	for(c = 'A'; c <= 'Z'; c++){
		t64e[i] = c;
		t64d[c] = i++;
	}
	for(c = 'a'; c <= 'z'; c++){
		t64e[i] = c;
		t64d[c] = i++;
	}
	for(c = '0'; c <= '9'; c++){
		t64e[i] = c;
		t64d[c] = i++;
	}
	t64e[i] = '+';
	t64d['+'] = i++;
	t64e[i] = '/';
	t64d['/'] = i;
}

int
enc64(char *out, int lim, uchar *in, int n)
{
	int i;
	ulong b24;
	char *start = out;
	char *e = out + lim;

	if(t64e[0] == 0)
		init64();
	for(i = 0; i < n/3; i++){
		b24 = (*in++)<<16;
		b24 |= (*in++)<<8;
		b24 |= *in++;
		if(out + 5 >= e)
			goto exhausted;
		*out++ = t64e[(b24>>18)];
		*out++ = t64e[(b24>>12)&0x3f];
		*out++ = t64e[(b24>>6)&0x3f];
		*out++ = t64e[(b24)&0x3f];
		if((i%18) == 17)
			*out++ = '\n';
	}

	switch(n%3){
	case 2:
		b24 = (*in++)<<16;
		b24 |= (*in)<<8;
		if(out + 4 >= e)
			goto exhausted;
		*out++ = t64e[(b24>>18)];
		*out++ = t64e[(b24>>12)&0x3f];
		*out++ = t64e[(b24>>6)&0x3f];
		break;
	case 1:
		b24 = (*in)<<16;
		if(out + 4 >= e)
			goto exhausted;
		*out++ = t64e[(b24>>18)];
		*out++ = t64e[(b24>>12)&0x3f];
		*out++ = '=';
		break;
	case 0:
		if((i%18) != 0)
			*out++ = '\n';
		*out = 0;
		return out - start;
	}
exhausted:
	*out++ = '=';
	*out++ = '\n';
	*out = 0;
	return out - start;
}

void
freealias(Alias *a)
{
	freeaddrs(a->addr);
	free(a);
}

void
freealiases(Alias *a)
{
	Alias *next;

	while(a != nil){
		next = a->next;
		freealias(a);
		a = next;
	}
}

/* */
/*  read alias file */
/* */
Alias*
readaliases(void)
{
	Alias *a, **l, *first;
	Addr *addr, **al;
	String *file, *line, *token;
	Sinstack *sp;

	first = nil;
	file = s_new();
	line = s_new();
	token = s_new();

	/* open and get length */
	mboxpath("names", login, file, 0);
	sp = s_allocinstack(s_to_c(file));
	if(sp == nil)
		goto out;

	l = &first;

	/* read a line at a time. */
	while(s_rdinstack(sp, s_restart(line))!=nil) {
		s_restart(line);
		a = emalloc(sizeof(Alias));
		al = &a->addr;
		for(;;){
			if(s_parse(line, s_restart(token))==0)
				break;
			addr = emalloc(sizeof(Addr));
			addr->v = strdup(s_to_c(token));
			addr->next = 0;
			*al = addr;
			al = &addr->next;
		}
		if(a->addr == nil || a->addr->next == nil){
			freealias(a);
			continue;
		}
		a->next = nil;
		*l = a;
		l = &a->next;
	}
	s_freeinstack(sp);

out:
	s_free(file);
	s_free(line);
	s_free(token);
	return first;
}

Addr*
newaddr(char *name)
{
	Addr *a;

	a = emalloc(sizeof(*a));
	a->next = nil;
	a->v = estrdup(name);
	if(a->v == nil)
		sysfatal("%r");
	return a;
}

/* */
/*  expand personal aliases since the names are meaningless in */
/*  other contexts */
/* */
Addr*
_expand(Addr *old, int *changedp)
{
	Alias *al;
	Addr *first, *next, **l, *a;

	*changedp = 0;
	first = nil;
	l = &first;
	for(;old != nil; old = next){
		next = old->next;
		for(al = aliases; al != nil; al = al->next){
			if(strcmp(al->addr->v, old->v) == 0){
				for(a = al->addr->next; a != nil; a = a->next){
					*l = newaddr(a->v);
					if(*l == nil)
						sysfatal("%r");
					l = &(*l)->next;
					*changedp = 1;
				}
				break;
			}
		}
		if(al != nil){
			freeaddr(old);
			continue;
		}
		*l = old;
		old->next = nil;
		l = &(*l)->next;
	}
	return first;
}

Addr*
rexpand(Addr *old)
{
	int i, changed;

	changed = 0;
	for(i=0; i<32; i++){
		old = _expand(old, &changed);
		if(changed == 0)
			break;
	}
	return old;
}

Addr*
unique(Addr *first)
{
	Addr *a, **l, *x;

	for(a = first; a != nil; a = a->next){
		for(l = &a->next; *l != nil;){
			if(strcmp(a->v, (*l)->v) == 0){
				x = *l;
				*l = x->next;
				freeaddr(x);
			} else
				l = &(*l)->next;
		}
	}
	return first;
}

Addr*
expand(int ac, char **av)
{
	Addr *first, **l;
	int i;

	first = nil;

	/* make a list of the starting addresses */
	l = &first;
	for(i = 0; i < ac; i++){
		*l = newaddr(av[i]);
		if(*l == nil)
			sysfatal("%r");
		l = &(*l)->next;
	}

	/* recurse till we don't change any more */
	return unique(rexpand(first));
}

Addr*
concataddr(Addr *a, Addr *b)
{
	Addr *oa;

	if(a == nil)
		return b;

	oa = a;
	for(; a->next; a=a->next)
		;
	a->next = b;
	return oa;
}

void
freeaddr(Addr *ap)
{
	free(ap->v);
	free(ap);
}

void
freeaddrs(Addr *ap)
{
	Addr *next;

	for(; ap; ap=next) {
		next = ap->next;
		freeaddr(ap);
	}
}

String*
s_copyn(char *s, int n)
{
	return s_nappend(s_reset(nil), s, n);
}

/* fetch the next token from an RFC822 address string */
/* we assume the header is RFC822-conformant in that */
/* we recognize escaping anywhere even though it is only */
/* supposed to be in quoted-strings, domain-literals, and comments. */
/* */
/* i'd use yylex or yyparse here, but we need to preserve  */
/* things like comments, which i think it tosses away. */
/* */
/* we're not strictly RFC822 compliant.  we misparse such nonsense as */
/* */
/*	To: gre @ (Grace) plan9 . (Emlin) bell-labs.com */
/* */
/* make sure there's no whitespace in your addresses and  */
/* you'll be fine. */
/* */
enum {
	Twhite,
	Tcomment,
	Twords,
	Tcomma,
	Tleftangle,
	Trightangle,
	Terror,
	Tend
};
/*char *ty82[] = {"white", "comment", "words", "comma", "<", ">", "err", "end"}; */
#define ISWHITE(p) ((p)==' ' || (p)=='\t' || (p)=='\n' || (p)=='\r')
int
get822token(String **tok, char *p, char **pp)
{
	char *op;
	int type;
	int quoting;

	op = p;
	switch(*p){
	case '\0':
		*tok = nil;
		*pp = nil;
		return Tend;

	case ' ':	/* get whitespace */
	case '\t':
	case '\n':
	case '\r':
		type = Twhite;
		while(ISWHITE(*p))
			p++;
		break;

	case '(':	/* get comment */
		type = Tcomment;
		for(p++; *p && *p != ')'; p++)
			if(*p == '\\') {
				if(*(p+1) == '\0') {
					*tok = nil;
					return Terror;
				}
				p++;
			}

		if(*p != ')') {
			*tok = nil;
			return Terror;
		}
		p++;
		break;
	case ',':
		type = Tcomma;
		p++;
		break;
	case '<':
		type = Tleftangle;
		p++;
		break;
	case '>':
		type = Trightangle;
		p++;
		break;
	default:	/* bunch of letters, perhaps quoted strings tossed in */
		type = Twords;
		quoting = 0;
		for(; *p && (quoting || (!ISWHITE(*p) && *p != '>' && *p != '<' && *p != ',')); p++) {
			if(*p == '"')
				quoting = !quoting;
			if(*p == '\\') {
				if(*(p+1) == '\0') {
					*tok = nil;
					return Terror;
				}
				p++;
			}
		}
		break;
	}

	if(pp)
		*pp = p;
	*tok = s_copyn(op, p-op);
	return type;
}

/* expand local aliases in an RFC822 mail line */
/* add list of expanded addresses to to. */
Addr*
expandline(String **s, Addr *to)
{
	Addr *na, *nto, *ap;
	char *p;
	int tok, inangle, hadangle, nword;
	String *os, *ns, *stok, *lastword, *sinceword;

	os = s_copy(s_to_c(*s));
	p = strchr(s_to_c(*s), ':');
	assert(p != nil);
	p++;

	ns = s_copyn(s_to_c(*s), p-s_to_c(*s));
	stok = nil;
	nto = nil;
	/* */
	/* the only valid mailbox namings are word */
	/* and word* < addr > */
	/* without comments this would be simple. */
	/* we keep the following: */
	/*	lastword - current guess at the address */
	/*	sinceword - whitespace and comment seen since lastword */
	/* */
	lastword = s_new();
	sinceword = s_new();
	inangle = 0;
	nword = 0;
	hadangle = 0;
	for(;;) {
		stok = nil;
		switch(tok = get822token(&stok, p, &p)){
		default:
			abort();
		case Tcomma:
		case Tend:
			if(inangle)
				goto Error;
			if(nword != 1)
				goto Error;
			na = rexpand(newaddr(s_to_c(lastword)));
			s_append(ns, na->v);
			s_append(ns, s_to_c(sinceword));
			for(ap=na->next; ap; ap=ap->next) {
				s_append(ns, ", ");
				s_append(ns, ap->v);
			}
			nto = concataddr(na, nto);
			if(tok == Tcomma){
				s_append(ns, ",");
				s_free(stok);
			}
			if(tok == Tend)
				goto Break2;
			inangle = 0;
			nword = 0;
			hadangle = 0;
			s_reset(sinceword);
			s_reset(lastword);
			break;
		case Twhite:
		case Tcomment:
			s_append(sinceword, s_to_c(stok));
			s_free(stok);
			break;
		case Trightangle:
			if(!inangle)
				goto Error;
			inangle = 0;
			hadangle = 1;
			s_append(sinceword, s_to_c(stok));
			s_free(stok);
			break;
		case Twords:
		case Tleftangle:
			if(hadangle)
				goto Error;
			if(tok != Tleftangle && inangle && s_len(lastword))
				goto Error;
			if(tok == Tleftangle) {
				inangle = 1;
				nword = 1;
			}
			s_append(ns, s_to_c(lastword));
			s_append(ns, s_to_c(sinceword));
			s_reset(sinceword);
			if(tok == Tleftangle) {
				s_append(ns, "<");
				s_reset(lastword);
			} else {
				s_free(lastword);
				lastword = stok;
			}
			if(!inangle)
				nword++;
			break;
		case Terror:	/* give up, use old string, addrs */
		Error:
			ns = os;
			os = nil;
			freeaddrs(nto);
			nto = nil;
			werrstr("rfc822 syntax error");
			rfc822syntaxerror = 1;
			goto Break2;
		}
	}
Break2:
	s_free(*s);
	s_free(os);
	*s = ns;
	nto = concataddr(nto, to);
	return nto;
}

void
Bdrain(Biobuf *b)
{
	char buf[8192];

	while(Bread(b, buf, sizeof buf) > 0)
		;
}

void
readmimetypes(void)
{
	Biobuf *b;
	char *p;
	char *f[6];
	char type[256];
	static int alloced, inuse;

	if(mimetypes == 0){
		alloced = 256;
		mimetypes = emalloc(alloced*sizeof(Ctype));
		mimetypes[0].ext = "";
	}

	b = Bopen(unsharp("#9/lib/mimetype"), OREAD);
	if(b == nil)
		return;
	for(;;){
		p = Brdline(b, '\n');
		if(p == nil)
			break;
		p[Blinelen(b)-1] = 0;
		if(tokenize(p, f, 6) < 4)
			continue;
		if(strcmp(f[0], "-") == 0 || strcmp(f[1], "-") == 0 || strcmp(f[2], "-") == 0)
			continue;
		if(inuse + 1 >= alloced){
			alloced += 256;
			mimetypes = erealloc(mimetypes, alloced*sizeof(Ctype));
		}
		snprint(type, sizeof(type), "%s/%s", f[1], f[2]);
		mimetypes[inuse].type = estrdup(type);
		mimetypes[inuse].ext = estrdup(f[0]+1);
		mimetypes[inuse].display = !strcmp(type, "text/plain");
		inuse++;

		/* always make sure there's a terminator */
		mimetypes[inuse].ext = 0;
	}
	Bterm(b);
}

char*
estrdup(char *x)
{
	x = strdup(x);
	if(x == nil)
		fatal("memory");
	return x;
}

void*
emalloc(int n)
{
	void *x;

	x = malloc(n);
	if(x == nil)
		fatal("%r");
	return x;
}

void*
erealloc(void *x, int n)
{
	x = realloc(x, n);
	if(x == nil)
		fatal("%r");
	return x;
}

/* */
/* Formatter for %" */
/* Use double quotes to protect white space, frogs, \ and " */
/* */
enum
{
	Qok = 0,
	Qquote,
	Qbackslash
};

static int
needtoquote(Rune r)
{
	if(r >= Runeself)
		return Qquote;
	if(r <= ' ')
		return Qquote;
	if(r=='\\' || r=='"')
		return Qbackslash;
	return Qok;
}

int
doublequote(Fmt *f)
{
	char *s, *t;
	int w, quotes;
	Rune r;

	s = va_arg(f->args, char*);
	if(s == nil || *s == '\0')
		return fmtstrcpy(f, "\"\"");

	quotes = 0;
	for(t=s; *t; t+=w){
		w = chartorune(&r, t);
		quotes |= needtoquote(r);
	}
	if(quotes == 0)
		return fmtstrcpy(f, s);

	fmtrune(f, '"');
	for(t=s; *t; t+=w){
		w = chartorune(&r, t);
		if(needtoquote(r) == Qbackslash)
			fmtrune(f, '\\');
		fmtrune(f, r);
	}
	return fmtrune(f, '"');
}

int
mountmail(void)
{
	if(mailfs != nil)
		return 0;
	if((mailfs = nsmount("mail", nil)) == nil)
		return -1;
	return 0;
}

int
rfc2047fmt(Fmt *fmt)
{
	char *s, *p;

	s = va_arg(fmt->args, char*);
	if(s == nil)
		return fmtstrcpy(fmt, "");
	for(p=s; *p; p++)
		if((uchar)*p >= 0x80)
			goto hard;
	return fmtstrcpy(fmt, s);

hard:
	fmtprint(fmt, "=?utf-8?q?");
	for(p=s; *p; p++){
		if(*p == ' ')
			fmtrune(fmt, '_');
		else if(*p == '_' || *p == '\t' || *p == '=' || *p == '?' || (uchar)*p >= 0x80)
			fmtprint(fmt, "=%.2uX", (uchar)*p);
		else
			fmtrune(fmt, (uchar)*p);
	}
	fmtprint(fmt, "?=");
	return 0;
}

char*
mksubject(char *line)
{
	char *p, *q;
	static char buf[1024];

	p = strchr(line, ':')+1;
	while(*p == ' ')
		p++;
	for(q=p; *q; q++)
		if((uchar)*q >= 0x80)
			goto hard;
	return line;

hard:
	snprint(buf, sizeof buf, "Subject: %U", p);
	return buf;
}
