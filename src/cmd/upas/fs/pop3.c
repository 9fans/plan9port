#include "common.h"
#include <ctype.h>
#include <plumb.h>
#include <libsec.h>
#include <auth.h>
#include <thread.h>
#include "dat.h"

#pragma varargck type "M" uchar*
#pragma varargck argpos pop3cmd 2

typedef struct Pop Pop;
struct Pop {
	char *freep;	/* free this to free the strings below */

	char *host;
	char *user;
	char *port;

	int ppop;
	int refreshtime;
	int debug;
	int pipeline;
	int encrypted;
	int needtls;
	int notls;
	int needssl;

	/* open network connection */
	Biobuf bin;
	Biobuf bout;
	int fd;
	char *lastline;	/* from Brdstr */

	Thumbprint *thumb;
};

char*
geterrstr(void)
{
	static char err[64];

	err[0] = '\0';
	errstr(err, sizeof(err));
	return err;
}

/* */
/* get pop3 response line , without worrying */
/* about multiline responses; the clients */
/* will deal with that. */
/* */
static int
isokay(char *s)
{
	return s!=nil && strncmp(s, "+OK", 3)==0;
}

static void
pop3cmd(Pop *pop, char *fmt, ...)
{
	char buf[128], *p;
	va_list va;

	va_start(va, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, va);
	va_end(va);

	p = buf+strlen(buf);
	if(p > (buf+sizeof(buf)-3))
		sysfatal("pop3 command too long");

	if(pop->debug)
		fprint(2, "<- %s\n", buf);
	strcpy(p, "\r\n");
	Bwrite(&pop->bout, buf, strlen(buf));
	Bflush(&pop->bout);
}

static char*
pop3resp(Pop *pop)
{
	char *s;
	char *p;

	alarm(60*1000);
	if((s = Brdstr(&pop->bin, '\n', 0)) == nil){
		close(pop->fd);
		pop->fd = -1;
		alarm(0);
		return "unexpected eof";
	}
	alarm(0);

	p = s+strlen(s)-1;
	while(p >= s && (*p == '\r' || *p == '\n'))
		*p-- = '\0';

	if(pop->debug)
		fprint(2, "-> %s\n", s);
	free(pop->lastline);
	pop->lastline = s;
	return s;
}

#if 0 /* jpc */
static int
pop3log(char *fmt, ...)
{
	va_list ap;

	va_start(ap,fmt);
	syslog(0, "/sys/log/pop3", fmt, ap);
	va_end(ap);
	return 0;
}
#endif

static char*
pop3pushtls(Pop *pop)
{
	int fd;
	uchar digest[SHA1dlen];
	TLSconn conn;

	memset(&conn, 0, sizeof conn);
	/* conn.trace = pop3log; */
	fd = tlsClient(pop->fd, &conn);
	if(fd < 0)
		return "tls error";
	if(conn.cert==nil || conn.certlen <= 0){
		close(fd);
		return "server did not provide TLS certificate";
	}
	sha1(conn.cert, conn.certlen, digest, nil);
	if(!pop->thumb || !okThumbprint(digest, pop->thumb)){
		fmtinstall('H', encodefmt);
		close(fd);
		free(conn.cert);
		fprint(2, "upas/fs pop3: server certificate %.*H not recognized\n", SHA1dlen, digest);
		return "bad server certificate";
	}
	free(conn.cert);
	close(pop->fd);
	pop->fd = fd;
	pop->encrypted = 1;
	Binit(&pop->bin, pop->fd, OREAD);
	Binit(&pop->bout, pop->fd, OWRITE);
	return nil;
}

/* */
/* get capability list, possibly start tls */
/* */
static char*
pop3capa(Pop *pop)
{
	char *s;
	int hastls;

	pop3cmd(pop, "CAPA");
	if(!isokay(pop3resp(pop)))
		return nil;

	hastls = 0;
	for(;;){
		s = pop3resp(pop);
		if(strcmp(s, ".") == 0 || strcmp(s, "unexpected eof") == 0)
			break;
		if(strcmp(s, "STLS") == 0)
			hastls = 1;
		if(strcmp(s, "PIPELINING") == 0)
			pop->pipeline = 1;
	}

	if(hastls && !pop->notls){
		pop3cmd(pop, "STLS");
		if(!isokay(s = pop3resp(pop)))
			return s;
		if((s = pop3pushtls(pop)) != nil)
			return s;
	}
	return nil;
}

