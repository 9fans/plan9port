#include "common.h"
#include <ctype.h>
#include <plumb.h>
#include <9pclient.h>
#include <thread.h>

#define system nedsystem
#define rcmd nedrcmd

typedef struct Message Message;
typedef struct Ctype Ctype;
typedef struct Cmd Cmd;

char	root[Pathlen];
char	mbname[Elemlen];
int	rootlen;
int	didopen;
char	*user;
char	wd[2048];
String	*mbpath;
int	natural;
int	doflush;

int interrupted;

struct Message {
	Message	*next;
	Message	*prev;
	Message	*cmd;
	Message	*child;
	Message	*parent;
	String	*path;
	int	id;
	int	len;
	int	fileno;	/* number of directory */
	String	*info;
	char	*from;
	char	*to;
	char	*cc;
	char	*replyto;
	char	*date;
	char	*subject;
	char	*type;
	char	*disposition;
	char	*filename;
	char	deleted;
	char	stored;
};

Message top;

struct Ctype {
	char	*type;
	char 	*ext;
	int	display;
	char	*plumbdest;
	Ctype	*next;
};

Ctype ctype[] = {
	{ "text/plain",			"txt",	1,	0	},
	{ "text/html",			"htm",	1,	0	},
	{ "text/html",			"html",	1,	0	},
	{ "text/tab-separated-values",	"tsv",	1,	0	},
	{ "text/richtext",		"rtx",	1,	0	},
	{ "text/rtf",			"rtf",	1,	0	},
	{ "text",			"txt",	1,	0	},
	{ "message/rfc822",		"msg",	0,	0	},
	{ "message/delivery-status",	"txt",	1,	0	},
	{ "image/bmp",			"bmp",	0,	"image"	},
	{ "image/jpeg",			"jpg",	0,	"image"	},
	{ "image/gif",			"gif",	0,	"image"	},
	{ "image/png",			"png",	0,	"image"	},
	{ "application/pdf",		"pdf",	0,	"postscript"	},
	{ "application/postscript",	"ps",	0,	"postscript"	},
	{ "application/",		0,	0,	0	},
	{ "image/",			0,	0,	0	},
	{ "multipart/",			"mul",	0,	0	},

};

Message*	acmd(Cmd*, Message*);
Message*	bcmd(Cmd*, Message*);
Message*	dcmd(Cmd*, Message*);
Message*	eqcmd(Cmd*, Message*);
Message*	hcmd(Cmd*, Message*);
Message*	Hcmd(Cmd*, Message*);
Message*	helpcmd(Cmd*, Message*);
Message*	icmd(Cmd*, Message*);
Message*	pcmd(Cmd*, Message*);
Message*	qcmd(Cmd*, Message*);
Message*	rcmd(Cmd*, Message*);
Message*	scmd(Cmd*, Message*);
Message*	ucmd(Cmd*, Message*);
Message*	wcmd(Cmd*, Message*);
Message*	xcmd(Cmd*, Message*);
Message*	ycmd(Cmd*, Message*);
Message*	pipecmd(Cmd*, Message*);
Message*	rpipecmd(Cmd*, Message*);
Message*	bangcmd(Cmd*, Message*);
Message*	Pcmd(Cmd*, Message*);
Message*	mcmd(Cmd*, Message*);
Message*	fcmd(Cmd*, Message*);
Message*	quotecmd(Cmd*, Message*);

struct {
	char		*cmd;
	int		args;
	Message*	(*f)(Cmd*, Message*);
	char		*help;
} cmdtab[] = {
	{ "a",	1,	acmd,	"a        reply to sender and recipients" },
	{ "A",	1,	acmd,	"A        reply to sender and recipients with copy" },
	{ "b",	0,	bcmd,	"b        print the next 10 headers" },
	{ "d",	0,	dcmd,	"d        mark for deletion" },
	{ "f",	0,	fcmd,	"f        file message by from address" },
	{ "h",	0,	hcmd,	"h        print elided message summary (,h for all)" },
	{ "help", 0,	helpcmd, "help     print this info" },
	{ "H",	0,	Hcmd,	"H        print message's MIME structure " },
	{ "i",	0,	icmd,	"i        incorporate new mail" },
	{ "m",	1,	mcmd,	"m addr   forward mail" },
	{ "M",	1,	mcmd,	"M addr   forward mail with message" },
	{ "p",	0,	pcmd,	"p        print the processed message" },
	{ "P",	0,	Pcmd,	"P        print the raw message" },
	{ "\"",	0,	quotecmd, "\"        print a quoted version of msg" },
	{ "q",	0,	qcmd,	"q        exit and remove all deleted mail" },
	{ "r",	1,	rcmd,	"r [addr] reply to sender plus any addrs specified" },
	{ "rf",	1,	rcmd,	"rf [addr]file message and reply" },
	{ "R",	1,	rcmd,	"R [addr] reply including copy of message" },
	{ "Rf",	1,	rcmd,	"Rf [addr]file message and reply with copy" },
	{ "s",	1,	scmd,	"s file   append raw message to file" },
	{ "u",	0,	ucmd,	"u        remove deletion mark" },
	{ "w",	1,	wcmd,	"w file   store message contents as file" },
	{ "x",	0,	xcmd,	"x        exit without flushing deleted messages" },
	{ "y",	0,	ycmd,	"y        synchronize with mail box" },
	{ "=",	1,	eqcmd,	"=        print current message number" },
	{ "|",	1,	pipecmd, "|cmd     pipe message body to a command" },
	{ "||",	1,	rpipecmd, "||cmd     pipe raw message to a command" },
	{ "!",	1,	bangcmd, "!cmd     run a command" },
	{ nil,	0,	nil, 	nil }
};

enum
{
	NARG=	32
};

struct Cmd {
	Message	*msgs;
	Message	*(*f)(Cmd*, Message*);
	int	an;
	char	*av[NARG];
	int	delete;
};

Biobuf out;
int startedfs;
int reverse;
int longestfrom = 12;

String*		file2string(String*, char*);
int		dir2message(Message*, int);
int		filelen(String*, char*);
String*		extendpath(String*, char*);
void		snprintheader(char*, int, Message*);
void		cracktime(char*, char*, int);
int		cistrncmp(char*, char*, int);
int		cistrcmp(char*, char*);
Reprog*		parsesearch(char**);
char*		parseaddr(char**, Message*, Message*, Message*, Message**);
char*		parsecmd(char*, Cmd*, Message*, Message*);
char*		readline(char*, char*, int);
void		messagecount(Message*);
void		system(char*, char**, int);
void		mkid(String*, Message*);
int		switchmb(char*, char*);
void		closemb(void);
int		lineize(char*, char**, int);
int		rawsearch(Message*, Reprog*);
Message*	dosingleton(Message*, char*);
String*		rooted(String*);
int		plumb(Message*, Ctype*);
String*		addrecolon(char*);
void		exitfs(char*);
Message*	flushdeleted(Message*);

CFsys *mailfs;

void
usage(void)
{
	fprint(2, "usage: %s [-nr] [-f mailfile] [-s mailfile]\n", argv0);
	fprint(2, "       %s -c dir\n", argv0);
	threadexitsall("usage");
}

void
catchnote(void *x, char *note)
{
	USED(x);

	if(strstr(note, "interrupt") != nil){
		interrupted = 1;
		noted(NCONT);
	}
	noted(NDFLT);
}

char *
plural(int n)
{
	if (n == 1)
		return "";

	return "s";
}

