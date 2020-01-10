#include "common.h"
#include "send.h"

/* globals to all files */
int rmail;
char *thissys, *altthissys;
int nflg;
int xflg;
int debug;
int rflg;
int iflg = 1;
int nosummary;

/* global to this file */
static String *errstring;
static message *mp;
static int interrupt;
static int savemail;
static Biobuf in;
static int forked;
static int add822headers = 1;
static String *arglist;

/* predeclared */
static int	send(dest *, message *, int);
static void	lesstedious(void);
static void	save_mail(message *);
static int	complain_mail(dest *, message *);
static int	pipe_mail(dest *, message *);
static void	appaddr(String *, dest *);
static void	mkerrstring(String *, message *, dest *, dest *, char *, int);
static int	replymsg(String *, message *, dest *);
static int	catchint(void*, char*);

void
usage(void)
{
	fprint(2, "usage: mail [-birtx] list-of-addresses\n");
	exits("usage");
}

void
main(int argc, char *argv[])
{
	dest *dp=0;
	int checkforward;
	char *base;
	int rv;

	/* process args */
	ARGBEGIN{
	case '#':
		nflg = 1;
		break;
	case 'b':
		add822headers = 0;
		break;
	case 'x':
		nflg = 1;
		xflg = 1;
		break;
	case 'd':
		debug = 1;
		break;
	case 'i':
		iflg = 0;
		break;
	case 'r':
		rflg = 1;
		break;
	default:
		usage();
	}ARGEND

	while(*argv){
		if(shellchars(*argv)){
			fprint(2, "illegal characters in destination\n");
			exits("syntax");
		}
		d_insert(&dp, d_new(s_copy(*argv++)));
	}

	if (dp == 0)
		usage();
	arglist = d_to(dp);

	/*
	 * get context:
	 *	- whether we're rmail or mail
	 */
	base = basename(argv0);
	checkforward = rmail = (strcmp(base, "rmail")==0) | rflg;
	thissys = sysname_read();
	altthissys = alt_sysname_read();
	if(rmail)
		add822headers = 0;

	/*
	 *  read the mail.  If an interrupt occurs while reading, save in
	 *  dead.letter
	 */
	if (!nflg) {
		Binit(&in, 0, OREAD);
		if(!rmail)
			atnotify(catchint, 1);
		mp = m_read(&in, rmail, !iflg);
		if (mp == 0)
			exit(0);
		if (interrupt != 0) {
			save_mail(mp);
			exit(1);
		}
	} else {
		mp = m_new();
		if(default_from(mp) < 0){
			fprint(2, "%s: can't determine login name\n", argv0);
			exit(1);
		}
	}
	errstring = s_new();
	getrules();

	/*
	 *  If this is a gateway, translate the sender address into a local
	 *  address.  This only happens if mail to the local address is
	 *  forwarded to the sender.
	 */
	gateway(mp);

	/*
	 *  Protect against shell characters in the sender name for
	 *  security reasons.
	 */
	mp->sender = escapespecial(mp->sender);
	if (shellchars(s_to_c(mp->sender)))
		mp->replyaddr = s_copy("postmaster");
	else
		mp->replyaddr = s_clone(mp->sender);

	/*
	 *  reject messages that have been looping for too long
	 */
	if(mp->received > 32)
		exit(refuse(dp, mp, "possible forward loop", 0, 0));

	/*
	 *  reject messages that are too long.  We don't do it earlier
	 *  in m_read since we haven't set up enough things yet.
	 */
	if(mp->size < 0)
		exit(refuse(dp, mp, "message too long", 0, 0));

	rv = send(dp, mp, checkforward);
	if(savemail)
		save_mail(mp);
	if(mp)
		m_free(mp);
	exit(rv);
}

/* send a message to a list of sites */
static int
send(dest *destp, message *mp, int checkforward)
{
	dest *dp;		/* destination being acted upon */
	dest *bound;		/* bound destinations */
	int errors=0;

	/* bind the destinations to actions */
	bound = up_bind(destp, mp, checkforward);
	if(add822headers && mp->haveto == 0){
		if(nosummary)
			mp->to = d_to(bound);
		else
			mp->to = arglist;
	}

	/* loop through and execute commands */
	for (dp = d_rm(&bound); dp != 0; dp = d_rm(&bound)) {
		switch (dp->status) {
		case d_cat:
			errors += cat_mail(dp, mp);
			break;
		case d_pipeto:
		case d_pipe:
			if (!rmail && !nflg && !forked) {
				forked = 1;
				lesstedious();
			}
			errors += pipe_mail(dp, mp);
			break;
		default:
			errors += complain_mail(dp, mp);
			break;
		}
	}

	return errors;
}

/* avoid user tedium (as Mike Lesk said in a previous version) */
static void
lesstedious(void)
{
	int i;

	if(debug)
		return;

	switch(fork()){
	case -1:
		break;
	case 0:
		sysdetach();
		for(i=0; i<3; i++)
			close(i);
		savemail = 0;
		break;
	default:
		exit(0);
	}
}


