#include "common.h"
#include "smtpd.h"
#include "smtp.h"
#include <ctype.h>
#include <ip.h>
#include <ndb.h>
#include <mp.h>
#include <libsec.h>
#include <auth.h>
#include <thread.h>
#include "../smtp/rfc822.tab.h"

#define DBGMX 1

char	*me;
char	*him="";
char	*dom;
process	*pp;
String	*mailer;
NetConnInfo *nci;

int	filterstate = ACCEPT;
int	trusted;
int	logged;
int	rejectcount;
int	hardreject;

Biobuf	bin;

int	debug;
int	Dflag;
int	fflag;
int	gflag;
int	rflag;
int	sflag;
int	authenticate;
int	authenticated;
int	passwordinclear;
char	*tlscert;

List	senders;
List	rcvers;

char pipbuf[ERRMAX];
char	*piperror;
int	pipemsg(int*);
String*	startcmd(void);
int	rejectcheck(void);
String*	mailerpath(char*);

static int
catchalarm(void *a, char *msg)
{
	int rv = 1;

	USED(a);

	/* log alarms but continue */
	if(strstr(msg, "alarm")){
		if(senders.first && rcvers.first)
			syslog(0, "smtpd", "note: %s->%s: %s", s_to_c(senders.first->p),
				s_to_c(rcvers.first->p), msg);
		else
			syslog(0, "smtpd", "note: %s", msg);
		rv = 0;
	}

	/* kill the children if there are any */
	if(pp)
		syskillpg(pp->pid);

	return rv;
}

	/* override string error functions to do something reasonable */
void
s_error(char *f, char *status)
{
	char errbuf[Errlen];

	errbuf[0] = 0;
	rerrstr(errbuf, sizeof(errbuf));
	if(f && *f)
		reply("452 out of memory %s: %s\r\n", f, errbuf);
	else
		reply("452 out of memory %s\r\n", errbuf);
	syslog(0, "smtpd", "++Malloc failure %s [%s]", him, nci->rsys);
	threadexitsall(status);
}

void
threadmain(int argc, char **argv)
{
	char *p, buf[1024];
	char *netdir;

	netdir = nil;
	quotefmtinstall();
	ARGBEGIN{
	case 'D':
		Dflag++;
		break;
	case 'd':
		debug++;
		break;
	case 'n':				/* log peer ip address */
		netdir = ARGF();
		break;
	case 'f':				/* disallow relaying */
		fflag = 1;
		break;
	case 'g':
		gflag = 1;
		break;
	case 'h':				/* default domain name */
		dom = ARGF();
		break;
	case 'k':				/* prohibited ip address */
		p = ARGF();
		if (p)
			addbadguy(p);
		break;
	case 'm':				/* set mail command */
		p = ARGF();
		if(p)
			mailer = mailerpath(p);
		break;
	case 'r':
		rflag = 1;			/* verify sender's domain */
		break;
	case 's':				/* save blocked messages */
		sflag = 1;
		break;
	case 'a':
		authenticate = 1;
		break;
	case 'p':
		passwordinclear = 1;
		break;
	case 'c':
		fprint(2, "tls is not available\n");
		threadexitsall("no tls");
		tlscert = ARGF();
		break;
	case 't':
		fprint(2, "%s: the -t option is no longer supported, see -c\n", argv0);
		tlscert = "/sys/lib/ssl/smtpd-cert.pem";
		break;
	default:
		fprint(2, "usage: smtpd [-dfhrs] [-n net] [-c cert]\n");
		threadexitsall("usage");
	}ARGEND;

	nci = getnetconninfo(netdir, 0);
	if(nci == nil)
		sysfatal("can't get remote system's address");

	if(mailer == nil)
		mailer = mailerpath("send");

	if(debug){
		close(2);
		snprint(buf, sizeof(buf), "%s/smtpd.db", UPASLOG);
		if (open(buf, OWRITE) >= 0) {
			seek(2, 0, 2);
			fprint(2, "%d smtpd %s\n", getpid(), thedate());
		} else
			debug = 0;
	}
	getconf();
	Binit(&bin, 0, OREAD);

	chdir(UPASLOG);
	me = sysname_read();
	if(dom == 0 || dom[0] == 0)
		dom = domainname_read();
	if(dom == 0 || dom[0] == 0)
		dom = me;
	sayhi();
	parseinit();
		/* allow 45 minutes to parse the header */
	atnotify(catchalarm, 1);
	alarm(45*60*1000);
	zzparse();
	threadexitsall(0);
}

void
listfree(List *l)
{
	Link *lp;
	Link *next;

	for(lp = l->first; lp; lp = next){
		next = lp->next;
		s_free(lp->p);
		free(lp);
	}
	l->first = l->last = 0;
}

void
listadd(List *l, String *path)
{
	Link *lp;

	lp = (Link *)malloc(sizeof(Link));
	lp->p = path;
	lp->next = 0;

	if(l->last)
		l->last->next = lp;
	else
		l->first = lp;
	l->last = lp;
}

