#include "common.h"
#include "smtp.h"
#include <ctype.h>
#include <mp.h>
#include <libsec.h>
#include <auth.h>
#include <ndb.h>
#include <thread.h>

static	char*	connect(char*);
static	char*	dotls(char*);
static	char*	doauth(char*);
char*	hello(char*, int);
char*	mailfrom(char*);
char*	rcptto(char*);
char*	data(String*, Biobuf*);
void	quit(char*);
int	getreply(void);
void	addhostdom(String*, char*);
String*	bangtoat(char*);
String*	convertheader(String*);
int	printheader(void);
char*	domainify(char*, char*);
void	putcrnl(char*, int);
char*	getcrnl(String*);
int	printdate(Node*);
char	*rewritezone(char *);
int	dBprint(char*, ...);
int	dBputc(int);
String*	fixrouteaddr(String*, Node*, Node*);
char* expand_addr(char* a);
int	ping;
int	insecure;

#define Retry	"Retry, Temporary Failure"
#define Giveup	"Permanent Failure"

int	debug;		/* true if we're debugging */
String	*reply;		/* last reply */
String	*toline;
int	alarmscale;
int	last = 'n';	/* last character sent by putcrnl() */
int	filter;
int	trysecure;	/* Try to use TLS if the other side supports it */
int	tryauth;	/* Try to authenticate, if supported */
int	quitting;	/* when error occurs in quit */
char	*quitrv;	/* deferred return value when in quit */
char	ddomain[1024];	/* domain name of destination machine */
char	*gdomain;	/* domain name of gateway */
char	*uneaten;	/* first character after rfc822 headers */
char	*farend;	/* system we are trying to send to */
char	*user;		/* user we are authenticating as, if authenticating */
char	hostdomain[256];
Biobuf	bin;
Biobuf	bout;
Biobuf	berr;
Biobuf	bfile;

void
usage(void)
{
	fprint(2, "usage: smtp [-adips] [-uuser] [-hhost] [.domain] net!host[!service] sender rcpt-list\n");
	threadexitsall(Giveup); 
}

int
timeout(void *x, char *msg)
{
	USED(x);
	syslog(0, "smtp.fail", "interrupt: %s: %s", farend,  msg);
	if(strstr(msg, "alarm")){
		fprint(2, "smtp timeout: connection to %s timed out\n", farend);
		if(quitting)
			threadexitsall(quitrv);
		threadexitsall(Retry);
	}
	if(strstr(msg, "closed pipe")){
		fprint(2, "smtp timeout: connection closed to %s\n", farend);
		if(quitting){
			syslog(0, "smtp.fail", "closed pipe to %s", farend);
			threadexitsall(quitrv);
		}
		threadexitsall(Retry);
	}
	return 0;
}

void
removenewline(char *p)
{
	int n = strlen(p)-1;

	if(n < 0)
		return;
	if(p[n] == '\n')
		p[n] = 0;
}

int
exitcode(char *s)
{
	if(strstr(s, "Retry"))	/* known to runq */
		return RetryCode;
	return 1;
}