/* */
/* log in using APOP if possible, password if allowed by user */
/* */
static char*
pop3login(Pop *pop)
{
	int n;
	char *s, *p, *q;
	char ubuf[128], user[128];
	char buf[500];
	UserPasswd *up;

	s = pop3resp(pop);
	if(!isokay(s))
		return "error in initial handshake";

	if(pop->user)
		snprint(ubuf, sizeof ubuf, " user=%q", pop->user);
	else
		ubuf[0] = '\0';

	/* look for apop banner */
	if(pop->ppop==0 && (p = strchr(s, '<')) && (q = strchr(p+1, '>'))) {
		*++q = '\0';
		if((n=auth_respond(p, q-p, user, sizeof user, buf, sizeof buf, auth_getkey, "proto=apop role=client server=%q%s",
			pop->host, ubuf)) < 0)
			return "factotum failed";
		if(user[0]=='\0')
			return "factotum did not return a user name";

		if(s = pop3capa(pop))
			return s;

		pop3cmd(pop, "APOP %s %.*s", user, n, buf);
		if(!isokay(s = pop3resp(pop)))
			return s;

		return nil;
	} else {
		if(pop->ppop == 0)
			return "no APOP hdr from server";

		if(s = pop3capa(pop))
			return s;

		if(pop->needtls && !pop->encrypted)
			return "could not negotiate TLS";

		up = auth_getuserpasswd(auth_getkey, "proto=pass role=client service=pop dom=%q%s",
			pop->host, ubuf);
		if(up == nil)
			return "no usable keys found";

		pop3cmd(pop, "USER %s", up->user);
		if(!isokay(s = pop3resp(pop))){
			free(up);
			return s;
		}
		pop3cmd(pop, "PASS %s", up->passwd);
		free(up);
		if(!isokay(s = pop3resp(pop)))
			return s;

		return nil;
	}
}

/* */
/* dial and handshake with pop server */
/* */
static char*
pop3dial(Pop *pop)
{
	char *err;

	if((pop->fd = dial(netmkaddr(pop->host, "net", pop->needssl ? "pop3s" : "pop3"), 0, 0, 0)) < 0)
		return geterrstr();

	if(pop->needssl){
		if((err = pop3pushtls(pop)) != nil)
			return err;
	}else{
		Binit(&pop->bin, pop->fd, OREAD);
		Binit(&pop->bout, pop->fd, OWRITE);
	}

	if(err = pop3login(pop)) {
		close(pop->fd);
		return err;
	}

	return nil;
}

/* */
/* close connection */
/* */
static void
pop3hangup(Pop *pop)
{
	pop3cmd(pop, "QUIT");
	pop3resp(pop);
	close(pop->fd);
}

/* */
/* download a single message */
/* */
static char*
pop3download(Pop *pop, Message *m)
{
	char *s, *f[3], *wp, *ep;
	char sdigest[SHA1dlen*2+1];
	int i, l, sz;

	if(!pop->pipeline)
		pop3cmd(pop, "LIST %d", m->mesgno);
	if(!isokay(s = pop3resp(pop)))
		return s;

	if(tokenize(s, f, 3) != 3)
		return "syntax error in LIST response";

	if(atoi(f[1]) != m->mesgno)
		return "out of sync with pop3 server";

	sz = atoi(f[2])+200;	/* 200 because the plan9 pop3 server lies */
	if(sz == 0)
		return "invalid size in LIST response";

	m->start = wp = emalloc(sz+1);
	ep = wp+sz;

	if(!pop->pipeline)
		pop3cmd(pop, "RETR %d", m->mesgno);
	if(!isokay(s = pop3resp(pop))) {
		m->start = nil;
		free(wp);
		return s;
	}

	s = nil;
	while(wp <= ep) {
		s = pop3resp(pop);
		if(strcmp(s, "unexpected eof") == 0) {
			free(m->start);
			m->start = nil;
			return "unexpected end of conversation";
		}
		if(strcmp(s, ".") == 0)
			break;

		l = strlen(s)+1;
		if(s[0] == '.') {
			s++;
			l--;
		}
		/*
		 * grow by 10%/200bytes - some servers
		 *  lie about message sizes
		 */
		if(wp+l > ep) {
			int pos = wp - m->start;
			sz += ((sz / 10) < 200)? 200: sz/10;
			m->start = erealloc(m->start, sz+1);
			wp = m->start+pos;
			ep = m->start+sz;
		}
		memmove(wp, s, l-1);
		wp[l-1] = '\n';
		wp += l;
	}

	if(s == nil || strcmp(s, ".") != 0)
		return "out of sync with pop3 server";

	m->end = wp;

	/* make sure there's a trailing null */
	/* (helps in body searches) */
	*m->end = 0;
	m->bend = m->rbend = m->end;
	m->header = m->start;

	/* digest message */
	sha1((uchar*)m->start, m->end - m->start, m->digest, nil);
	for(i = 0; i < SHA1dlen; i++)
		sprint(sdigest+2*i, "%2.2ux", m->digest[i]);
	m->sdigest = s_copy(sdigest);

	return nil;
}