#define	SIZE	4096
int
reply(char *fmt, ...)
{
	char buf[SIZE], *out;
	va_list arg;
	int n;

	va_start(arg, fmt);
	out = vseprint(buf, buf+SIZE, fmt, arg);
	va_end(arg);
	n = (long)(out-buf);
	if(debug) {
		seek(2, 0, 2);
		write(2, buf, n);
	}
	write(1, buf, n);
	return n;
}

void
reset(void)
{
	if(rejectcheck())
		return;
	listfree(&rcvers);
	listfree(&senders);
	if(filterstate != DIALUP){
		logged = 0;
		filterstate = ACCEPT;
	}
	reply("250 ok\r\n");
}

void
sayhi(void)
{
	reply("220 %s SMTP\r\n", dom);
}

void
hello(String *himp, int extended)
{
	char **mynames;

	him = s_to_c(himp);
	syslog(0, "smtpd", "%s from %s as %s", extended ? "ehlo" : "helo", nci->rsys, him);
	if(rejectcheck())
		return;

	if(strchr(him, '.') && nci && !trusted && fflag && strcmp(nci->rsys, nci->lsys) != 0){
		/*
		 * We don't care if he lies about who he is, but it is
		 * not okay to pretend to be us.  Many viruses do this,
		 * just parroting back what we say in the greeting.
		 */
		if(strcmp(him, dom) == 0)
			goto Liarliar;
		for(mynames=sysnames_read(); mynames && *mynames; mynames++){
			if(cistrcmp(*mynames, him) == 0){
			Liarliar:
				syslog(0, "smtpd", "Hung up on %s; claimed to be %s",
					nci->rsys, him);
				reply("554 Liar!\r\n");
				threadexitsall("client pretended to be us");
				return;
			}
		}
	}
	/*
	 * it is never acceptable to claim to be "localhost",
	 * "localhost.localdomain" or "localhost.example.com"; only spammers
	 * do this.  it should be unacceptable to claim any string that doesn't
	 * look like a domain name (e.g., has at least one dot in it), but
	 * Microsoft mail software gets this wrong.
	 */
	if (strcmp(him, "localhost") == 0 ||
	    strcmp(him, "localhost.localdomain") == 0 ||
	    strcmp(him, "localhost.example.com") == 0)
		goto Liarliar;
	if(strchr(him, '.') == 0 && nci != nil && strchr(nci->rsys, '.') != nil)
		him = nci->rsys;

	if(Dflag)
		sleep(15*1000);
	reply("250%c%s you are %s\r\n", extended ? '-' : ' ', dom, him);
	if (extended) {
		if(tlscert != nil)
			reply("250-STARTTLS\r\n");
		if (passwordinclear)
			reply("250 AUTH CRAM-MD5 PLAIN LOGIN\r\n");
		else
			reply("250 AUTH CRAM-MD5\r\n");
	}
}

void
sender(String *path)
{
	String *s;
	static char *lastsender;

	if(rejectcheck())
		return;
	if (authenticate && !authenticated) {
		rejectcount++;
		reply("530 Authentication required\r\n");
		return;
	}
	if(him == 0 || *him == 0){
		rejectcount++;
		reply("503 Start by saying HELO, please.\r\n", s_to_c(path));
		return;
	}

	/* don't add the domain onto black holes or we will loop */
	if(strchr(s_to_c(path), '!') == 0 && strcmp(s_to_c(path), "/dev/null") != 0){
		s = s_new();
		s_append(s, him);
		s_append(s, "!");
		s_append(s, s_to_c(path));
		s_terminate(s);
		s_free(path);
		path = s;
	}
	if(shellchars(s_to_c(path))){
		rejectcount++;
		reply("503 Bad character in sender address %s.\r\n", s_to_c(path));
		return;
	}

	/*
	 * if the last sender address resulted in a rejection because the sending
	 * domain didn't exist and this sender has the same domain, reject immediately.
	 */
	if(lastsender){
		if (strncmp(lastsender, s_to_c(path), strlen(lastsender)) == 0){
			filterstate = REFUSED;
			rejectcount++;
			reply("554 Sender domain must exist: %s\r\n", s_to_c(path));
			return;
		}
		free(lastsender);	/* different sender domain */
		lastsender = 0;
	}

	/*
	 * see if this ip address, domain name, user name or account is blocked
	 */
	filterstate = blocked(path);

	logged = 0;
	listadd(&senders, path);
	reply("250 sender is %s\r\n", s_to_c(path));
}

enum { Rcpt, Domain, Ntoks };

typedef struct Sender Sender;
struct Sender {
	Sender	*next;
	char	*rcpt;
	char	*domain;
};
static Sender *sendlist, *sendlast;
static uchar rsysip[IPaddrlen];