void
threadmain(int argc, char **argv)
{
	char hellodomain[256];
	char *host, *domain;
	String *from;
	String *fromm;
	String *sender;
	char *addr;
	char *rv, *trv;
	int i, ok, rcvrs;
	char **errs;

	alarmscale = 60*1000;	/* minutes */
	quotefmtinstall();
	errs = malloc(argc*sizeof(char*));
	reply = s_new();
	host = 0;
	ARGBEGIN{
	case 'a':
		tryauth = 1;
		trysecure = 1;
		break;
	case 'f':
		filter = 1;
		break;
	case 'd':
		debug = 1;
		break;
	case 'g':
		gdomain = ARGF();
		break;
	case 'h':
		host = ARGF();
		break;
	case 'i':
		insecure = 1;
		break;
	case 'p':
		alarmscale = 10*1000;	/* tens of seconds */
		ping = 1;
		break;
	case 's':
		trysecure = 1;
		break;
	case 'u':
		user = ARGF();
		break;
	default:
		usage();
		break;
	}ARGEND;

	Binit(&berr, 2, OWRITE);
	Binit(&bfile, 0, OREAD);

	/*
	 *  get domain and add to host name
	 */
	if(*argv && **argv=='.') {
		domain = *argv;
		argv++; argc--;
	} else
		domain = domainname_read();
	if(host == 0)
		host = sysname_read();
	strcpy(hostdomain, domainify(host, domain));
	strcpy(hellodomain, domainify(sysname_read(), domain));

	/*
	 *  get destination address
	 */
	if(*argv == 0)
		usage();
	addr = *argv++; argc--;
	/* expand $smtp if necessary */
	addr = expand_addr(addr);
	farend = addr;

	/*
	 *  get sender's machine.
	 *  get sender in internet style.  domainify if necessary.
	 */
	if(*argv == 0)
		usage();
	sender = unescapespecial(s_copy(*argv++));
	argc--;
	fromm = s_clone(sender);
	rv = strrchr(s_to_c(fromm), '!');
	if(rv)
		*rv = 0;
	else
		*s_to_c(fromm) = 0;
	from = bangtoat(s_to_c(sender));

	/*
	 *  send the mail
	 */
	if(filter){
		Binit(&bout, 1, OWRITE);
		rv = data(from, &bfile);
		if(rv != 0)
			goto error;
		threadexitsall(0);
	}

	/* mxdial uses its own timeout handler */
	if((rv = connect(addr)) != 0)
		threadexitsall(rv);

	/* 10 minutes to get through the initial handshake */
	atnotify(timeout, 1);
	alarm(10*alarmscale);
	if((rv = hello(hellodomain, 0)) != 0)
		goto error;
	alarm(10*alarmscale);
	if((rv = mailfrom(s_to_c(from))) != 0)
		goto error;

	ok = 0;
	rcvrs = 0;
	/* if any rcvrs are ok, we try to send the message */
	for(i = 0; i < argc; i++){
		if((trv = rcptto(argv[i])) != 0){
			/* remember worst error */
			if(rv != nil && strcmp(rv, Giveup) != 0)
				rv = trv;
			errs[rcvrs] = strdup(s_to_c(reply));
			removenewline(errs[rcvrs]);
		} else {
			ok++;
			errs[rcvrs] = 0;
		}
		rcvrs++;
	}

	/* if no ok rcvrs or worst error is retry, give up */
	if(ok == 0 || (rv != nil && strcmp(rv, Retry) == 0))
		goto error;

	if(ping){
		quit(0);
		threadexitsall(0);
	}

	rv = data(from, &bfile);
	if(rv != 0)
		goto error;
	quit(0);
	if(rcvrs == ok)
		threadexitsall(0);

	/*
	 *  here when some but not all rcvrs failed
	 */
	fprint(2, "%s connect to %s:\n", thedate(), addr);
	for(i = 0; i < rcvrs; i++){
		if(errs[i]){
			syslog(0, "smtp.fail", "delivery to %s at %s failed: %s", argv[i], addr, errs[i]);
			fprint(2, "  mail to %s failed: %s", argv[i], errs[i]);
		}
	}
	threadexitsall(Giveup);

	/*
	 *  here when all rcvrs failed
	 */
error:
	removenewline(s_to_c(reply));
	syslog(0, "smtp.fail", "%s to %s failed: %s",
		ping ? "ping" : "delivery",
		addr, s_to_c(reply));
	fprint(2, "%s connect to %s:\n%s\n", thedate(), addr, s_to_c(reply));
	if(!filter)
		quit(rv);
	threadexitsall(rv);
}

/*
 *  connect to the remote host
 */
static char *
connect(char* net)
{
	char buf[256];
	int fd;

	fd = mxdial(net, ddomain, gdomain);

	if(fd < 0){
		rerrstr(buf, sizeof(buf));
		Bprint(&berr, "smtp: %s (%s)\n", buf, net);
		syslog(0, "smtp.fail", "%s (%s)", buf, net);
		if(strstr(buf, "illegal")
		|| strstr(buf, "unknown")
		|| strstr(buf, "can't translate"))
			return Giveup;
		else
			return Retry;
	}
	Binit(&bin, fd, OREAD);
	fd = dup(fd, -1);
	Binit(&bout, fd, OWRITE);
	return 0;
}