/* save the mail */
static void
save_mail(message *mp)
{
	Biobuf *fp;
	String *file;

	file = s_new();
	deadletter(file);
	fp = sysopen(s_to_c(file), "cAt", 0660);
	if (fp == 0)
		return;
	m_bprint(mp, fp);
	sysclose(fp);
	fprint(2, "saved in %s\n", s_to_c(file));
	s_free(file);
}

/* remember the interrupt happened */

static int
catchint(void *a, char *msg)
{
	USED(a);
	if(strstr(msg, "interrupt") || strstr(msg, "hangup")) {
		interrupt = 1;
		return 1;
	}
	return 0;
}

/* dispose of incorrect addresses */
static int
complain_mail(dest *dp, message *mp)
{
	char *msg;

	switch (dp->status) {
	case d_undefined:
		msg = "Invalid address"; /* a little different, for debugging */
		break;
	case d_syntax:
		msg = "invalid address";
		break;
	case d_unknown:
		msg = "unknown user";
		break;
	case d_eloop:
	case d_loop:
		msg = "forwarding loop";
		break;
	case d_noforward:
		if(dp->pstat && *s_to_c(dp->repl2))
			return refuse(dp, mp, s_to_c(dp->repl2), dp->pstat, 0);
		else
			msg = "destination unknown or forwarding disallowed";
		break;
	case d_pipe:
		msg = "broken pipe";
		break;
	case d_cat:
		msg = "broken cat";
		break;
	case d_translate:
		if(dp->pstat && *s_to_c(dp->repl2))
			return refuse(dp, mp, s_to_c(dp->repl2), dp->pstat, 0);
		else
			msg = "name translation failed";
		break;
	case d_alias:
		msg = "broken alias";
		break;
	case d_badmbox:
		msg = "corrupted mailbox";
		break;
	case d_resource:
		return refuse(dp, mp, "out of some resource.  Try again later.", 0, 1);
	default:
		msg = "unknown d_";
		break;
	}
	if (nflg) {
		print("%s: %s\n", msg, s_to_c(dp->addr));
		return 0;
	}
	return refuse(dp, mp, msg, 0, 0);
}

/* dispose of remote addresses */
static int
pipe_mail(dest *dp, message *mp)
{
	dest *next, *list=0;
	String *cmd;
	process *pp;
	int status;
	char *none;
	String *errstring=s_new();

	if (dp->status == d_pipeto)
		none = "none";
	else
		none = 0;
	/*
	 *  collect the arguments
	 */
	next = d_rm_same(&dp);
	if(xflg)
		cmd = s_new();
	else
		cmd = s_clone(next->repl1);
	for(; next != 0; next = d_rm_same(&dp)){
		if(xflg){
			s_append(cmd, s_to_c(next->addr));
			s_append(cmd, "\n");
		} else {
			if (next->repl2 != 0) {
				s_append(cmd, " ");
				s_append(cmd, s_to_c(next->repl2));
			}
		}
		d_insert(&list, next);
	}

	if (nflg) {
		if(xflg)
			print("%s", s_to_c(cmd));
		else
			print("%s\n", s_to_c(cmd));
		s_free(cmd);
		return 0;
	}

	/*
	 *  run the process
	 */
	pp = proc_start(s_to_c(cmd), instream(), 0, outstream(), 1, none);
	if(pp==0 || pp->std[0]==0 || pp->std[2]==0)
		return refuse(list, mp, "out of processes, pipes, or memory", 0, 1);
	pipesig(0);
	m_print(mp, pp->std[0]->fp, thissys, 0);
	pipesigoff();
	stream_free(pp->std[0]);
	pp->std[0] = 0;
	while(s_read_line(pp->std[2]->fp, errstring))
		;
	status = proc_wait(pp);
	proc_free(pp);
	s_free(cmd);

	/*
	 *  return status
	 */
	if (status != 0)
		return refuse(list, mp, s_to_c(errstring), status, 0);
	loglist(list, mp, "remote");
	return 0;
}

static void
appaddr(String *sp, dest *dp)
{
	dest *parent;
	String *s;

	if (dp->parent != 0) {
		for(parent=dp->parent; parent->parent!=0; parent=parent->parent)
			;
		s = unescapespecial(s_clone(parent->addr));
		s_append(sp, s_to_c(s));
		s_free(s);
		s_append(sp, "' alias `");
	}
	s = unescapespecial(s_clone(dp->addr));
	s_append(sp, s_to_c(s));
	s_free(s);
}

/*
 *  reject delivery
 *
 *  returns	0	- if mail has been disposed of
 *		other	- if mail has not been disposed
 */