static int
rdsenders(void)
{
	int lnlen, nf, ok = 1;
	char *line, *senderfile;
	char *toks[Ntoks];
	Biobuf *sf;
	Sender *snd;
	static int beenhere = 0;

	if (beenhere)
		return 1;
	beenhere = 1;

	fmtinstall('I', eipfmt);
	parseip(rsysip, nci->rsys);

	/*
	 * we're sticking with a system-wide sender list because
	 * per-user lists would require fully resolving recipient
	 * addresses to determine which users they correspond to
	 * (barring syntactic conventions).
	 */
	senderfile = smprint("%s/senders", UPASLIB);
	sf = Bopen(senderfile, OREAD);
	free(senderfile);
	if (sf == nil)
		return 1;
	while ((line = Brdline(sf, '\n')) != nil) {
		if (line[0] == '#' || line[0] == '\n')
			continue;
		lnlen = Blinelen(sf);
		line[lnlen-1] = '\0';		/* clobber newline */
		nf = tokenize(line, toks, nelem(toks));
		if (nf != nelem(toks))
			continue;		/* malformed line */

		snd = malloc(sizeof *snd);
		if (snd == nil)
			sysfatal("out of memory: %r");
		memset(snd, 0, sizeof *snd);
		snd->next = nil;

		if (sendlast == nil)
			sendlist = snd;
		else
			sendlast->next = snd;
		sendlast = snd;
		snd->rcpt = strdup(toks[Rcpt]);
		snd->domain = strdup(toks[Domain]);
	}
	Bterm(sf);
	return ok;
}

/*
 * read (recipient, sender's DNS) pairs from /mail/lib/senders.
 * Only allow mail to recipient from any of sender's IPs.
 * A recipient not mentioned in the file is always permitted.
 */
static int
senderok(char *rcpt)
{
	int mentioned = 0, matched = 0;
	uchar dnsip[IPaddrlen];
	Sender *snd;
	Ndbtuple *nt, *next, *first;

	rdsenders();
	for (snd = sendlist; snd != nil; snd = snd->next) {
		if (strcmp(rcpt, snd->rcpt) != 0)
			continue;
		/*
		 * see if this domain's ips match nci->rsys.
		 * if not, perhaps a later entry's domain will.
		 */
		mentioned = 1;
		if (parseip(dnsip, snd->domain) != -1 &&
		    memcmp(rsysip, dnsip, IPaddrlen) == 0)
			return 1;
		/*
		 * NB: nt->line links form a circular list(!).
		 * we need to make one complete pass over it to free it all.
		 */
		first = nt = dnsquery(nci->root, snd->domain, "ip");
		if (first == nil)
			continue;
		do {
			if (strcmp(nt->attr, "ip") == 0 &&
			    parseip(dnsip, nt->val) != -1 &&
			    memcmp(rsysip, dnsip, IPaddrlen) == 0)
				matched = 1;
			next = nt->line;
			free(nt);
			nt = next;
		} while (nt != first);
	}
	if (matched)
		return 1;
	else
		return !mentioned;
}

void
receiver(String *path)
{
	char *sender, *rcpt;

	if(rejectcheck())
		return;
	if(him == 0 || *him == 0){
		rejectcount++;
		reply("503 Start by saying HELO, please\r\n");
		return;
	}
	if(senders.last)
		sender = s_to_c(senders.last->p);
	else
		sender = "<unknown>";

	if(!recipok(s_to_c(path))){
		rejectcount++;
		syslog(0, "smtpd", "Disallowed %s (%s/%s) to blocked name %s",
				sender, him, nci->rsys, s_to_c(path));
		reply("550 %s ... user unknown\r\n", s_to_c(path));
		return;
	}
	rcpt = s_to_c(path);
	if (!senderok(rcpt)) {
		rejectcount++;
		syslog(0, "smtpd", "Disallowed sending IP of %s (%s/%s) to %s",
				sender, him, nci->rsys, rcpt);
		reply("550 %s ... sending system not allowed\r\n", rcpt);
		return;
	}

	logged = 0;
		/* forwarding() can modify 'path' on loopback request */
	if(filterstate == ACCEPT && (fflag && !authenticated) && forwarding(path)) {
		syslog(0, "smtpd", "Bad Forward %s (%s/%s) (%s)",
			s_to_c(senders.last->p), him, nci->rsys, s_to_c(path));
		rejectcount++;
		reply("550 we don't relay.  send to your-path@[] for loopback.\r\n");
		return;
	}
	listadd(&rcvers, path);
	reply("250 receiver is %s\r\n", s_to_c(path));
}

void
quit(void)
{
	reply("221 Successful termination\r\n");
	close(0);
	threadexitsall(0);
}

void
turn(void)
{
	if(rejectcheck())
		return;
	reply("502 TURN unimplemented\r\n");
}

void
noop(void)
{
	if(rejectcheck())
		return;
	reply("250 Stop wasting my time!\r\n");
}

void
help(String *cmd)
{
	if(rejectcheck())
		return;
	if(cmd)
		s_free(cmd);
	reply("250 Read rfc821 and stop wasting my time\r\n");
}

void
verify(String *path)
{
	char *p, *q;
	char *av[4];

	if(rejectcheck())
		return;
	if(shellchars(s_to_c(path))){
		reply("503 Bad character in address %s.\r\n", s_to_c(path));
		return;
	}
	av[0] = s_to_c(mailer);
	av[1] = "-x";
	av[2] = s_to_c(path);
	av[3] = 0;

	pp = noshell_proc_start(av, (stream *)0, outstream(),  (stream *)0, 1, 0);
	if (pp == 0) {
		reply("450 We're busy right now, try later\r\n");
		return;
	}

	p = Brdline(pp->std[1]->fp, '\n');
	if(p == 0){
		reply("550 String does not match anything.\r\n");
	} else {
		p[Blinelen(pp->std[1]->fp)-1] = 0;
		if(strchr(p, ':'))
			reply("550 String does not match anything.\r\n");
		else{
			q = strrchr(p, '!');
			if(q)
				p = q+1;
			reply("250 %s <%s@%s>\r\n", s_to_c(path), p, dom);
		}
	}
	proc_wait(pp);
	proc_free(pp);
	pp = 0;
}