void
threadmain(int argc, char **argv)
{
	Message *cur, *m, *x;
	char cmdline[4*1024];
	Cmd cmd;
	Ctype *cp;
	char *err;
	int n, cflag;
	String *prompt;
	char *file, *singleton, *service;

	Binit(&out, 1, OWRITE);

	file = nil;
	singleton = nil;
	reverse = 1;
	cflag = 0;
	service = "mail";
	ARGBEGIN {
	case 'S':
		service = EARGF(usage());
		break;
	case 'c':
		cflag = 1;
		break;
	case 'f':
		file = EARGF(usage());
		break;
	case 's':
		singleton = EARGF(usage());
		break;
	case 'r':
		reverse = 0;
		break;
	case 'n':
		natural = 1;
		reverse = 0;
		break;
	default:
		usage();
		break;
	} ARGEND;

	user = getlog();
	if(user == nil || *user == 0)
		sysfatal("can't read user name");

	if(cflag){
		if(argc > 0)
			creatembox(user, argv[0]);
		else
			creatembox(user, nil);
		threadexitsall(0);
	}

	if(argc)
		usage();
	if((mailfs = nsmount(service, nil)) == nil)
		sysfatal("cannot mount %s: %r", service);

	switchmb(file, singleton);

	top.path = s_copy(root);

	for(cp = ctype; cp < ctype+nelem(ctype)-1; cp++)
		cp->next = cp+1;

	if(singleton != nil){
		cur = dosingleton(&top, singleton);
		if(cur == nil){
			Bprint(&out, "no message\n");
			exitfs(0);
		}
		pcmd(nil, cur);
	} else {
		cur = &top;
		n = dir2message(&top, reverse);
		if(n < 0)
			sysfatal("can't read %s", s_to_c(top.path));
		Bprint(&out, "%d message%s\n", n, plural(n));
	}


	notify(catchnote);
	prompt = s_new();
	for(;;){
		s_reset(prompt);
		if(cur == &top)
			s_append(prompt, ": ");
		else {
			mkid(prompt, cur);
			s_append(prompt, ": ");
		}

		/* leave space at the end of cmd line in case parsecmd needs to */
		/* add a space after a '|' or '!' */
		if(readline(s_to_c(prompt), cmdline, sizeof(cmdline)-1) == nil)
			break;
		err = parsecmd(cmdline, &cmd, top.child, cur);
		if(err != nil){
			Bprint(&out, "!%s\n", err);
			continue;
		}
		if(singleton != nil && cmd.f == icmd){
			Bprint(&out, "!illegal command\n");
			continue;
		}
		interrupted = 0;
		if(cmd.msgs == nil || cmd.msgs == &top){
			x = (*cmd.f)(&cmd, &top);
			if(x != nil)
				cur = x;
		} else for(m = cmd.msgs; m != nil; m = m->cmd){
			x = m;
			if(cmd.delete){
				dcmd(&cmd, x);

				/* dp acts differently than all other commands */
				/* since its an old lesk idiom that people love. */
				/* it deletes the current message, moves the current */
				/* pointer ahead one and prints. */
				if(cmd.f == pcmd){
					if(x->next == nil){
						Bprint(&out, "!address\n");
						cur = x;
						break;
					} else
						x = x->next;
				}
			}
			x = (*cmd.f)(&cmd, x);
			if(x != nil)
				cur = x;
			if(interrupted)
				break;
			if(singleton != nil && (cmd.delete || cmd.f == dcmd))
				qcmd(nil, nil);
		}
		if(doflush)
			cur = flushdeleted(cur);
	}
	qcmd(nil, nil);
}

static char*
mkaddrs(char *t)
{
	int i, nf, inquote;
	char **f, *s;
	Fmt fmt;

	inquote = 0;
	nf = 2;
	for(s=t; *s; s++){
		if(*s == '\'')
			inquote = !inquote;
		if(*s == ' ' && !inquote)
			nf++;
	}
	f = malloc(nf*sizeof f[0]);
	if(f == nil)
		return nil;
	nf = tokenize(t, f, nf);
	fmtstrinit(&fmt);
	for(i=0; i+1<nf; i+=2){
		if(i > 0)
			fmtprint(&fmt, " ");
	/*	if(f[i][0] == 0 || strcmp(f[i], f[i+1]) == 0) */
			fmtprint(&fmt, "%s", f[i+1]);
	/*	else */
	/*		fmtprint(&fmt, "%s <%s>", f[i], f[i+1]); */
	}
	free(f);
	return fmtstrflush(&fmt);
}

/* */
/* read the message info */
/* */
Message*
file2message(Message *parent, char *name)
{
	Message *m;
	String *path;
	char *f[30], *s, *t;
	int i, nf;

	m = mallocz(sizeof(Message), 1);
	if(m == nil)
		return nil;
	m->path = path = extendpath(parent->path, name);
	m->fileno = atoi(name);
	m->info = file2string(path, "info");
	m->from = "";
	m->to = "";
	m->cc = "";
	m->replyto = "";
	m->date = "";
	m->subject = "";
	m->type = "";
	m->disposition = "";
	m->filename = "";
	nf = lineize(s_to_c(m->info), f, nelem(f));
	for(i=0; i<nf; i++){
		s = f[i];
		t = strchr(f[i], ' ');
		if(t == nil)
			continue;
		*t++ = 0;

		if(strcmp(s, "from") == 0)
			m->from = mkaddrs(t);
		else if(strcmp(s, "to") == 0)
			m->to = mkaddrs(t);
		else if(strcmp(s, "cc") == 0)
			m->cc = mkaddrs(t);
		else if(strcmp(s, "replyto") == 0)
			m->replyto = mkaddrs(t);
		else if(strcmp(s, "unixdate") == 0 && (t=strchr(t, ' ')) != nil)
			m->date = t;
		else if(strcmp(s, "subject") == 0)
			m->subject = t;
		else if(strcmp(s, "type") == 0)
			m->type = t;
		else if(strcmp(s, "disposition") == 0)
			m->disposition = t;
		else if(strcmp(s, "filename") == 0)
			m->filename = t;
	}
	m->len = filelen(path, "raw");
	if(strstr(m->type, "multipart") != nil || strcmp(m->type, "message/rfc822") == 0)
		dir2message(m, 0);
	m->parent = parent;

	return m;
}

void
freemessage(Message *m)
{
	Message *nm, *next;

	for(nm = m->child; nm != nil; nm = next){
		next = nm->next;
		freemessage(nm);
	}
	s_free(m->path);
	s_free(m->info);
	free(m);
}

/* */
/*  read a directory into a list of messages */
/* */
int
dir2message(Message *parent, int reverse)
{
	int i, n, highest, newmsgs;
	CFid *fd;

	Dir *d;
	Message *first, *last, *m;

	fd = fsopen(mailfs, s_to_c(parent->path), OREAD);
	if(fd == nil)
		return -1;

	/* count current entries */
	first = parent->child;
	highest = newmsgs = 0;
	for(last = parent->child; last != nil && last->next != nil; last = last->next)
		if(last->fileno > highest)
			highest = last->fileno;
	if(last != nil)
		if(last->fileno > highest)
			highest = last->fileno;

	n = fsdirreadall(fd, &d);
	for(i = 0; i < n; i++){
		if((d[i].qid.type & QTDIR) == 0)
			continue;
		if(atoi(d[i].name) <= highest)
			continue;
		m = file2message(parent, d[i].name);
		/* fprint(2,"returned from file2message\n"); */
		if(m == nil)
			break;
		newmsgs++;
		if(reverse){
			m->next = first;
			if(first != nil)
				first->prev = m;
			first = m;
		} else {
			if(first == nil)
				first = m;
			else
				last->next = m;
			m->prev = last;
			last = m;
		}
	}
	free(d);
	fsclose(fd);
	parent->child = first;

	/* renumber and file longest from */
	i = 1;
	longestfrom = 12;
	for(m = first; m != nil; m = m->next){
		m->id = natural ? m->fileno : i++;
		n = strlen(m->from);
		if(n > longestfrom)
			longestfrom = n;
	}

	return newmsgs;
}