static char smtpthumbs[] =	"/sys/lib/tls/smtp";
static char smtpexclthumbs[] =	"/sys/lib/tls/smtp.exclude";

/*
 *  exchange names with remote host, attempt to
 *  enable encryption and optionally authenticate.
 *  not fatal if we can't.
 */
static char *
dotls(char *me)
{
	TLSconn *c;
	Thumbprint *goodcerts;
	char *h;
	int fd;
	uchar hash[SHA1dlen];

	return Giveup;

	c = mallocz(sizeof(*c), 1);	/* Note: not freed on success */
	if (c == nil)
		return Giveup;

	dBprint("STARTTLS\r\n");
	if (getreply() != 2)
		return Giveup;

	fd = tlsClient(Bfildes(&bout), c);
	if (fd < 0) {
		syslog(0, "smtp", "tlsClient to %q: %r", ddomain);
		return Giveup;
	}
	goodcerts = initThumbprints(smtpthumbs, smtpexclthumbs);
	if (goodcerts == nil) {
		free(c);
		close(fd);
		syslog(0, "smtp", "bad thumbprints in %s", smtpthumbs);
		return Giveup;		/* how to recover? TLS is started */
	}

	/* compute sha1 hash of remote's certificate, see if we know it */
	sha1(c->cert, c->certlen, hash, nil);
	if (!okThumbprint(hash, goodcerts)) {
		/* TODO? if not excluded, add hash to thumb list */
		free(c);
		close(fd);
		h = malloc(2*sizeof hash + 1);
		if (h != nil) {
			enc16(h, 2*sizeof hash + 1, hash, sizeof hash);
			/* print("x509 sha1=%s", h); */
			syslog(0, "smtp",
		"remote cert. has bad thumbprint: x509 sha1=%s server=%q",
				h, ddomain);
			free(h);
		}
		return Giveup;		/* how to recover? TLS is started */
	}
	freeThumbprints(goodcerts);
	Bterm(&bin);
	Bterm(&bout);

	/*
	 * set up bin & bout to use the TLS fd, i/o upon which generates
	 * i/o on the original, underlying fd.
	 */
	Binit(&bin, fd, OREAD);
	fd = dup(fd, -1);
	Binit(&bout, fd, OWRITE);

	syslog(0, "smtp", "started TLS to %q", ddomain);
	return(hello(me, 1));
}

static char *
doauth(char *methods)
{
	char *buf, *base64;
	int n;
	DS ds;
	UserPasswd *p;

	dial_string_parse(ddomain, &ds);

	if(user != nil)
		p = auth_getuserpasswd(nil,
	  	  "proto=pass service=smtp role=client server=%q user=%q", ds.host, user);
	else
		p = auth_getuserpasswd(nil,
	  	  "proto=pass service=smtp role=client server=%q", ds.host);
	if (p == nil)
		return Giveup;

	if (strstr(methods, "LOGIN")){
		dBprint("AUTH LOGIN\r\n");
		if (getreply() != 3)
			return Retry;

		n = strlen(p->user);
		base64 = malloc(2*n);
		if (base64 == nil)
			return Retry;	/* Out of memory */
		enc64(base64, 2*n, (uchar *)p->user, n);
		dBprint("%s\r\n", base64);
		if (getreply() != 3)
			return Retry;

		n = strlen(p->passwd);
		base64 = malloc(2*n);
		if (base64 == nil)
			return Retry;	/* Out of memory */
		enc64(base64, 2*n, (uchar *)p->passwd, n);
		dBprint("%s\r\n", base64);
		if (getreply() != 2)
			return Retry;

		free(base64);
	}
	else
	if (strstr(methods, "PLAIN")){
		n = strlen(p->user) + strlen(p->passwd) + 3;
		buf = malloc(n);
		base64 = malloc(2 * n);
		if (buf == nil || base64 == nil) {
			free(buf);
			return Retry;	/* Out of memory */
		}
		snprint(buf, n, "%c%s%c%s", 0, p->user, 0, p->passwd);
		enc64(base64, 2 * n, (uchar *)buf, n - 1);
		free(buf);
		dBprint("AUTH PLAIN %s\r\n", base64);
		free(base64);
		if (getreply() != 2)
			return Retry;
	}
	else
		return "No supported AUTH method";
	return(0);
}