/* */
/* check for new messages on pop server */
/* UIDL is not required by RFC 1939, but  */
/* netscape requires it, so almost every server supports it. */
/* we'll use it to make our lives easier. */
/* */
static char*
pop3read(Pop *pop, Mailbox *mb, int doplumb)
{
	char *s, *p, *uidl, *f[2];
	int mesgno, ignore, nnew;
	Message *m, *next, **l;

	/* Some POP servers disallow UIDL if the maildrop is empty. */
	pop3cmd(pop, "STAT");
	if(!isokay(s = pop3resp(pop)))
		return s;

	/* fetch message listing; note messages to grab */
	l = &mb->root->part;
	if(strncmp(s, "+OK 0 ", 6) != 0) {
		pop3cmd(pop, "UIDL");
		if(!isokay(s = pop3resp(pop)))
			return s;

		for(;;){
			p = pop3resp(pop);
			if(strcmp(p, ".") == 0 || strcmp(p, "unexpected eof") == 0)
				break;

			if(tokenize(p, f, 2) != 2)
				continue;

			mesgno = atoi(f[0]);
			uidl = f[1];
			if(strlen(uidl) > 75)	/* RFC 1939 says 70 characters max */
				continue;

			ignore = 0;
			while(*l != nil) {
				if(strcmp((*l)->uidl, uidl) == 0) {
					/* matches mail we already have, note mesgno for deletion */
					(*l)->mesgno = mesgno;
					ignore = 1;
					l = &(*l)->next;
					break;
				} else {
					/* old mail no longer in box mark deleted */
					if(doplumb)
						mailplumb(mb, *l, 1);
					(*l)->inmbox = 0;
					(*l)->deleted = 1;
					l = &(*l)->next;
				}
			}
			if(ignore)
				continue;

			m = newmessage(mb->root);
			m->mallocd = 1;
			m->inmbox = 1;
			m->mesgno = mesgno;
			strcpy(m->uidl, uidl);

			/* chain in; will fill in message later */
			*l = m;
			l = &m->next;
		}
	}

	/* whatever is left has been removed from the mbox, mark as deleted */
	while(*l != nil) {
		if(doplumb)
			mailplumb(mb, *l, 1);
		(*l)->inmbox = 0;
		(*l)->deleted = 1;
		l = &(*l)->next;
	}

	/* download new messages */
	nnew = 0;
	if(pop->pipeline){
		switch(rfork(RFPROC|RFMEM)){
		case -1:
			fprint(2, "rfork: %r\n");
			pop->pipeline = 0;

		default:
			break;

		case 0:
			for(m = mb->root->part; m != nil; m = m->next){
				if(m->start != nil)
					continue;
				Bprint(&pop->bout, "LIST %d\r\nRETR %d\r\n", m->mesgno, m->mesgno);
			}
			Bflush(&pop->bout);
			threadexits(nil);
			/* _exits(nil); jpc */
		}
	}

	for(m = mb->root->part; m != nil; m = next) {
		next = m->next;

		if(m->start != nil)
			continue;

		if(s = pop3download(pop, m)) {
			/* message disappeared? unchain */
			fprint(2, "download %d: %s\n", m->mesgno, s);
			delmessage(mb, m);
			mb->root->subname--;
			continue;
		}
		nnew++;
		parse(m, 0, mb, 1);

		if(doplumb)
			mailplumb(mb, m, 0);
	}
	if(pop->pipeline)
		waitpid();

	if(nnew || mb->vers == 0) {
		mb->vers++;
		henter(PATH(0, Qtop), mb->name,
			(Qid){PATH(mb->id, Qmbox), mb->vers, QTDIR}, nil, mb);
	}

	return nil;
}

/* */
/* delete marked messages */
/* */
static void
pop3purge(Pop *pop, Mailbox *mb)
{
	Message *m, *next;

	if(pop->pipeline){
		switch(rfork(RFPROC|RFMEM)){
		case -1:
			fprint(2, "rfork: %r\n");
			pop->pipeline = 0;

		default:
			break;

		case 0:
			for(m = mb->root->part; m != nil; m = next){
				next = m->next;
				if(m->deleted && m->refs == 0){
					if(m->inmbox)
						Bprint(&pop->bout, "DELE %d\r\n", m->mesgno);
				}
			}
			Bflush(&pop->bout);
			/* _exits(nil); jpc */
			threadexits(nil);
		}
	}
	for(m = mb->root->part; m != nil; m = next) {
		next = m->next;
		if(m->deleted && m->refs == 0) {
			if(m->inmbox) {
				if(!pop->pipeline)
					pop3cmd(pop, "DELE %d", m->mesgno);
				if(isokay(pop3resp(pop)))
					delmessage(mb, m);
			} else
				delmessage(mb, m);
		}
	}
}