/* */
/*  point directly to a message */
/* */
Message*
dosingleton(Message *parent, char *path)
{
	char *p, *np;
	Message *m;

	/* walk down to message and read it */
	if(strlen(path) < rootlen)
		return nil;
	if(path[rootlen] != '/')
		return nil;
	p = path+rootlen+1;
	np = strchr(p, '/');
	if(np != nil)
		*np = 0;
	m = file2message(parent, p);
	if(m == nil)
		return nil;
	parent->child = m;
	m->id = 1;

	/* walk down to requested component */
	while(np != nil){
		*np = '/';
		np = strchr(np+1, '/');
		if(np != nil)
			*np = 0;
		for(m = m->child; m != nil; m = m->next)
			if(strcmp(path, s_to_c(m->path)) == 0)
				return m;
		if(m == nil)
			return nil;
	}
	return m;
}

/* */
/*  read a file into a string */
/* */
String*
file2string(String *dir, char *file)
{
	String *s;
	int n, m;
	CFid *fd;

	s = extendpath(dir, file);
	fd = fsopen(mailfs, s_to_c(s), OREAD);
	s_grow(s, 512);			/* avoid multiple reads on info files */
	s_reset(s);
	if(fd == nil)
		return s;

	for(;;){
		n = s->end - s->ptr;
		if(n == 0){
			s_grow(s, 128);
			continue;
		}
		m = fsread(fd, s->ptr, n);
		if(m <= 0)
			break;
		s->ptr += m;
		if(m < n)
			break;
	}
	s_terminate(s);
	fsclose(fd);

	return s;
}

/* */
/*  get the length of a file */
/* */
int
filelen(String *dir, char *file)
{
	String *path;
	Dir *d;
	int rv;

	path = extendpath(dir, file);
	d = fsdirstat(mailfs, s_to_c(path));
	if(d == nil){
		s_free(path);
		return -1;
	}
	s_free(path);
	rv = d->length;
	free(d);
	return rv;
}