char *
hello(char *me, int encrypted)
{
	int ehlo;
	String *r;
	char *ret, *s, *t;

	if (!encrypted)
		switch(getreply()){
		case 2:
			break;
		case 5:
			return Giveup;
		default:
			return Retry;
		}

	ehlo = 1;
	encrypted = 1;
  Again:
	if(ehlo)
		dBprint("EHLO %s\r\n", me);
	else
		dBprint("HELO %s\r\n", me);
	switch (getreply()) {
	case 2:
		break;
	case 5:
		if(ehlo){
			ehlo = 0;
			goto Again;
		}
		return Giveup;
	default:
		return Retry;
	}
	r = s_clone(reply);
	if(r == nil)
		return Retry;	/* Out of memory or couldn't get string */

	/* Invariant: every line has a newline, a result of getcrlf() */
	for(s = s_to_c(r); (t = strchr(s, '\n')) != nil; s = t + 1){
		*t = '\0';
		for (t = s; *t != '\0'; t++)
			*t = toupper(*t);
		if(!encrypted && trysecure &&
		    (strcmp(s, "250-STARTTLS") == 0 ||
		     strcmp(s, "250 STARTTLS") == 0)){
			s_free(r);
			return(dotls(me));
		}
		if(tryauth && (encrypted || insecure) &&
		    (strncmp(s, "250 AUTH", strlen("250 AUTH")) == 0 ||
		     strncmp(s, "250-AUTH", strlen("250 AUTH")) == 0)){
			ret = doauth(s + strlen("250 AUTH "));
			s_free(r);
			return ret;
		}
	}
	s_free(r);
	return 0;
}

/*
 *  report sender to remote
 */
char *
mailfrom(char *from)
{
	if(!returnable(from))
		dBprint("MAIL FROM:<>\r\n");
	else
	if(strchr(from, '@'))
		dBprint("MAIL FROM:<%s>\r\n", from);
	else
		dBprint("MAIL FROM:<%s@%s>\r\n", from, hostdomain);
	switch(getreply()){
	case 2:
		break;
	case 5:
		return Giveup;
	default:
		return Retry;
	}
	return 0;
}

/*
 *  report a recipient to remote
 */
char *
rcptto(char *to)
{
	String *s;

	s = unescapespecial(bangtoat(to));
	if(toline == 0)
		toline = s_new();
	else
		s_append(toline, ", ");
	s_append(toline, s_to_c(s));
	if(strchr(s_to_c(s), '@'))
		dBprint("RCPT TO:<%s>\r\n", s_to_c(s));
	else {
		s_append(toline, "@");
		s_append(toline, ddomain);
		dBprint("RCPT TO:<%s@%s>\r\n", s_to_c(s), ddomain);
	}
	alarm(10*alarmscale);
	switch(getreply()){
	case 2:
		break;
	case 5:
		return Giveup;
	default:
		return Retry;
	}
	return 0;
}

static char hex[] = "0123456789abcdef";

/*
 *  send the damn thing
 */