/*
 *  get a line that ends in crnl or cr, turn terminating crnl into a nl
 *
 *  return 0 on EOF
 */
static int
getcrnl(String *s, Biobuf *fp)
{
	int c;

	for(;;){
		c = Bgetc(fp);
		if(debug) {
			seek(2, 0, 2);
			fprint(2, "%c", c);
		}
		switch(c){
		case -1:
			goto out;
		case '\r':
			c = Bgetc(fp);
			if(c == '\n'){
				if(debug) {
					seek(2, 0, 2);
					fprint(2, "%c", c);
				}
				s_putc(s, '\n');
				goto out;
			}
			Bungetc(fp);
			s_putc(s, '\r');
			break;
		case '\n':
			s_putc(s, c);
			goto out;
		default:
			s_putc(s, c);
			break;
		}
	}
out:
	s_terminate(s);
	return s_len(s);
}

void
logcall(int nbytes)
{
	Link *l;
	String *to, *from;

	to = s_new();
	from = s_new();
	for(l = senders.first; l; l = l->next){
		if(l != senders.first)
			s_append(from, ", ");
		s_append(from, s_to_c(l->p));
	}
	for(l = rcvers.first; l; l = l->next){
		if(l != rcvers.first)
			s_append(to, ", ");
		s_append(to, s_to_c(l->p));
	}
	syslog(0, "smtpd", "[%s/%s] %s sent %d bytes to %s", him, nci->rsys,
		s_to_c(from), nbytes, s_to_c(to));
	s_free(to);
	s_free(from);
}

static void
logmsg(char *action)
{
	Link *l;

	if(logged)
		return;

	logged = 1;
	for(l = rcvers.first; l; l = l->next)
		syslog(0, "smtpd", "%s %s (%s/%s) (%s)", action,
			s_to_c(senders.last->p), him, nci->rsys, s_to_c(l->p));
}

static int
optoutall(int filterstate)
{
	Link *l;

	switch(filterstate){
	case ACCEPT:
	case TRUSTED:
		return filterstate;
	}

	for(l = rcvers.first; l; l = l->next)
		if(!optoutofspamfilter(s_to_c(l->p)))
			return filterstate;

	return ACCEPT;
}

String*
startcmd(void)
{
	int n;
	Link *l;
	char **av;
	String *cmd;
	char *filename;

	/*
	 *  ignore the filterstate if the all the receivers prefer it.
	 */
	filterstate = optoutall(filterstate);

	switch (filterstate){
	case BLOCKED:
	case DELAY:
		rejectcount++;
		logmsg("Blocked");
		filename = dumpfile(s_to_c(senders.last->p));
		cmd = s_new();
		s_append(cmd, "cat > ");
		s_append(cmd, filename);
		pp = proc_start(s_to_c(cmd), instream(), 0, outstream(), 0, 0);
		break;
	case DIALUP:
		logmsg("Dialup");
		rejectcount++;
		reply("554 We don't accept mail from dial-up ports.\r\n");
		/*
		 * we could exit here, because we're never going to accept mail from this
		 * ip address, but it's unclear that RFC821 allows that.  Instead we set
		 * the hardreject flag and go stupid.
		 */
		hardreject = 1;
		return 0;
	case DENIED:
		logmsg("Denied");
		rejectcount++;
		reply("554-We don't accept mail from %s.\r\n", s_to_c(senders.last->p));
		reply("554 Contact postmaster@%s for more information.\r\n", dom);
		return 0;
	case REFUSED:
		logmsg("Refused");
		rejectcount++;
		reply("554 Sender domain must exist: %s\r\n", s_to_c(senders.last->p));
		return 0;
	default:
	case NONE:
		logmsg("Confused");
		rejectcount++;
		reply("554-We have had an internal mailer error classifying your message.\r\n");
		reply("554-Filterstate is %d\r\n", filterstate);
		reply("554 Contact postmaster@%s for more information.\r\n", dom);
		return 0;
	case ACCEPT:
	case TRUSTED:
		/*
		 * now that all other filters have been passed,
		 * do grey-list processing.
		 */
		if(gflag)
			vfysenderhostok();

		/*
		 *  set up mail command
		 */
		cmd = s_clone(mailer);
		n = 3;
		for(l = rcvers.first; l; l = l->next)
			n++;
		av = malloc(n*sizeof(char*));
		if(av == nil){
			reply("450 We're busy right now, try later\n");
			s_free(cmd);
			return 0;
		}

			n = 0;
		av[n++] = s_to_c(cmd);
		av[n++] = "-r";
		for(l = rcvers.first; l; l = l->next)
			av[n++] = s_to_c(l->p);
		av[n] = 0;
		/*
		 *  start mail process
		 */
		pp = noshell_proc_start(av, instream(), outstream(), outstream(), 0, 0);
		free(av);
		break;
	}
	if(pp == 0) {
		reply("450 We're busy right now, try later\n");
		s_free(cmd);
		return 0;
	}
	return cmd;
}