/* connect to pop3 server, sync mailbox */
static char*
pop3sync(Mailbox *mb, int doplumb)
{
	char *err;
	Pop *pop;

	pop = mb->aux;

	if(err = pop3dial(pop)) {
		mb->waketime = time(0) + pop->refreshtime;
		return err;
	}

	if((err = pop3read(pop, mb, doplumb)) == nil){
		pop3purge(pop, mb);
		mb->d->atime = mb->d->mtime = time(0);
	}
	pop3hangup(pop);
	mb->waketime = time(0) + pop->refreshtime;
	return err;
}

static char Epop3ctl[] = "bad pop3 control message";

static char*
pop3ctl(Mailbox *mb, int argc, char **argv)
{
	int n;
	Pop *pop;
	char *m, *me;

	pop = mb->aux;
	if(argc < 1)
		return Epop3ctl;

	if(argc==1 && strcmp(argv[0], "debug")==0){
		pop->debug = 1;
		return nil;
	}

	if(argc==1 && strcmp(argv[0], "nodebug")==0){
		pop->debug = 0;
		return nil;
	}

	if(argc==1 && strcmp(argv[0], "thumbprint")==0){
		if(pop->thumb)
			freeThumbprints(pop->thumb);
		/* pop->thumb = initThumbprints("/sys/lib/tls/mail", "/sys/lib/tls/mail.exclude"); jpc */
		m = unsharp("#9/sys/lib/tls/mail");
		me = unsharp("#9/sys/lib/tls/mail.exclude");
		pop->thumb = initThumbprints(m, me);
	}
	if(strcmp(argv[0], "refresh")==0){
		if(argc==1){
			pop->refreshtime = 60;
			return nil;
		}
		if(argc==2){
			n = atoi(argv[1]);
			if(n < 15)
				return Epop3ctl;
			pop->refreshtime = n;
			return nil;
		}
	}

	return Epop3ctl;
}

/* free extra memory associated with mb */
static void
pop3close(Mailbox *mb)
{
	Pop *pop;

	pop = mb->aux;
	free(pop->freep);
	free(pop);
}

/* */
/* open mailboxes of the form /pop/host/user or /apop/host/user */
/* */
char*
pop3mbox(Mailbox *mb, char *path)
{
	char *f[10];
	int nf, apop, ppop, popssl, apopssl, apoptls, popnotls, apopnotls, poptls;
	Pop *pop;
	char *m, *me;

	quotefmtinstall();
	popssl = strncmp(path, "/pops/", 6) == 0;
	apopssl = strncmp(path, "/apops/", 7) == 0;
	poptls = strncmp(path, "/poptls/", 8) == 0;
	popnotls = strncmp(path, "/popnotls/", 10) == 0;
	ppop = popssl || poptls || popnotls || strncmp(path, "/pop/", 5) == 0;
	apoptls = strncmp(path, "/apoptls/", 9) == 0;
	apopnotls = strncmp(path, "/apopnotls/", 11) == 0;
	apop = apopssl || apoptls || apopnotls || strncmp(path, "/apop/", 6) == 0;

	if(!ppop && !apop)
		return Enotme;

	path = strdup(path);
	if(path == nil)
		return "out of memory";

	nf = getfields(path, f, nelem(f), 0, "/");
	if(nf != 3 && nf != 4) {
		free(path);
		return "bad pop3 path syntax /[a]pop[tls|ssl]/system[/user]";
	}

	pop = emalloc(sizeof(*pop));
	pop->freep = path;
	pop->host = f[2];
	if(nf < 4)
		pop->user = nil;
	else
		pop->user = f[3];
	pop->ppop = ppop;
	pop->needssl = popssl || apopssl;
	pop->needtls = poptls || apoptls;
	pop->refreshtime = 60;
	pop->notls = popnotls || apopnotls;
	/* pop->thumb = initThumbprints("/sys/lib/tls/mail", "/sys/lib/tls/mail.exclude"); jpc */
		m = unsharp("#9/sys/lib/tls/mail");
		me = unsharp("#9/sys/lib/tls/mail.exclude");
		pop->thumb = initThumbprints(m, me);

	mb->aux = pop;
	mb->sync = pop3sync;
	mb->close = pop3close;
	mb->ctl = pop3ctl;
	mb->d = emalloc(sizeof(*mb->d));

	return nil;
}