char *
data(String *from, Biobuf *b)
{
	char *buf, *cp;
	int i, n, nbytes, bufsize, eof, r;
	String *fromline;
	char errmsg[Errlen];
	char id[40];

	/*
	 *  input the header.
	 */

	buf = malloc(1);
	if(buf == 0){
		s_append(s_restart(reply), "out of memory");
		return Retry;
	}
	n = 0;
	eof = 0;
	for(;;){
		cp = Brdline(b, '\n');
		if(cp == nil){
			eof = 1;
			break;
		}
		nbytes = Blinelen(b);
		buf = realloc(buf, n+nbytes+1);
		if(buf == 0){
			s_append(s_restart(reply), "out of memory");
			return Retry;
		}
		strncpy(buf+n, cp, nbytes);
		n += nbytes;
		if(nbytes == 1)		/* end of header */
			break;
	}
	buf[n] = 0;
	bufsize = n;

	/*
	 *  parse the header, turn all addresses into @ format
	 */
	yyinit(buf, n);
	yyparse();

	/*
	 *  print message observing '.' escapes and using \r\n for \n
	 */
	alarm(20*alarmscale);
	if(!filter){
		dBprint("DATA\r\n");
		switch(getreply()){
		case 3:
			break;
		case 5:
			free(buf);
			return Giveup;
		default:
			free(buf);
			return Retry;
		}
	}
	/*
	 *  send header.  add a message-id, a sender, and a date if there
	 *  isn't one
	 */
	nbytes = 0;
	fromline = convertheader(from);
	uneaten = buf;

	srand(truerand());
	if(messageid == 0){
		for(i=0; i<16; i++){
			r = rand()&0xFF;
			id[2*i] = hex[r&0xF];
			id[2*i+1] = hex[(r>>4)&0xF];
		}
		id[2*i] = '\0';
		nbytes += Bprint(&bout, "Message-ID: <%s@%s>\r\n", id, hostdomain);
		if(debug)
			Bprint(&berr, "Message-ID: <%s@%s>\r\n", id, hostdomain);
	}	

	if(originator==0){
		nbytes += Bprint(&bout, "From: %s\r\n", s_to_c(fromline));
		if(debug)
			Bprint(&berr, "From: %s\r\n", s_to_c(fromline));
	}
	s_free(fromline);

	if(destination == 0 && toline)
		if(*s_to_c(toline) == '@'){	/* route addr */
			nbytes += Bprint(&bout, "To: <%s>\r\n", s_to_c(toline));
			if(debug)
				Bprint(&berr, "To: <%s>\r\n", s_to_c(toline));
		} else {
			nbytes += Bprint(&bout, "To: %s\r\n", s_to_c(toline));
			if(debug)
				Bprint(&berr, "To: %s\r\n", s_to_c(toline));
		}

	if(date==0 && udate)
		nbytes += printdate(udate);
	if (usys)
		uneaten = usys->end + 1;
	nbytes += printheader();
	if (*uneaten != '\n')
		putcrnl("\n", 1);

	/*
	 *  send body
	 */
		
	putcrnl(uneaten, buf+n - uneaten);
	nbytes += buf+n - uneaten;
	if(eof == 0){
		for(;;){
			n = Bread(b, buf, bufsize);
			if(n < 0){
				rerrstr(errmsg, sizeof(errmsg));
				s_append(s_restart(reply), errmsg);
				free(buf);
				return Retry;
			}
			if(n == 0)
				break;
			alarm(10*alarmscale);
			putcrnl(buf, n);
			nbytes += n;
		}
	}
	free(buf);
	if(!filter){
		if(last != '\n')
			dBprint("\r\n.\r\n");
		else
			dBprint(".\r\n");
		alarm(10*alarmscale);
		switch(getreply()){
		case 2:
			break;
		case 5:
			return Giveup;
		default:
			return Retry;
		}
		syslog(0, "smtp", "%s sent %d bytes to %s", s_to_c(from),
				nbytes, s_to_c(toline));/**/
	}
	return 0;
}

/*
 *  we're leaving
 */
void
quit(char *rv)
{
		/* 60 minutes to quit */
	quitting = 1;
	quitrv = rv;
	alarm(60*alarmscale);
	dBprint("QUIT\r\n");
	getreply();
	Bterm(&bout);
	Bterm(&bfile);
}

/*
 *  read a reply into a string, return the reply code
 */