/*
 *  print out a header line, expanding any domainless addresses into
 *  address@him
 */
char*
bprintnode(Biobuf *b, Node *p)
{
	if(p->s){
		if(p->addr && strchr(s_to_c(p->s), '@') == nil){
			if(Bprint(b, "%s@%s", s_to_c(p->s), him) < 0)
				return nil;
		} else {
			if(Bwrite(b, s_to_c(p->s), s_len(p->s)) < 0)
				return nil;
		}
	}else{
		if(Bputc(b, p->c) < 0)
			return nil;
	}
	if(p->white)
		if(Bwrite(b, s_to_c(p->white), s_len(p->white)) < 0)
			return nil;
	return p->end+1;
}

static String*
getaddr(Node *p)
{
	for(; p; p = p->next)
		if(p->s && p->addr)
			return p->s;
	return nil;
}

/*
 *  add waring headers of the form
 *	X-warning: <reason>
 *  for any headers that looked like they might be forged.
 *
 *  return byte count of new headers
 */
static int
forgedheaderwarnings(void)
{
	int nbytes;
	Field *f;

	nbytes = 0;

	/* warn about envelope sender */
	if(strcmp(s_to_c(senders.last->p), "/dev/null") != 0 && masquerade(senders.last->p, nil))
		nbytes += Bprint(pp->std[0]->fp, "X-warning: suspect envelope domain\n");

	/*
	 *  check Sender: field.  If it's OK, ignore the others because this is an
	 *  exploded mailing list.
	 */
	for(f = firstfield; f; f = f->next){
		if(f->node->c == SENDER){
			if(masquerade(getaddr(f->node), him))
				nbytes += Bprint(pp->std[0]->fp, "X-warning: suspect Sender: domain\n");
			else
				return nbytes;
		}
	}

	/* check From: */
	for(f = firstfield; f; f = f->next){
		if(f->node->c == FROM && masquerade(getaddr(f->node), him))
			nbytes += Bprint(pp->std[0]->fp, "X-warning: suspect From: domain\n");
	}
	return nbytes;
}

/*
 *  pipe message to mailer with the following transformations:
 *	- change \r\n into \n.
 *	- add sender's domain to any addrs with no domain
 *	- add a From: if none of From:, Sender:, or Replyto: exists
 *	- add a Received: line
 */
int
pipemsg(int *byteswritten)
{
	int status;
	char *cp;
	String *line;
	String *hdr;
	int n, nbytes;
	int sawdot;
	Field *f;
	Node *p;
	Link *l;

	pipesig(&status);	/* set status to 1 on write to closed pipe */
	sawdot = 0;
	status = 0;

	/*
	 *  add a 'From ' line as envelope
	 */
	nbytes = 0;
	nbytes += Bprint(pp->std[0]->fp, "From %s %s remote from \n",
			s_to_c(senders.first->p), thedate());

	/*
	 *  add our own Received: stamp
	 */
	nbytes += Bprint(pp->std[0]->fp, "Received: from %s ", him);
	if(nci->rsys)
		nbytes += Bprint(pp->std[0]->fp, "([%s]) ", nci->rsys);
	nbytes += Bprint(pp->std[0]->fp, "by %s; %s\n", me, thedate());

	/*
	 *  read first 16k obeying '.' escape.  we're assuming
	 *  the header will all be there.
	 */
	line = s_new();
	hdr = s_new();
	while(sawdot == 0 && s_len(hdr) < 16*1024){
		n = getcrnl(s_reset(line), &bin);

		/* eof or error ends the message */
		if(n <= 0)
			break;

		/* a line with only a '.' ends the message */
		cp = s_to_c(line);
		if(n == 2 && *cp == '.' && *(cp+1) == '\n'){
			sawdot = 1;
			break;
		}

		s_append(hdr, *cp == '.' ? cp+1 : cp);
	}

	/*
 	 *  parse header
	 */
	yyinit(s_to_c(hdr), s_len(hdr));
	yyparse();

	/*
 	 *  Look for masquerades.  Let Sender: trump From: to allow mailing list
	 *  forwarded messages.
	 */
	if(fflag)
		nbytes += forgedheaderwarnings();

	/*
	 *  add an orginator and/or destination if either is missing
	 */
	if(originator == 0){
		if(senders.last == nil)
			Bprint(pp->std[0]->fp, "From: /dev/null@%s\n", him);
		else
			Bprint(pp->std[0]->fp, "From: %s\n", s_to_c(senders.last->p));
	}
	if(destination == 0){
		Bprint(pp->std[0]->fp, "To: ");
		for(l = rcvers.first; l; l = l->next){
			if(l != rcvers.first)
				Bprint(pp->std[0]->fp, ", ");
			Bprint(pp->std[0]->fp, "%s", s_to_c(l->p));
		}
		Bprint(pp->std[0]->fp, "\n");
	}

	/*
	 *  add sender's domain to any domainless addresses
	 *  (to avoid forging local addresses)
	 */
	cp = s_to_c(hdr);
	for(f = firstfield; cp != nil && f; f = f->next){
		for(p = f->node; cp != 0 && p; p = p->next)
			cp = bprintnode(pp->std[0]->fp, p);
		if(status == 0 && Bprint(pp->std[0]->fp, "\n") < 0){
			piperror = "write error";
			status = 1;
		}
	}
	if(cp == nil){
		piperror = "sender domain";
		status = 1;
	}

	/* write anything we read following the header */
	if(status == 0 && Bwrite(pp->std[0]->fp, cp, s_to_c(hdr) + s_len(hdr) - cp) < 0){
		piperror = "write error 2";
		status = 1;
	}
	s_free(hdr);

	/*
	 *  pass rest of message to mailer.  take care of '.'
	 *  escapes.
	 */
	while(sawdot == 0){
		n = getcrnl(s_reset(line), &bin);

		/* eof or error ends the message */
		if(n <= 0)
			break;

		/* a line with only a '.' ends the message */
		cp = s_to_c(line);
		if(n == 2 && *cp == '.' && *(cp+1) == '\n'){
			sawdot = 1;
			break;
		}
		nbytes += n;
		if(status == 0 && Bwrite(pp->std[0]->fp, *cp == '.' ? cp+1 : cp, n) < 0){
			piperror = "write error 3";
			status = 1;
		}
	}
	s_free(line);
	if(sawdot == 0){
		/* message did not terminate normally */
		snprint(pipbuf, sizeof pipbuf, "network eof: %r");
		piperror = pipbuf;
		syskillpg(pp->pid);
		status = 1;
	}

	if(status == 0 && Bflush(pp->std[0]->fp) < 0){
		piperror = "write error 4";
		status = 1;
	}
	stream_free(pp->std[0]);
	pp->std[0] = 0;
	*byteswritten = nbytes;
	pipesigoff();
	if(status && !piperror)
		piperror = "write on closed pipe";
	return status;
}