/* */
/*  walk the path name an element */
/* */
String*
extendpath(String *dir, char *name)
{
	String *path;

	if(strcmp(s_to_c(dir), ".") == 0)
		path = s_new();
	else {
		path = s_copy(s_to_c(dir));
		s_append(path, "/");
	}
	s_append(path, name);
	return path;
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

char*
nosecs(char *t)
{
	char *p;

	p = strchr(t, ':');
	if(p == nil)
		return t;
	p = strchr(p+1, ':');
	if(p != nil)
		*p = 0;
	return t;
}

char *months[12] =
{
	"jan", "feb", "mar", "apr", "may", "jun",
	"jul", "aug", "sep", "oct", "nov", "dec"
};

int
month(char *m)
{
	int i;

	for(i = 0; i < 12; i++)
		if(cistrcmp(m, months[i]) == 0)
			return i+1;
	return 1;
}

enum
{
	Yearsecs= 365*24*60*60
};

void
cracktime(char *d, char *out, int len)
{
	char in[64];
	char *f[6];
	int n;
	Tm tm;
	long now, then;
	char *dtime;

	*out = 0;
	if(d == nil)
		return;
	strncpy(in, d, sizeof(in));
	in[sizeof(in)-1] = 0;
	n = getfields(in, f, 6, 1, " \t\r\n");
	if(n != 6){
		/* unknown style */
		snprint(out, 16, "%10.10s", d);
		return;
	}
	now = time(0);
	memset(&tm, 0, sizeof tm);
	if(strchr(f[0], ',') != nil && strchr(f[4], ':') != nil){
		/* 822 style */
		tm.year = atoi(f[3])-1900;
		tm.mon = month(f[2]);
		tm.mday = atoi(f[1]);
		dtime = nosecs(f[4]);
		then = tm2sec(&tm);
	} else if(strchr(f[3], ':') != nil){
		/* unix style */
		tm.year = atoi(f[5])-1900;
		tm.mon = month(f[1]);
		tm.mday = atoi(f[2]);
		dtime = nosecs(f[3]);
		then = tm2sec(&tm);
	} else {
		then = now;
		tm = *localtime(now);
		dtime = "";
	}

	if(now - then < Yearsecs/2)
		snprint(out, len, "%2d/%2.2d %s", tm.mon, tm.mday, dtime);
	else
		snprint(out, len, "%2d/%2.2d  %4d", tm.mon, tm.mday, tm.year+1900);
}

Ctype*
findctype(Message *m)
{
	char *p;
	char ftype[128];
	int n, pfd[2];
	Ctype *a, *cp;
	static Ctype bintype 	= { "application/octet-stream", "bin", 0, 0 };

	for(cp = ctype; cp; cp = cp->next)
		if(strncmp(cp->type, m->type, strlen(cp->type)) == 0)
			return cp;

	if(pipe(pfd) < 0)
		return &bintype;

	*ftype = 0;
	switch(fork()){
	case -1:
		break;
	case 0:
		close(pfd[1]);
		close(0);
		dup(pfd[0], 0);
		close(1);
		dup(pfd[0], 1);
		execl(unsharp("#9/bin/file"), "file", "-m", s_to_c(extendpath(m->path, "body")), nil);
		threadexits(0);
	default:
		close(pfd[0]);
		n = read(pfd[1], ftype, sizeof(ftype));
		if(n > 0)
			ftype[n] = 0;
		close(pfd[1]);
		waitpid();
		break;
	}

	if (*ftype=='\0' || (p = strchr(ftype, '/')) == nil)
		return &bintype;
	*p++ = 0;

	a = mallocz(sizeof(Ctype), 1);
	a->type = strdup(ftype);
	a->ext = strdup(p);
	a->display = 0;
	a->plumbdest = strdup(ftype);
	for(cp = ctype; cp->next; cp = cp->next)
		continue;
	cp->next = a;
	a->next = nil;
	return a;
}

void
mkid(String *s, Message *m)
{
	char buf[32];

	if(m->parent != &top){
		mkid(s, m->parent);
		s_append(s, ".");
	}
	sprint(buf, "%d", m->id);
	s_append(s, buf);
}

void
snprintheader(char *buf, int len, Message *m)
{
	char timebuf[32];
	String *id;
	char *p, *q;;

	/* create id */
	id = s_new();
	mkid(id, m);

	if(*m->from == 0){
		/* no from */
		snprint(buf, len, "%-3s    %s %6d  %s",
			s_to_c(id),
			m->type,
			m->len,
			m->filename);
	} else if(*m->subject){
		q = p = strdup(m->subject);
		while(*p == ' ')
			p++;
		if(strlen(p) > 50)
			p[50] = 0;
		cracktime(m->date, timebuf, sizeof(timebuf));
		snprint(buf, len, "%-3s %c%c%c %6d  %11.11s %-*.*s %s",
			s_to_c(id),
			m->child ? 'H' : ' ',
			m->deleted ? 'd' : ' ',
			m->stored ? 's' : ' ',
			m->len,
			timebuf,
			longestfrom, longestfrom, m->from,
			p);
		free(q);
	} else {
		cracktime(m->date, timebuf, sizeof(timebuf));
		snprint(buf, len, "%-3s %c%c%c %6d  %11.11s %s",
			s_to_c(id),
			m->child ? 'H' : ' ',
			m->deleted ? 'd' : ' ',
			m->stored ? 's' : ' ',
			m->len,
			timebuf,
			m->from);
	}
	s_free(id);
}

char *spaces = "                                                                    ";

void
snprintHeader(char *buf, int len, int indent, Message *m)
{
	String *id;
	char typeid[64];
	char *p, *e;

	/* create id */
	id = s_new();
	mkid(id, m);

	e = buf + len;

	snprint(typeid, sizeof typeid, "%s    %s", s_to_c(id), m->type);
	if(indent < 6)
		p = seprint(buf, e, "%-32s %-6d ", typeid, m->len);
	else
		p = seprint(buf, e, "%-64s %-6d ", typeid, m->len);
	if(m->filename && *m->filename)
		p = seprint(p, e, "(file,%s)", m->filename);
	if(m->from && *m->from)
		p = seprint(p, e, "(from,%s)", m->from);
	if(m->subject && *m->subject)
		seprint(p, e, "(subj,%s)", m->subject);

	s_free(id);
}

char sstring[256];

/*	cmd := range cmd ' ' arg-list ;  */
/*	range := address */
/*		| address ',' address */
/*		| 'g' search ; */
/*	address := msgno */
/*		| search ; */
/*	msgno := number */
/*		| number '/' msgno ; */
/*	search := '/' string '/' */
/*		| '%' string '%' ; */
/* */
Reprog*
parsesearch(char **pp)
{
	char *p, *np;
	int c, n;

	p = *pp;
	c = *p++;
	np = strchr(p, c);
	if(np != nil){
		*np++ = 0;
		*pp = np;
	} else {
		n = strlen(p);
		*pp = p + n;
	}
	if(*p == 0)
		p = sstring;
	else{
		strncpy(sstring, p, sizeof(sstring));
		sstring[sizeof(sstring)-1] = 0;
	}
	return regcomp(p);
}

char*
parseaddr(char **pp, Message *first, Message *cur, Message *unspec, Message **mp)
{
	int n;
	Message *m;
	char *p;
	Reprog *prog;
	int c, sign;
	char buf[256];

	*mp = nil;
	p = *pp;

	if(*p == '+'){
		sign = 1;
		p++;
		*pp = p;
	} else if(*p == '-'){
		sign = -1;
		p++;
		*pp = p;
	} else
		sign = 0;

	switch(*p){
	default:
		if(sign){
			n = 1;
			goto number;
		}
		*mp = unspec;
		break;
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		n = strtoul(p, pp, 10);
		if(n == 0){
			if(sign)
				*mp = cur;
			else
				*mp = &top;
			break;
		}
	number:
		m = nil;
		switch(sign){
		case 0:
			for(m = first; m != nil; m = m->next)
				if(m->id == n)
					break;
			break;
		case -1:
			if(cur != &top)
				for(m = cur; m != nil && n > 0; n--)
					m = m->prev;
			break;
		case 1:
			if(cur == &top){
				n--;
				cur = first;
			}
			for(m = cur; m != nil && n > 0; n--)
				m = m->next;
			break;
		}
		if(m == nil)
			return "address";
		*mp = m;
		break;
	case '%':
	case '/':
	case '?':
		c = *p;
		prog = parsesearch(pp);
		if(prog == nil)
			return "badly formed regular expression";
		m = nil;
		switch(c){
		case '%':
			for(m = cur == &top ? first : cur->next; m != nil; m = m->next){
				if(rawsearch(m, prog))
					break;
			}
			break;
		case '/':
			for(m = cur == &top ? first : cur->next; m != nil; m = m->next){
				snprintheader(buf, sizeof(buf), m);
				if(regexec(prog, buf, nil, 0))
					break;
			}
			break;
		case '?':
			for(m = cur == &top ? nil : cur->prev; m != nil; m = m->prev){
				snprintheader(buf, sizeof(buf), m);
				if(regexec(prog, buf, nil, 0))
					break;
			}
			break;
		}
		if(m == nil)
			return "search";
		*mp = m;
		free(prog);
		break;
	case '$':
		for(m = first; m != nil && m->next != nil; m = m->next)
			;
		*mp = m;
		*pp = p+1;
		break;
	case '.':
		*mp = cur;
		*pp = p+1;
		break;
	case ',':
		*mp = first;
		*pp = p;
		break;
	}

	if(*mp != nil && **pp == '.'){
		(*pp)++;
		if((*mp)->child == nil)
			return "no sub parts";
		return parseaddr(pp, (*mp)->child, (*mp)->child, (*mp)->child, mp);
	}
	if(**pp == '+' || **pp == '-' || **pp == '/' || **pp == '%')
		return parseaddr(pp, first, *mp, *mp, mp);

	return nil;
}

/* */
/*  search a message for a regular expression match */
/* */
int
rawsearch(Message *m, Reprog *prog)
{
	char buf[4096+1];
	int i, rv;
	CFid *fd;
	String *path;

	path = extendpath(m->path, "raw");
	fd = fsopen(mailfs, s_to_c(path), OREAD);
	if(fd == nil)
		return 0;

	/* march through raw message 4096 bytes at a time */
	/* with a 128 byte overlap to chain the re search. */
	rv = 0;
	for(;;){
		i = fsread(fd, buf, sizeof(buf)-1);
		if(i <= 0)
			break;
		buf[i] = 0;
		if(regexec(prog, buf, nil, 0)){
			rv = 1;
			break;
		}
		if(i < sizeof(buf)-1)
			break;
		if(fsseek(fd, -128LL, 1) < 0)
			break;
	}

	fsclose(fd);
	s_free(path);
	return rv;
}


char*
parsecmd(char *p, Cmd *cmd, Message *first, Message *cur)
{
	Reprog *prog;
	Message *m, *s, *e, **l, *last;
	char buf[256];
	char *err;
	int i, c;
	char *q;
	static char errbuf[Errlen];

	cmd->delete = 0;
	l = &cmd->msgs;
	*l = nil;

	/* eat white space */
	while(*p == ' ')
		p++;

	/* null command is a special case (advance and print) */
	if(*p == 0){
		if(cur == &top){
			/* special case */
			m = first;
		} else {
			/* walk to the next message even if we have to go up */
			m = cur->next;
			while(m == nil && cur->parent != nil){
				cur = cur->parent;
				m = cur->next;
			}
		}
		if(m == nil)
			return "address";
		*l = m;
		m->cmd = nil;
		cmd->an = 0;
		cmd->f = pcmd;
		return nil;
	}

	/* global search ? */
	if(*p == 'g'){
		p++;

		/* no search string means all messages */
		if(*p != '/' && *p != '%'){
			for(m = first; m != nil; m = m->next){
				*l = m;
				l = &m->cmd;
				*l = nil;
			}
		} else {
			/* mark all messages matching this search string */
			c = *p;
			prog = parsesearch(&p);
			if(prog == nil)
				return "badly formed regular expression";
			if(c == '%'){
				for(m = first; m != nil; m = m->next){
					if(rawsearch(m, prog)){
						*l = m;
						l = &m->cmd;
						*l = nil;
					}
				}
			} else {
				for(m = first; m != nil; m = m->next){
					snprintheader(buf, sizeof(buf), m);
					if(regexec(prog, buf, nil, 0)){
						*l = m;
						l = &m->cmd;
						*l = nil;
					}
				}
			}
			free(prog);
		}
	} else {

		/* parse an address */
		s = e = nil;
		err = parseaddr(&p, first, cur, cur, &s);
		if(err != nil)
			return err;
		if(*p == ','){
			/* this is an address range */
			if(s == &top)
				s = first;
			p++;
			for(last = s; last != nil && last->next != nil; last = last->next)
				;
			err = parseaddr(&p, first, cur, last, &e);
			if(err != nil)
				return err;

			/* select all messages in the range */
			for(; s != nil; s = s->next){
				*l = s;
				l = &s->cmd;
				*l = nil;
				if(s == e)
					break;
			}
			if(s == nil)
				return "null address range";
		} else {
			/* single address */
			if(s != &top){
				*l = s;
				s->cmd = nil;
			}
		}
	}

	/* insert a space after '!'s and '|'s */
	for(q = p; *q; q++)
		if(*q != '!' && *q != '|')
			break;
	if(q != p && *q != ' '){
		memmove(q+1, q, strlen(q)+1);
		*q = ' ';
	}

	cmd->an = getfields(p, cmd->av, nelem(cmd->av) - 1, 1, " \t\r\n");
	if(cmd->an == 0 || *cmd->av[0] == 0)
		cmd->f = pcmd;
	else {
		/* hack to allow all messages to start with 'd' */
		if(*(cmd->av[0]) == 'd' && *(cmd->av[0]+1) != 0){
			cmd->delete = 1;
			cmd->av[0]++;
		}

		/* search command table */
		for(i = 0; cmdtab[i].cmd != nil; i++)
			if(strcmp(cmd->av[0], cmdtab[i].cmd) == 0)
				break;
		if(cmdtab[i].cmd == nil)
			return "illegal command";
		if(cmdtab[i].args == 0 && cmd->an > 1){
			snprint(errbuf, sizeof(errbuf), "%s doesn't take an argument", cmdtab[i].cmd);
			return errbuf;
		}
		cmd->f = cmdtab[i].f;
	}
	return nil;
}

/* inefficient read from standard input */
char*
readline(char *prompt, char *line, int len)
{
	char *p, *e;
	int n;

retry:
	interrupted = 0;
	Bprint(&out, "%s", prompt);
	Bflush(&out);
	e = line + len;
	for(p = line; p < e; p++){
		n = read(0, p, 1);
		if(n < 0){
			if(interrupted)
				goto retry;
			return nil;
		}
		if(n == 0)
			return nil;
		if(*p == '\n')
			break;
	}
	*p = 0;
	return line;
}

void
messagecount(Message *m)
{
	int i;

	i = 0;
	for(; m != nil; m = m->next)
		i++;
	Bprint(&out, "%d message%s\n", i, plural(i));
}

Message*
aichcmd(Message *m, int indent)
{
	char	hdr[256];

	if(m == &top)
		return nil;

	snprintHeader(hdr, sizeof(hdr), indent, m);
	Bprint(&out, "%s\n", hdr);
	for(m = m->child; m != nil; m = m->next)
		aichcmd(m, indent+1);
	return nil;
}

Message*
Hcmd(Cmd *x, Message *m)
{
	USED(x);

	if(m == &top)
		return nil;
	aichcmd(m, 0);
	return nil;
}

Message*
hcmd(Cmd *x, Message *m)
{
	char	hdr[256];

	USED(x);
	if(m == &top)
		return nil;

	snprintheader(hdr, sizeof(hdr), m);
	Bprint(&out, "%s\n", hdr);
	return nil;
}

Message*
bcmd(Cmd *x, Message *m)
{
	int i;
	Message *om = m;

	USED(x);
	if(m == &top)
		m = top.child;
	for(i = 0; i < 10 && m != nil; i++){
		hcmd(nil, m);
		om = m;
		m = m->next;
	}

	return om;
}

Message*
ncmd(Cmd *x, Message *m)
{
	USED(x);
	if(m == &top)
		return m->child;
	return m->next;
}

int
printpart(String *s, char *part)
{
	char buf[4096];
	int n, tot;
	CFid *fd;
	String *path;

	path = extendpath(s, part);
	fd = fsopen(mailfs, s_to_c(path), OREAD);
	s_free(path);
	if(fd == nil){
		fprint(2, "!message dissappeared\n");
		return 0;
	}
	tot = 0;
	while((n = fsread(fd, buf, sizeof(buf))) > 0){
		if(interrupted)
			break;
		if(Bwrite(&out, buf, n) <= 0)
			break;
		tot += n;
	}
	fsclose(fd);
	return tot;
}

int
printhtml(Message *m)
{
	Cmd c;

	c.an = 3;
	c.av[1] = "htmlfmt";
	c.av[2] = "-l 40 -cutf-8";
	Bprint(&out, "!%s\n", c.av[1]);
	Bflush(&out);
	pipecmd(&c, m);
	return 0;
}

Message*
Pcmd(Cmd *x, Message *m)
{
	USED(x);
	if(m == &top)
		return &top;
	if(m->parent == &top)
		printpart(m->path, "unixheader");
	printpart(m->path, "raw");
	return m;
}

void
compress(char *p)
{
	char *np;
	int last;

	last = ' ';
	for(np = p; *p; p++){
		if(*p != ' ' || last != ' '){
			last = *p;
			*np++ = last;
		}
	}
	*np = 0;
}

Message*
pcmd(Cmd *x, Message *m)
{
	Message *nm;
	Ctype *cp;
	String *s;
	char buf[128];

	USED(x);
	if(m == &top)
		return &top;
	if(m->parent == &top)
		printpart(m->path, "unixheader");
	if(printpart(m->path, "header") > 0)
		Bprint(&out, "\n");
	cp = findctype(m);
	if(cp->display){
		if(strcmp(m->type, "text/html") == 0)
			printhtml(m);
		else
			printpart(m->path, "body");
	} else if(strcmp(m->type, "multipart/alternative") == 0){
		for(nm = m->child; nm != nil; nm = nm->next){
			cp = findctype(nm);
			if(cp->ext != nil && strncmp(cp->ext, "txt", 3) == 0)
				break;
		}
		if(nm == nil)
			for(nm = m->child; nm != nil; nm = nm->next){
				cp = findctype(nm);
				if(cp->display)
					break;
			}
		if(nm != nil)
			pcmd(nil, nm);
		else
			hcmd(nil, m);
	} else if(strncmp(m->type, "multipart/", 10) == 0){
		nm = m->child;
		if(nm != nil){
			/* always print first part */
			pcmd(nil, nm);

			for(nm = nm->next; nm != nil; nm = nm->next){
				s = rooted(s_clone(nm->path));
				cp = findctype(nm);
				snprintHeader(buf, sizeof buf, -1, nm);
				compress(buf);
				if(strcmp(nm->disposition, "inline") == 0){
					if(cp->ext != nil)
						Bprint(&out, "\n--- %s %s/body.%s\n\n",
							buf, s_to_c(s), cp->ext);
					else
						Bprint(&out, "\n--- %s %s/body\n\n",
							buf, s_to_c(s));
					pcmd(nil, nm);
				} else {
					if(cp->ext != nil)
						Bprint(&out, "\n!--- %s %s/body.%s\n",
							buf, s_to_c(s), cp->ext);
					else
						Bprint(&out, "\n!--- %s %s/body\n",
							buf, s_to_c(s));
				}
				s_free(s);
			}
		} else {
			hcmd(nil, m);
		}
	} else if(strcmp(m->type, "message/rfc822") == 0){
		pcmd(nil, m->child);
	} else if(plumb(m, cp) >= 0)
		Bprint(&out, "\n!--- using plumber to display message of type %s\n", m->type);
	else
		Bprint(&out, "\n!--- cannot display messages of type %s\n", m->type);

	return m;
}

void
printpartindented(String *s, char *part, char *indent)
{
	int fd;
	char *p;
	String *path;
	Biobuf *b;

	path = extendpath(s, part);
	fd = fsopenfd(mailfs, s_to_c(path), OREAD);
	s_free(path);
	if(fd < 0){
		fprint(2, "!message disappeared\n");
		return;
	}
	b = Bfdopen(fd, OREAD);
	if(b == 0){
		fprint(2, "out of memory\n");
		close(fd);
		return;
	}
	while((p = Brdline(b, '\n')) != nil){
		if(interrupted)
			break;
		p[Blinelen(b)-1] = 0;
		if(Bprint(&out, "%s%s\n", indent, p) < 0)
			break;
	}
	Bprint(&out, "\n");
	Bterm(b);
}

Message*
quotecmd(Cmd *x, Message *m)
{
	Message *nm;
	Ctype *cp;

	USED(x);
	if(m == &top)
		return &top;
	Bprint(&out, "\n");
	if(m->from != nil && *m->from)
		Bprint(&out, "On %s, %s wrote:\n", m->date, m->from);
	cp = findctype(m);
	if(cp->display){
		printpartindented(m->path, "body", "> ");
	} else if(strcmp(m->type, "multipart/alternative") == 0){
		for(nm = m->child; nm != nil; nm = nm->next){
			cp = findctype(nm);
			if(cp->ext != nil && strncmp(cp->ext, "txt", 3) == 0)
				break;
		}
		if(nm == nil)
			for(nm = m->child; nm != nil; nm = nm->next){
				cp = findctype(nm);
				if(cp->display)
					break;
			}
		if(nm != nil)
			quotecmd(nil, nm);
	} else if(strncmp(m->type, "multipart/", 10) == 0){
		nm = m->child;
		if(nm != nil){
			cp = findctype(nm);
			if(cp->display || strncmp(m->type, "multipart/", 10) == 0)
				quotecmd(nil, nm);
		}
	}
	return m;
}

/* really delete messages */
Message*
flushdeleted(Message *cur)
{
	Message *m, **l;
	char buf[1024], *p, *e, *msg;
	int deld, n;
	CFid *fd;
	int i;

	doflush = 0;
	deld = 0;

	snprint(buf, sizeof buf, "%s/ctl", mbname);
	fd = fsopen(mailfs, buf, OWRITE);
	if(fd == nil){
		fprint(2, "!can't delete mail, opening %s: %r\n", buf);
		exitfs(0);
	}
	e = &buf[sizeof(buf)];
	p = seprint(buf, e, "delete");
	n = 0;
	for(l = &top.child; *l != nil;){
		m = *l;
		if(!m->deleted){
			l = &(*l)->next;
			continue;
		}

		/* don't return a pointer to a deleted message */
		if(m == cur)
			cur = m->next;

		deld++;
		msg = strrchr(s_to_c(m->path), '/');
		if(msg == nil)
			msg = s_to_c(m->path);
		else
			msg++;
		if(e-p < 10){
			fswrite(fd, buf, p-buf);
			n = 0;
			p = seprint(buf, e, "delete");
		}
		p = seprint(p, e, " %s", msg);
		n++;

		/* unchain and free */
		*l = m->next;
		if(m->next)
			m->next->prev = m->prev;
		freemessage(m);
	}
	if(n)
		fswrite(fd, buf, p-buf);

	fsclose(fd);

	if(deld)
		Bprint(&out, "!%d message%s deleted\n", deld, plural(deld));

	/* renumber */
	i = 1;
	for(m = top.child; m != nil; m = m->next)
		m->id = natural ? m->fileno : i++;

	/* if we're out of messages, go back to first */
	/* if no first, return the fake first */
	if(cur == nil){
		if(top.child)
			return top.child;
		else
			return &top;
	}
	return cur;
}

Message*
qcmd(Cmd *x, Message *m)
{
	USED(x);
	USED(m);

	flushdeleted(nil);

	if(didopen)
		closemb();
	Bflush(&out);

	exitfs(0);
	return nil;	/* not reached */
}

Message*
ycmd(Cmd *x, Message *m)
{
	USED(x);

	doflush = 1;

	return icmd(nil, m);
}

Message*
xcmd(Cmd *x, Message *m)
{
	USED(x);
	USED(m);

	exitfs(0);
	return nil;	/* not reached */
}

Message*
eqcmd(Cmd *x, Message *m)
{
	USED(x);

	if(m == &top)
		Bprint(&out, "0\n");
	else
		Bprint(&out, "%d\n", m->id);
	return nil;
}

Message*
dcmd(Cmd *x, Message *m)
{
	USED(x);

	if(m == &top){
		Bprint(&out, "!address\n");
		return nil;
	}
	while(m->parent != &top)
		m = m->parent;
	m->deleted = 1;
	return m;
}

Message*
ucmd(Cmd *x, Message *m)
{
	USED(x);

	if(m == &top)
		return nil;
	while(m->parent != &top)
		m = m->parent;
	if(m->deleted < 0)
		Bprint(&out, "!can't undelete, already flushed\n");
	m->deleted = 0;
	return m;
}


Message*
icmd(Cmd *x, Message *m)
{
	int n;
	char buf[1024];
	CFid *fd;

	USED(x);
	snprint(buf, sizeof buf, "%s/ctl", mbname);
	fd = fsopen(mailfs, buf, OWRITE);
	if(fd){
		fswrite(fd, "refresh", 7);
		fsclose(fd);
	}
	n = dir2message(&top, reverse);
	if(n > 0)
		Bprint(&out, "%d new message%s\n", n, plural(n));
	return m;
}

Message*
helpcmd(Cmd *x, Message *m)
{
	int i;

	USED(x);
	Bprint(&out, "Commands are of the form [<range>] <command> [args]\n");
	Bprint(&out, "<range> := <addr> | <addr>','<addr>| 'g'<search>\n");
	Bprint(&out, "<addr> := '.' | '$' | '^' | <number> | <search> | <addr>'+'<addr> | <addr>'-'<addr>\n");
	Bprint(&out, "<search> := '/'<regexp>'/' | '?'<regexp>'?' | '%%'<regexp>'%%'\n");
	Bprint(&out, "<command> :=\n");
	for(i = 0; cmdtab[i].cmd != nil; i++)
		Bprint(&out, "%s\n", cmdtab[i].help);
	return m;
}

int
tomailer(char **av)
{
	static char *marshal;
	Waitmsg *w;
	int pid, i;

	if(marshal == nil)
		marshal = unsharp("#9/bin/upas/marshal");

	/* start the mailer and get out of the way */
	switch(pid = fork()){
	case -1:
		fprint(2, "can't fork: %r\n");
		return -1;
	case 0:
		Bprint(&out, "!%s", marshal);
		for(i = 1; av[i]; i++){
			if(strchr(av[i], ' ') != nil)
				Bprint(&out, " '%s'", av[i]);
			else
				Bprint(&out, " %s", av[i]);
		}
		Bprint(&out, "\n");
		Bflush(&out);
		av[0] = "marshal";
		chdir(wd);
		exec(marshal, av);
		fprint(2, "couldn't exec %s\n", marshal);
		threadexits(0);
	default:
		w = wait();
		if(w == nil){
			if(interrupted)
				postnote(PNPROC, pid, "die");
			waitpid();
			return -1;
		}
		if(w->msg[0]){
			fprint(2, "mailer failed: %s\n", w->msg);
			free(w);
			return -1;
		}
		free(w);
		Bprint(&out, "!\n");
		break;
	}
	return 0;
}

/* */
/* like tokenize but obey "" quoting */
/* */
int
tokenize822(char *str, char **args, int max)
{
	int na;
	int intok = 0, inquote = 0;

	if(max <= 0)
		return 0;
	for(na=0; ;str++)
		switch(*str) {
		case ' ':
		case '\t':
			if(inquote)
				goto Default;
			/* fall through */
		case '\n':
			*str = 0;
			if(!intok)
				continue;
			intok = 0;
			if(na < max)
				continue;
			/* fall through */
		case 0:
			return na;
		case '"':
			inquote ^= 1;
			/* fall through */
		Default:
		default:
			if(intok)
				continue;
			args[na++] = str;
			intok = 1;
		}
	return 0;	/* can't get here; silence compiler */
}

Message*
rcmd(Cmd *c, Message *m)
{
	char *av[128];
	int i, ai = 1;
	Message *nm;
	char *addr;
	String *path = nil;
	String *rpath;
	String *subject = nil;
	String *from;

	if(m == &top){
		Bprint(&out, "!address\n");
		return nil;
	}

	addr = nil;
	for(nm = m; nm != &top; nm = nm->parent){
 		if(*nm->replyto != 0){
			addr = nm->replyto;
			break;
		}
	}
	if(addr == nil){
		Bprint(&out, "!no reply address\n");
		return nil;
	}

	if(nm == &top){
		print("!noone to reply to\n");
		return nil;
	}

	for(nm = m; nm != &top; nm = nm->parent){
		if(*nm->subject){
			av[ai++] = "-s";
			subject = addrecolon(nm->subject);
			av[ai++] = s_to_c(subject);;
			break;
		}
	}

	av[ai++] = "-R";
	rpath = rooted(s_clone(m->path));
	av[ai++] = s_to_c(rpath);

	if(strchr(c->av[0], 'f') != nil){
		fcmd(c, m);
		av[ai++] = "-F";
	}

	if(strchr(c->av[0], 'R') != nil){
		av[ai++] = "-t";
		av[ai++] = "message/rfc822";
		av[ai++] = "-A";
		path = rooted(extendpath(m->path, "raw"));
		av[ai++] = s_to_c(path);
	}

	for(i = 1; i < c->an && ai < nelem(av)-1; i++)
		av[ai++] = c->av[i];
	from = s_copy(addr);
	ai += tokenize822(s_to_c(from), &av[ai], nelem(av) - ai);
	av[ai] = 0;
	if(tomailer(av) < 0)
		m = nil;
	s_free(path);
	s_free(rpath);
	s_free(subject);
	s_free(from);
	return m;
}

Message*
mcmd(Cmd *c, Message *m)
{
	char **av;
	int i, ai;
	String *path;

	if(m == &top){
		Bprint(&out, "!address\n");
		return nil;
	}

	if(c->an < 2){
		fprint(2, "!usage: M list-of addresses\n");
		return nil;
	}

	ai = 1;
	av = malloc(sizeof(char*)*(c->an + 8));

	av[ai++] = "-t";
	if(m->parent == &top)
		av[ai++] = "message/rfc822";
	else
		av[ai++] = "mime";

	av[ai++] = "-A";
	path = rooted(extendpath(m->path, "raw"));
	av[ai++] = s_to_c(path);

	if(strchr(c->av[0], 'M') == nil)
		av[ai++] = "-n";

	for(i = 1; i < c->an; i++)
		av[ai++] = c->av[i];
	av[ai] = 0;

	if(tomailer(av) < 0)
		m = nil;
	if(path != nil)
		s_free(path);
	free(av);
	return m;
}

Message*
acmd(Cmd *c, Message *m)
{
	char *av[128];
	int i, ai;
	String *from, *to, *cc, *path = nil, *subject = nil;

	if(m == &top){
		Bprint(&out, "!address\n");
		return nil;
	}

	ai = 1;
	if(*m->subject){
		av[ai++] = "-s";
		subject = addrecolon(m->subject);
		av[ai++] = s_to_c(subject);
	}

	if(strchr(c->av[0], 'A') != nil){
		av[ai++] = "-t";
		av[ai++] = "message/rfc822";
		av[ai++] = "-A";
		path = rooted(extendpath(m->path, "raw"));
		av[ai++] = s_to_c(path);
	}

	for(i = 1; i < c->an && ai < nelem(av)-1; i++)
		av[ai++] = c->av[i];
	from = s_copy(m->from);
	ai += tokenize822(s_to_c(from), &av[ai], nelem(av) - ai);
	to = s_copy(m->to);
	ai += tokenize822(s_to_c(to), &av[ai], nelem(av) - ai);
	cc = s_copy(m->cc);
	ai += tokenize822(s_to_c(cc), &av[ai], nelem(av) - ai);
	av[ai] = 0;
	if(tomailer(av) < 0)
		return nil;
	s_free(from);
	s_free(to);
	s_free(cc);
	s_free(subject);
	s_free(path);
	return m;
}

String *
relpath(char *path, String *to)
{
	if (*path=='/' || strncmp(path, "./", 2) == 0
			      || strncmp(path, "../", 3) == 0) {
		to = s_append(to, path);
	} else if(mbpath) {
		to = s_append(to, s_to_c(mbpath));
		to->ptr = strrchr(to->base, '/')+1;
		s_append(to, path);
	}
	return to;
}

int
appendtofile(Message *m, char *part, char *base, int mbox)
{
	String *file, *h;
	int in, out, rv;

	file = extendpath(m->path, part);
	in = open(s_to_c(file), OREAD);
	if(in < 0){
		fprint(2, "!message disappeared\n");
		return -1;
	}

	s_reset(file);

	relpath(base, file);
	if(sysisdir(s_to_c(file))){
		s_append(file, "/");
		if(m->filename && strchr(m->filename, '/') == nil)
			s_append(file, m->filename);
		else {
			s_append(file, "att.XXXXXXXXXXX");
			mktemp(s_to_c(file));
		}
	}
	if(mbox)
		out = open(s_to_c(file), OWRITE);
	else
		out = open(s_to_c(file), OWRITE|OTRUNC);
	if(out < 0){
		out = create(s_to_c(file), OWRITE, 0666);
		if(out < 0){
			fprint(2, "!can't open %s: %r\n", s_to_c(file));
			close(in);
			s_free(file);
			return -1;
		}
	}
	if(mbox)
		seek(out, 0, 2);

	/* put on a 'From ' line */
	if(mbox){
		while(m->parent != &top)
			m = m->parent;
		h = file2string(m->path, "unixheader");
		fprint(out, "%s", s_to_c(h));
		s_free(h);
	}

	/* copy the message escaping what we have to ad adding newlines if we have to */
	if(mbox)
		rv = appendfiletombox(in, out);
	else
		rv = appendfiletofile(in, out);

	close(in);
	close(out);

	if(rv >= 0)
		print("!saved in %s\n", s_to_c(file));
	s_free(file);
	return rv;
}

Message*
scmd(Cmd *c, Message *m)
{
	char buf[256];
	CFid *fd;
	char *file, *msg;

	if(m == &top){
		Bprint(&out, "!address\n");
		return nil;
	}

	switch(c->an){
	case 1:
		file = "stored";
		break;
	case 2:
		file = c->av[1];
		break;
	default:
		fprint(2, "!usage: s filename\n");
		return nil;
	}

	if(file[0] == '/' || (file[0]=='.' && file[1]=='/')){
		if(appendtofile(m, "raw", file, 1) < 0)
			return nil;
	}else{
		snprint(buf, sizeof buf, "%s/ctl", mbname);
		if((fd = fsopen(mailfs, buf, OWRITE)) == nil)
			return nil;
		msg = strrchr(s_to_c(m->path), '/');
		if(msg == nil)
			msg = s_to_c(m->path);
		else
			msg++;
		if(fsprint(fd, "save %s %s", file, msg) < 0){
			fsclose(fd);
			return nil;
		}
		fsclose(fd);
	}
	m->stored = 1;
	return m;
}

Message*
wcmd(Cmd *c, Message *m)
{
	char *file;

	if(m == &top){
		Bprint(&out, "!address\n");
		return nil;
	}

	switch(c->an){
	case 2:
		file = c->av[1];
		break;
	case 1:
		if(*m->filename == 0){
			fprint(2, "!usage: w filename\n");
			return nil;
		}
		file = strrchr(m->filename, '/');
		if(file != nil)
			file++;
		else
			file = m->filename;
		break;
	default:
		fprint(2, "!usage: w filename\n");
		return nil;
	}

	if(appendtofile(m, "body", file, 0) < 0)
		return nil;
	m->stored = 1;
	return m;
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
static String*
foldername(char *rcvr)
{
	char *p;
	int c;
	String *file;
	Dir *d;
	int scarey;

	file = s_new();
	mboxpath("f", user, file, 0);
	d = dirstat(s_to_c(file));

	/* if $mail/f exists, store there, otherwise in $mail */
	s_restart(file);
	if(d && d->qid.type == QTDIR){
		scarey = 0;
		s_append(file, "f/");
	} else {
		scarey = 1;
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
		fprint(2, "!won't overwrite %s\n", s_to_c(file));
		s_free(file);
		return nil;
	}

	return file;
}

Message*
fcmd(Cmd *c, Message *m)
{
	String *folder;

	if(c->an > 1){
		fprint(2, "!usage: f takes no arguments\n");
		return nil;
	}

	if(m == &top){
		Bprint(&out, "!address\n");
		return nil;
	}

	folder = foldername(m->from);
	if(folder == nil)
		return nil;

	if(appendtofile(m, "raw", s_to_c(folder), 1) < 0){
		s_free(folder);
		return nil;
	}
	s_free(folder);

	m->stored = 1;
	return m;
}

void
system(char *cmd, char **av, int in)
{
	int pid;

	switch(pid=fork()){
	case -1:
		return;
	case 0:
		if(strcmp(cmd, "rc") == 0)
			cmd = unsharp("#9/bin/rc");
		if(in >= 0){
			close(0);
			dup(in, 0);
			close(in);
		}
		if(wd[0] != 0)
			chdir(wd);
		exec(cmd, av);
		fprint(2, "!couldn't exec %s\n", cmd);
		threadexits(0);
	default:
		if(in >= 0)
			close(in);
		while(waitpid() < 0){
			if(!interrupted)
				break;
			postnote(PNPROC, pid, "die");
			continue;
		}
		break;
	}
}

Message*
bangcmd(Cmd *c, Message *m)
{
	char cmd[4*1024];
	char *p, *e;
	char *av[4];
	int i;

	cmd[0] = 0;
	p = cmd;
	e = cmd+sizeof(cmd);
	for(i = 1; i < c->an; i++)
		p = seprint(p, e, "%s ", c->av[i]);
	av[0] = "rc";
	av[1] = "-c";
	av[2] = cmd;
	av[3] = 0;
	system("rc", av, -1);
	Bprint(&out, "!\n");
	return m;
}

Message*
xpipecmd(Cmd *c, Message *m, char *part)
{
	char cmd[128];
	char *p, *e;
	char *av[4];
	String *path;
	int i, fd;

	if(c->an < 2){
		Bprint(&out, "!usage: | cmd\n");
		return nil;
	}

	if(m == &top){
		Bprint(&out, "!address\n");
		return nil;
	}

	path = extendpath(m->path, part);
	fd = fsopenfd(mailfs, s_to_c(path), OREAD);
	s_free(path);

	if(fd < 0){	/* compatibility with older upas/fs */
		path = extendpath(m->path, "raw");
		fd = fsopenfd(mailfs, s_to_c(path), OREAD);
		s_free(path);
	}
	if(fd < 0){
		fprint(2, "!message disappeared\n");
		return nil;
	}

	p = cmd;
	e = cmd+sizeof(cmd);
	cmd[0] = 0;
	for(i = 1; i < c->an; i++)
		p = seprint(p, e, "%s ", c->av[i]);
	av[0] = "rc";
	av[1] = "-c";
	av[2] = cmd;
	av[3] = 0;
	system("rc", av, fd);	/* system closes fd */
	Bprint(&out, "!\n");
	return m;
}

Message*
pipecmd(Cmd *c, Message *m)
{
	return xpipecmd(c, m, "body");
}

Message*
rpipecmd(Cmd *c, Message *m)
{
	return xpipecmd(c, m, "rawunix");
}

void
closemb(void)
{
	CFid *fd;

	fd = fsopen(mailfs, "ctl", OWRITE);
	if(fd == nil)
		sysfatal("can't open ctl: %r");

	/* close current mailbox */
	if(*mbname && strcmp(mbname, "mbox") != 0)
		fsprint(fd, "close %s", mbname);

	fsclose(fd);
}

int
switchmb(char *file, char *singleton)
{
	char *p;
	int n, fd;
	String *path;
	char buf[256];

	/* if the user didn't say anything and there */
	/* is an mbox mounted already, use that one */
	/* so that the upas/fs -fdefault default is honored. */
	if(0 && (file || (singleton && fsaccess(mailfs, singleton, 0) < 0))){
	/* XXX all wrong */
		fprint(2, "file=%s singleton=%s\n", file, singleton);
		if(file == nil)
			file = "mbox";

		/* close current mailbox */
		closemb();
		didopen = 1;

		fd = open("/mail/fs/ctl", ORDWR);
		if(fd < 0)
			sysfatal("can't open /mail/fs/ctl: %r");

		path = s_new();

		/* get an absolute path to the mail box */
		if(strncmp(file, "./", 2) == 0){
			/* resolve path here since upas/fs doesn't know */
			/* our working directory */
			if(getwd(buf, sizeof(buf)-strlen(file)) == nil){
				fprint(2, "!can't get working directory: %s\n", buf);
				return -1;
			}
			s_append(path, buf);
			s_append(path, file+1);
		} else {
			mboxpath(file, user, path, 0);
		}

		/* make up a handle to use when talking to fs */
		p = strrchr(file, '/');
		if(p == nil){
			/* if its in the mailbox directory, just use the name */
			strncpy(mbname, file, sizeof(mbname));
			mbname[sizeof(mbname)-1] = 0;
		} else {
			/* make up a mailbox name */
			p = strrchr(s_to_c(path), '/');
			p++;
			if(*p == 0){
				fprint(2, "!bad mbox name");
				return -1;
			}
			strncpy(mbname, p, sizeof(mbname));
			mbname[sizeof(mbname)-1] = 0;
			n = strlen(mbname);
			if(n > Elemlen-12)
				n = Elemlen-12;
			sprint(mbname+n, "%ld", time(0));
		}

		if(fprint(fd, "open %s %s", s_to_c(path), mbname) < 0){
			fprint(2, "!can't 'open %s %s': %r\n", file, mbname);
			s_free(path);
			return -1;
		}
		close(fd);
	}else
	if (singleton && fsaccess(mailfs, singleton, 0)==0){
		if ((p = strchr(singleton, '/')) == nil){
			fprint(2, "!bad mbox name");
			return -1;
		}
		n = p-singleton;
		strncpy(mbname, singleton, n);
		mbname[n+1] = 0;
		path = s_reset(nil);
		mboxpath(mbname, user, path, 0);
	}else{
		if(file)
			strecpy(mbname, mbname+sizeof mbname, file);
		else
			strcpy(mbname, "mbox");
		path = s_reset(nil);
		mboxpath(mbname, user, path, 0);
	}

	snprint(root, sizeof root, "%s", mbname);
	rootlen = strlen(root);

	if(mbpath != nil)
		s_free(mbpath);
	mbpath = path;
	return 0;
}

/* like tokenize but for into lines */
int
lineize(char *s, char **f, int n)
{
	int i;

	for(i = 0; *s && i < n; i++){
		f[i] = s;
		s = strchr(s, '\n');
		if(s == nil)
			break;
		*s++ = 0;
	}
	return i;
}



String*
rooted(String *s)
{
	static char buf[256];

	if(strcmp(root, ".") != 0)
		return s;
	snprint(buf, sizeof(buf), "/mail/fs/%s/%s", mbname, s_to_c(s));
	s_free(s);
	return s_copy(buf);
}

int
plumb(Message *m, Ctype *cp)
{
	String *s;
	Plumbmsg *pm;
	static int fd = -2;

	if(cp->plumbdest == nil)
		return -1;

	if(fd < -1)
		fd = plumbopen("send", OWRITE);
	if(fd < 0)
		return -1;

	pm = mallocz(sizeof(Plumbmsg), 1);
	pm->src = strdup("mail");
	if(*cp->plumbdest)
		pm->dst = strdup(cp->plumbdest);
	pm->wdir = nil;
	pm->type = strdup("text");
	pm->ndata = -1;
	s = rooted(extendpath(m->path, "body"));
	if(cp->ext != nil){
		s_append(s, ".");
		s_append(s, cp->ext);
	}
	pm->data = strdup(s_to_c(s));
	s_free(s);
	plumbsend(fd, pm);
	plumbfree(pm);
	return 0;
}

void
regerror(char *x)
{
	USED(x);
}

String*
addrecolon(char *s)
{
	String *str;

	if(cistrncmp(s, "re:", 3) != 0){
		str = s_copy("Re: ");
		s_append(str, s);
	} else
		str = s_copy(s);
	return str;
}

void
exitfs(char *rv)
{
	threadexitsall(rv);
}