int
getreply(void)
{
	char *line;
	int rv;

	reply = s_reset(reply);
	for(;;){
		line = getcrnl(reply);
		if(debug)
			Bflush(&berr);
		if(line == 0)
			return -1;
		if(!isdigit(line[0]) || !isdigit(line[1]) || !isdigit(line[2]))
			return -1;
		if(line[3] != '-')
			break;
	}
	rv = atoi(line)/100;
	return rv;
}
void
addhostdom(String *buf, char *host)
{
	s_append(buf, "@");
	s_append(buf, host);
}

/*
 *	Convert from `bang' to `source routing' format.
 *
 *	   a.x.y!b.p.o!c!d ->	@a.x.y:c!d@b.p.o
 */
String *
bangtoat(char *addr)
{
	String *buf;
	register int i;
	int j, d;
	char *field[128];

	/* parse the '!' format address */
	buf = s_new();
	for(i = 0; addr; i++){
		field[i] = addr;
		addr = strchr(addr, '!');
		if(addr)
			*addr++ = 0;
	}
	if (i==1) {
		s_append(buf, field[0]);
		return buf;
	}

	/*
	 *  count leading domain fields (non-domains don't count)
	 */
	for(d = 0; d<i-1; d++)
		if(strchr(field[d], '.')==0)
			break;
	/*
	 *  if there are more than 1 leading domain elements,
	 *  put them in as source routing
	 */
	if(d > 1){
		addhostdom(buf, field[0]);
		for(j=1; j<d-1; j++){
			s_append(buf, ",");
			s_append(buf, "@");
			s_append(buf, field[j]);
		}
		s_append(buf, ":");
	}

	/*
	 *  throw in the non-domain elements separated by '!'s
	 */
	s_append(buf, field[d]);
	for(j=d+1; j<=i-1; j++) {
		s_append(buf, "!");
		s_append(buf, field[j]);
	}
	if(d)
		addhostdom(buf, field[d-1]);
	return buf;
}

/*
 *  convert header addresses to @ format.
 *  if the address is a source address, and a domain is specified,
 *  make sure it falls in the domain.
 */
String*
convertheader(String *from)
{
	Field *f;
	Node *p, *lastp;
	String *a;

	if(!returnable(s_to_c(from))){
		from = s_new();
		s_append(from, "Postmaster");
		addhostdom(from, hostdomain);
	} else
	if(strchr(s_to_c(from), '@') == 0){
		a = username(from);
		if(a) {
			s_append(a, " <");
			s_append(a, s_to_c(from));
			addhostdom(a, hostdomain);
			s_append(a, ">");
			from = a;
		} else {
			from = s_copy(s_to_c(from));
			addhostdom(from, hostdomain);
		}
	} else
		from = s_copy(s_to_c(from));
	for(f = firstfield; f; f = f->next){
		lastp = 0;
		for(p = f->node; p; lastp = p, p = p->next){
			if(!p->addr)
				continue;
			a = bangtoat(s_to_c(p->s));
			s_free(p->s);
			if(strchr(s_to_c(a), '@') == 0)
				addhostdom(a, hostdomain);
			else if(*s_to_c(a) == '@')
				a = fixrouteaddr(a, p->next, lastp);
			p->s = a;
		}
	}
	return from;
}
/*
 *	ensure route addr has brackets around it
 */
String*
fixrouteaddr(String *raddr, Node *next, Node *last)
{
	String *a;

	if(last && last->c == '<' && next && next->c == '>')
		return raddr;			/* properly formed already */

	a = s_new();
	s_append(a, "<");
	s_append(a, s_to_c(raddr));
	s_append(a, ">");
	s_free(raddr);
	return a;
}

/*
 *  print out the parsed header
 */
int
printheader(void)
{
	int n, len;
	Field *f;
	Node *p;
	char *cp;
	char c[1];

	n = 0;
	for(f = firstfield; f; f = f->next){
		for(p = f->node; p; p = p->next){
			if(p->s)
				n += dBprint("%s", s_to_c(p->s));
			else {
				c[0] = p->c;
				putcrnl(c, 1);
				n++;
			}
			if(p->white){
				cp = s_to_c(p->white);
				len = strlen(cp);
				putcrnl(cp, len);
				n += len;
			}
			uneaten = p->end;
		}
		putcrnl("\n", 1);
		n++;
		uneaten++;		/* skip newline */
	}
	return n;
}