char*
firstline(char *x)
{
	static char buf[128];
	char *p;

	strncpy(buf, x, sizeof(buf));
	buf[sizeof(buf)-1] = 0;
	p = strchr(buf, '\n');
	if(p)
		*p = 0;
	return buf;
}

int
sendermxcheck(void)
{
	char *cp, *senddom, *user;
	char *who;
	int pid;
	Waitmsg *w;
	static char *validate;

	who = s_to_c(senders.first->p);
	if(strcmp(who, "/dev/null") == 0){
		/* /dev/null can only send to one rcpt at a time */
		if(rcvers.first != rcvers.last){
			werrstr("rejected: /dev/null sending to multiple recipients");
			return -1;
		}
		return 0;
	}

	if(validate == nil)
		validate = unsharp("#9/mail/lib/validatesender");
	if(access(validate, AEXEC) < 0)
		return 0;

	senddom = strdup(who);
	if((cp = strchr(senddom, '!')) == nil){
		werrstr("rejected: domainless sender %s", who);
		free(senddom);
		return -1;
	}
	*cp++ = 0;
	user = cp;

	switch(pid = fork()){
	case -1:
		werrstr("deferred: fork: %r");
		return -1;
	case 0:
		/*
		 * Could add an option with the remote IP address
		 * to allow validatesender to implement SPF eventually.
		 */
		execl(validate, "validatesender",
			"-n", nci->root, senddom, user, nil);
		threadexitsall("exec validatesender: %r");
	default:
		break;
	}

	free(senddom);
	w = wait();
	if(w == nil){
		werrstr("deferred: wait failed: %r");
		return -1;
	}
	if(w->pid != pid){
		werrstr("deferred: wait returned wrong pid %d != %d", w->pid, pid);
		free(w);
		return -1;
	}
	if(w->msg[0] == 0){
		free(w);
		return 0;
	}
	/*
	 * skip over validatesender 143123132: prefix from rc.
	 */
	cp = strchr(w->msg, ':');
	if(cp && *(cp+1) == ' ')
		werrstr("%s", cp+2);
	else
		werrstr("%s", w->msg);
	free(w);
	return -1;
}