int
refuse(dest *list, message *mp, char *cp, int status, int outofresources)
{
	String *errstring=s_new();
	dest *dp;
	int rv;

	dp = d_rm(&list);
	mkerrstring(errstring, mp, dp, list, cp, status);

	/*
	 *  log first in case we get into trouble
	 */
	logrefusal(dp, mp, s_to_c(errstring));

	/*
	 *  bulk mail is never replied to, if we're out of resources,
	 *  let the sender try again
	 */
	if(rmail){
		/* accept it or request a retry */
		if(outofresources){
			fprint(2, "Mail %s\n", s_to_c(errstring));
			rv = 1;					/* try again later */
		} else if(mp->bulk)
			rv = 0;					/* silently discard bulk */
		else
			rv = replymsg(errstring, mp, dp);	/* try later if we can't reply */
	} else {
		/* aysnchronous delivery only happens if !rmail */
		if(forked){
			/*
			 *  if spun off for asynchronous delivery, we own the mail now.
			 *  return it or dump it on the floor.  rv really doesn't matter.
			 */
			rv = 0;
			if(!outofresources && !mp->bulk)
				replymsg(errstring, mp, dp);
		} else {
			fprint(2, "Mail %s\n", s_to_c(errstring));
			savemail = 1;
			rv = 1;
		}
	}

	s_free(errstring);
	return rv;
}

/* make the error message */
static void
mkerrstring(String *errstring, message *mp, dest *dp, dest *list, char *cp, int status)
{
	dest *next;
	char smsg[64];
	String *sender;

	sender = unescapespecial(s_clone(mp->sender));

	/* list all aliases */
	s_append(errstring, " from '");
	s_append(errstring, s_to_c(sender));
	s_append(errstring, "'\nto '");
	appaddr(errstring, dp);
	for(next = d_rm(&list); next != 0; next = d_rm(&list)) {
		s_append(errstring, "'\nand '");
		appaddr(errstring, next);
		d_insert(&dp, next);
	}
	s_append(errstring, "'\nfailed with error '");
	s_append(errstring, cp);
	s_append(errstring, "'.\n");

	/* >> and | deserve different flavored messages */
	switch(dp->status) {
	case d_pipe:
		s_append(errstring, "The mailer `");
		s_append(errstring, s_to_c(dp->repl1));
		sprint(smsg, "' returned error status %x.\n\n", status);
		s_append(errstring, smsg);
		break;
	}

	s_free(sender);
}

/*
 *  create a new boundary
 */
static String*
mkboundary(void)
{
	char buf[32];
	int i;
	static int already;

	if(already == 0){
		srand((time(0)<<16)|getpid());
		already = 1;
	}
	strcpy(buf, "upas-");
	for(i = 5; i < sizeof(buf)-1; i++)
		buf[i] = 'a' + nrand(26);
	buf[i] = 0;
	return s_copy(buf);
}

/*
 *  reply with up to 1024 characters of the
 *  original message
 */
static int
replymsg(String *errstring, message *mp, dest *dp)
{
	message *refp = m_new();
	dest *ndp;
	char *rcvr;
	int rv;
	String *boundary;

	boundary = mkboundary();

	refp->bulk = 1;
	refp->rfc822headers = 1;
	rcvr = dp->status==d_eloop ? "postmaster" : s_to_c(mp->replyaddr);
	ndp = d_new(s_copy(rcvr));
	s_append(refp->sender, "postmaster");
	s_append(refp->replyaddr, "/dev/null");
	s_append(refp->date, thedate());
	refp->haveto = 1;
	s_append(refp->body, "To: ");
	s_append(refp->body, rcvr);
	s_append(refp->body, "\n");
	s_append(refp->body, "Subject: bounced mail\n");
	s_append(refp->body, "MIME-Version: 1.0\n");
	s_append(refp->body, "Content-Type: multipart/mixed;\n");
	s_append(refp->body, "\tboundary=\"");
	s_append(refp->body, s_to_c(boundary));
	s_append(refp->body, "\"\n");
	s_append(refp->body, "Content-Disposition: inline\n");
	s_append(refp->body, "\n");
	s_append(refp->body, "This is a multi-part message in MIME format.\n");
	s_append(refp->body, "--");
	s_append(refp->body, s_to_c(boundary));
	s_append(refp->body, "\n");
	s_append(refp->body, "Content-Disposition: inline\n");
	s_append(refp->body, "Content-Type: text/plain; charset=\"US-ASCII\"\n");
	s_append(refp->body, "Content-Transfer-Encoding: 7bit\n");
	s_append(refp->body, "\n");
	s_append(refp->body, "The attached mail");
	s_append(refp->body, s_to_c(errstring));
	s_append(refp->body, "--");
	s_append(refp->body, s_to_c(boundary));
	s_append(refp->body, "\n");
	s_append(refp->body, "Content-Type: message/rfc822\n");
	s_append(refp->body, "Content-Disposition: inline\n\n");
	s_append(refp->body, s_to_c(mp->body));
	s_append(refp->body, "--");
	s_append(refp->body, s_to_c(boundary));
	s_append(refp->body, "--\n");

	refp->size = s_len(refp->body);
	rv = send(ndp, refp, 0);
	m_free(refp);
	d_free(ndp);
	return rv;
}