/*
 *  add a domain onto an name, return the new name
 */
char *
domainify(char *name, char *domain)
{
	static String *s;
	char *p;

	if(domain==0 || strchr(name, '.')!=0)
		return name;

	s = s_reset(s);
	s_append(s, name);
	p = strchr(domain, '.');
	if(p == 0){
		s_append(s, ".");
		p = domain;
	}
	s_append(s, p);
	return s_to_c(s);
}

/*
 *  print message observing '.' escapes and using \r\n for \n
 */
void
putcrnl(char *cp, int n)
{
	int c;

	for(; n; n--, cp++){
		c = *cp;
		if(c == '\n')
			dBputc('\r');
		else if(c == '.' && last=='\n')
			dBputc('.');
		dBputc(c);
		last = c;
	}
}

/*
 *  Get a line including a crnl into a string.  Convert crnl into nl.
 */
char *
getcrnl(String *s)
{
	int c;
	int count;

	count = 0;
	for(;;){
		c = Bgetc(&bin);
		if(debug)
			Bputc(&berr, c);
		switch(c){
		case -1:
			s_append(s, "connection closed unexpectedly by remote system");
			s_terminate(s);
			return 0;
		case '\r':
			c = Bgetc(&bin);
			if(c == '\n'){
		case '\n':
				s_putc(s, c);
				if(debug)
					Bputc(&berr, c);
				count++;
				s_terminate(s);
				return s->ptr - count;
			}
			Bungetc(&bin);
			s_putc(s, '\r');
			if(debug)
				Bputc(&berr, '\r');
			count++;
			break;
		default:
			s_putc(s, c);
			count++;
			break;
		}
	}
	return 0;
}

/*
 *  print out a parsed date
 */
int
printdate(Node *p)
{
	int n, sep = 0;

	n = dBprint("Date: %s,", s_to_c(p->s));
	for(p = p->next; p; p = p->next){
		if(p->s){
			if(sep == 0) {
				dBputc(' ');
				n++;
			}
			if (p->next)
				n += dBprint("%s", s_to_c(p->s));
			else
				n += dBprint("%s", rewritezone(s_to_c(p->s)));
			sep = 0;
		} else {
			dBputc(p->c);
			n++;
			sep = 1;
		}
	}
	n += dBprint("\r\n");
	return n;
}

char *
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

/*
 *  stolen from libc/port/print.c
 */
#define	SIZE	4096
int
dBprint(char *fmt, ...)
{
	char buf[SIZE], *out;
	va_list arg;
	int n;

	va_start(arg, fmt);
	out = vseprint(buf, buf+SIZE, fmt, arg);
	va_end(arg);
	if(debug){
		Bwrite(&berr, buf, (long)(out-buf));
		Bflush(&berr);
	}
	n = Bwrite(&bout, buf, (long)(out-buf));
	Bflush(&bout);
	return n;
}

int
dBputc(int x)
{
	if(debug)
		Bputc(&berr, x);
	return Bputc(&bout, x);
}

char* 
expand_addr(char *addr)
{
	static char buf[256];
	char *p, *q, *name, *sys;
	Ndbtuple *t;
	Ndb *db;
	
	p = strchr(addr, '!');
	if(p){
		q = strchr(p+1, '!');
		name = p+1;
	}else{
		name = addr;
		q = nil;
	}

	if(name[0] != '$')
		return addr;
	name++;
	if(q)
		*q = 0;
		
	sys = sysname();
	db = ndbopen(0);
	t = ndbipinfo(db, "sys", sys, &name, 1);
	if(t == nil){
		ndbclose(db);
		if(q)
			*q = '!';
		return addr;
	}
	
	*(name-1) = 0;
	if(q)
		*q = '!';
	else
		q = "";
	snprint(buf, sizeof buf, "%s%s%s", addr, t->val, q);
	return buf;
}