void
data(void)
{
	String *cmd;
	String *err;
	int status, nbytes;
	char *cp, *ep;
	char errx[ERRMAX];
	Link *l;

	if(rejectcheck())
		return;
	if(senders.last == 0){
		reply("503 Data without MAIL FROM:\r\n");
		rejectcount++;
		return;
	}
	if(rcvers.last == 0){
		reply("503 Data without RCPT TO:\r\n");
		rejectcount++;
		return;
	}
	if(sendermxcheck()){
		rerrstr(errx, sizeof errx);
		if(strncmp(errx, "rejected:", 9) == 0)
			reply("554 %s\r\n", errx);
		else
			reply("450 %s\r\n", errx);
		for(l=rcvers.first; l; l=l->next)
			syslog(0, "smtpd", "[%s/%s] %s -> %s sendercheck: %s",
					him, nci->rsys, s_to_c(senders.first->p),
					s_to_c(l->p), errx);
		rejectcount++;
		return;
	}

	cmd = startcmd();
	if(cmd == 0)
		return;

	reply("354 Input message; end with <CRLF>.<CRLF>\r\n");

	/*
	 *  allow 145 more minutes to move the data
	 */
	alarm(145*60*1000);

	status = pipemsg(&nbytes);

	/*
	 *  read any error messages
	 */
	err = s_new();
	while(s_read_line(pp->std[2]->fp, err))
		;

	alarm(0);
	atnotify(catchalarm, 0);

	status |= proc_wait(pp);
	if(debug){
		seek(2, 0, 2);
		fprint(2, "%d status %ux\n", getpid(), status);
		if(*s_to_c(err))
			fprint(2, "%d error %s\n", getpid(), s_to_c(err));
	}

	/*
	 *  if process terminated abnormally, send back error message
	 */
	if(status){
		int code;

		if(strstr(s_to_c(err), "mail refused")){
			syslog(0, "smtpd", "++[%s/%s] %s %s refused: %s", him, nci->rsys,
				s_to_c(senders.first->p), s_to_c(cmd), firstline(s_to_c(err)));
			code = 554;
		} else {
			syslog(0, "smtpd", "++[%s/%s] %s %s %s%s%sreturned %#q %s", him, nci->rsys,
				s_to_c(senders.first->p), s_to_c(cmd),
				piperror ? "error during pipemsg: " : "",
				piperror ? piperror : "",
				piperror ? "; " : "",
				pp->waitmsg->msg, firstline(s_to_c(err)));
			code = 450;
		}
		for(cp = s_to_c(err); ep = strchr(cp, '\n'); cp = ep){
			*ep++ = 0;
			reply("%d-%s\r\n", code, cp);
		}
		reply("%d mail process terminated abnormally\r\n", code);
	} else {
		/*
		 * if a message appeared on stderr, despite good status,
		 * log it.  this can happen if rewrite.in contains a bad
		 * r.e., for example.
		 */
		if(*s_to_c(err))
			syslog(0, "smtpd",
				"%s returned good status, but said: %s",
				s_to_c(mailer), s_to_c(err));

		if(filterstate == BLOCKED)
			reply("554 we believe this is spam.  we don't accept it.\r\n");
		else
		if(filterstate == DELAY)
			reply("554 There will be a delay in delivery of this message.\r\n");
		else {
			reply("250 sent\r\n");
			logcall(nbytes);
		}
	}
	proc_free(pp);
	pp = 0;
	s_free(cmd);
	s_free(err);

	listfree(&senders);
	listfree(&rcvers);
}

/*
 * when we have blocked a transaction based on IP address, there is nothing
 * that the sender can do to convince us to take the message.  after the
 * first rejection, some spammers continually RSET and give a new MAIL FROM:
 * filling our logs with rejections.  rejectcheck() limits the retries and
 * swiftly rejects all further commands after the first 500-series message
 * is issued.
 */
int
rejectcheck(void)
{

	if(rejectcount > MAXREJECTS){
		syslog(0, "smtpd", "Rejected (%s/%s)", him, nci->rsys);
		reply("554 too many errors.  transaction failed.\r\n");
		threadexitsall("errcount");
	}
	if(hardreject){
		rejectcount++;
		reply("554 We don't accept mail from dial-up ports.\r\n");
	}
	return hardreject;
}

/*
 *  create abs path of the mailer
 */
String*
mailerpath(char *p)
{
	String *s;

	if(p == nil)
		return nil;
	if(*p == '/')
		return s_copy(p);
	s = s_new();
	s_append(s, UPASBIN);
	s_append(s, "/");
	s_append(s, p);
	return s;
}

String *
s_dec64(String *sin)
{
	String *sout;
	int lin, lout;
	lin = s_len(sin);

	/*
	 * if the string is coming from smtpd.y, it will have no nl.
	 * if it is coming from getcrnl below, it will have an nl.
	 */
	if (*(s_to_c(sin)+lin-1) == '\n')
		lin--;
	sout = s_newalloc(lin+1);
	lout = dec64((uchar *)s_to_c(sout), lin, s_to_c(sin), lin);
	if (lout < 0) {
		s_free(sout);
		return nil;
	}
	sout->ptr = sout->base + lout;
	s_terminate(sout);
	return sout;
}

void
starttls(void)
{
	uchar *cert;
	int certlen, fd;
	TLSconn *conn;

	conn = mallocz(sizeof *conn, 1);
	cert = readcert(tlscert, &certlen);
	if (conn == nil || cert == nil) {
		if (conn != nil)
			free(conn);
		reply("454 TLS not available\r\n");
		return;
	}
	reply("220 Go ahead make my day\r\n");
	conn->cert = cert;
	conn->certlen = certlen;
	fd = tlsServer(Bfildes(&bin), conn);
	if (fd < 0) {
		free(cert);
		free(conn);
		syslog(0, "smtpd", "TLS start-up failed with %s", him);

		/* force the client to hang up */
		close(Bfildes(&bin));		/* probably fd 0 */
		close(1);
		threadexitsall("tls failed");
	}
	Bterm(&bin);
	Binit(&bin, fd, OREAD);
	if (dup(fd, 1) < 0)
		fprint(2, "dup of %d failed: %r\n", fd);
	passwordinclear = 1;
	syslog(0, "smtpd", "started TLS with %s", him);
}

void
auth(String *mech, String *resp)
{
	Chalstate *chs = nil;
	AuthInfo *ai = nil;
	String *s_resp1_64 = nil;
	String *s_resp2_64 = nil;
	String *s_resp1 = nil;
	String *s_resp2 = nil;
	char *scratch = nil;
	char *user, *pass;

	if (rejectcheck())
		goto bomb_out;

 	syslog(0, "smtpd", "auth(%s, %s) from %s", s_to_c(mech),
		"(protected)", him);

	if (authenticated) {
	bad_sequence:
		rejectcount++;
		reply("503 Bad sequence of commands\r\n");
		goto bomb_out;
	}
	if (cistrcmp(s_to_c(mech), "plain") == 0) {

		if (!passwordinclear) {
			rejectcount++;
			reply("538 Encryption required for requested authentication mechanism\r\n");
			goto bomb_out;
		}
		s_resp1_64 = resp;
		if (s_resp1_64 == nil) {
			reply("334 \r\n");
			s_resp1_64 = s_new();
			if (getcrnl(s_resp1_64, &bin) <= 0) {
				goto bad_sequence;
			}
		}
		s_resp1 = s_dec64(s_resp1_64);
		if (s_resp1 == nil) {
			rejectcount++;
			reply("501 Cannot decode base64\r\n");
			goto bomb_out;
		}
		memset(s_to_c(s_resp1_64), 'X', s_len(s_resp1_64));
		user = (s_to_c(s_resp1) + strlen(s_to_c(s_resp1)) + 1);
		pass = user + (strlen(user) + 1);
		ai = auth_userpasswd(user, pass);
		authenticated = ai != nil;
		memset(pass, 'X', strlen(pass));
		goto windup;
	}
	else if (cistrcmp(s_to_c(mech), "login") == 0) {

		if (!passwordinclear) {
			rejectcount++;
			reply("538 Encryption required for requested authentication mechanism\r\n");
			goto bomb_out;
		}
		if (resp == nil) {
			reply("334 VXNlcm5hbWU6\r\n");
			s_resp1_64 = s_new();
			if (getcrnl(s_resp1_64, &bin) <= 0)
				goto bad_sequence;
		}
		reply("334 UGFzc3dvcmQ6\r\n");
		s_resp2_64 = s_new();
		if (getcrnl(s_resp2_64, &bin) <= 0)
			goto bad_sequence;
		s_resp1 = s_dec64(s_resp1_64);
		s_resp2 = s_dec64(s_resp2_64);
		memset(s_to_c(s_resp2_64), 'X', s_len(s_resp2_64));
		if (s_resp1 == nil || s_resp2 == nil) {
			rejectcount++;
			reply("501 Cannot decode base64\r\n");
			goto bomb_out;
		}
		ai = auth_userpasswd(s_to_c(s_resp1), s_to_c(s_resp2));
		authenticated = ai != nil;
		memset(s_to_c(s_resp2), 'X', s_len(s_resp2));
	windup:
		if (authenticated)
			reply("235 Authentication successful\r\n");
		else {
			rejectcount++;
			reply("535 Authentication failed\r\n");
		}
		goto bomb_out;
	}
	else if (cistrcmp(s_to_c(mech), "cram-md5") == 0) {
		char *resp;
		int chal64n;
		char *t;

		chs = auth_challenge("proto=cram role=server");
		if (chs == nil) {
			rejectcount++;
			reply("501 Couldn't get CRAM-MD5 challenge\r\n");
			goto bomb_out;
		}
		scratch = malloc(chs->nchal * 2 + 1);
		chal64n = enc64(scratch, chs->nchal * 2, (uchar *)chs->chal, chs->nchal);
		scratch[chal64n] = 0;
		reply("334 %s\r\n", scratch);
		s_resp1_64 = s_new();
		if (getcrnl(s_resp1_64, &bin) <= 0)
			goto bad_sequence;
		s_resp1 = s_dec64(s_resp1_64);
		if (s_resp1 == nil) {
			rejectcount++;
			reply("501 Cannot decode base64\r\n");
			goto bomb_out;
		}
		/* should be of form <user><space><response> */
		resp = s_to_c(s_resp1);
		t = strchr(resp, ' ');
		if (t == nil) {
			rejectcount++;
			reply("501 Poorly formed CRAM-MD5 response\r\n");
			goto bomb_out;
		}
		*t++ = 0;
		chs->user = resp;
		chs->resp = t;
		chs->nresp = strlen(t);
		ai = auth_response(chs);
		authenticated = ai != nil;
		goto windup;
	}
	rejectcount++;
	reply("501 Unrecognised authentication type %s\r\n", s_to_c(mech));
bomb_out:
	if (ai)
		auth_freeAI(ai);
	if (chs)
		auth_freechal(chs);
	if (scratch)
		free(scratch);
	if (s_resp1)
		s_free(s_resp1);
	if (s_resp2)
		s_free(s_resp2);
	if (s_resp1_64)
		s_free(s_resp1_64);
	if (s_resp2_64)
		s_free(s_resp2_64);
}
