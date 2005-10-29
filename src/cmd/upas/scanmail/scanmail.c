#include "common.h"
#include "spam.h"

int	cflag;
int	debug;
int	hflag;
int	nflag;
int	sflag;
int	tflag;
int	vflag;
Biobuf	bin, bout, *cout;

	/* file names */
char	patfile[128];
char	linefile[128];
char	holdqueue[128];
char	copydir[128];

char	header[Hdrsize+2];
char	cmd[1024];
char	**qname;
char	**qdir;
char	*sender;
String	*recips;

char*	canon(Biobuf*, char*, char*, int*);
int	matcher(char*, Pattern*, char*, Resub*);
int	matchaction(int, char*, Resub*);
Biobuf	*opencopy(char*);
Biobuf	*opendump(char*);
char	*qmail(char**, char*, int, Biobuf*);
void	saveline(char*, char*, Resub*);
int	optoutofspamfilter(char*);

void
usage(void)
{
	fprint(2, "missing or bad arguments to qer\n");
	exits("usage");
}

void
regerror(char *s)
{
	fprint(2, "scanmail: %s\n", s);
}

void *
Malloc(long n)
{
	void *p;

	p = malloc(n);
	if(p == 0)
		exits("malloc");
	return p;
}

void*
Realloc(void *p, ulong n)
{
	p = realloc(p, n);
	if(p == 0)
		exits("realloc");
	return p;
}

void
main(int argc, char *argv[])
{
	int i, n, nolines, optout;
	char **args, **a, *cp, *buf;
	char body[Bodysize+2];
	Resub match[1];
	Biobuf *bp;

	optout = 1;
	a = args = Malloc((argc+1)*sizeof(char*));
	sprint(patfile, "%s/patterns", UPASLIB);
	sprint(linefile, "%s/lines", UPASLOG);
	sprint(holdqueue, "%s/queue.hold", SPOOL);
	sprint(copydir, "%s/copy", SPOOL);

	*a++ = argv[0];
	for(argc--, argv++; argv[0] && argv[0][0] == '-'; argc--, argv++){
		switch(argv[0][1]){
		case 'c':			/* save copy of message */
			cflag = 1;
			break;
		case 'd':			/* debug */
			debug++;
			*a++ = argv[0];
			break;
		case 'h':			/* queue held messages by sender domain */
			hflag = 1;		/* -q flag must be set also */
			break;
		case 'n':			/* NOHOLD mode */
			nflag = 1;
			break;
		case 'p':			/* pattern file */
			if(argv[0][2] || argv[1] == 0)
				usage();
			argc--;
			argv++;
			strecpy(patfile, patfile+sizeof patfile, *argv);
			break;
		case 'q':			/* queue name */
			if(argv[0][2] ||  argv[1] == 0)
				usage();
			*a++ = argv[0];
			argc--;
			argv++;
			qname = a;
			*a++ = argv[0];
			break;
		case 's':			/* save copy of dumped message */
			sflag = 1;
			break;
		case 't':			/* test mode - don't log match
						 * and write message to /dev/null
						 */
			tflag = 1;
			break;
		case 'v':			/* vebose - print matches */
			vflag = 1;
			break;
		default:
			*a++ = argv[0];
			break;
		}
	}

	if(argc < 3)
		usage();

	Binit(&bin, 0, OREAD);
	bp = Bopen(patfile, OREAD);
	if(bp){
		parsepats(bp);
		Bterm(bp);
	}
	qdir = a;
	sender = argv[2];

		/* copy the rest of argv, acummulating the recipients as we go */
	for(i = 0; argv[i]; i++){
		*a++ = argv[i];
		if(i < 4)	/* skip queue, 'mail', sender, dest sys */
			continue;
			/* recipients and smtp flags - skip the latter*/
		if(strcmp(argv[i], "-g") == 0){
			*a++ = argv[++i];
			continue;
		}
		if(recips)
			s_append(recips, ", ");
		else
			recips = s_new();
		s_append(recips, argv[i]);
		if(optout && !optoutofspamfilter(argv[i]))
			optout = 0;
	}
	*a = 0;
		/* construct a command string for matching */
	snprint(cmd, sizeof(cmd)-1, "%s %s", sender, s_to_c(recips));
	cmd[sizeof(cmd)-1] = 0;
	for(cp = cmd; *cp; cp++)
		*cp = tolower(*cp);

		/* canonicalize a copy of the header and body.
		 * buf points to orginal message and n contains
		 * number of bytes of original message read during
		 * canonicalization.
		 */
	*body = 0;
	*header = 0;
	buf = canon(&bin, header+1, body+1, &n);
	if (buf == 0)
		exits("read");

		/* if all users opt out, don't try matches */
	if(optout){
		if(cflag)
			cout = opencopy(sender);
		exits(qmail(args, buf, n, cout));
	}

		/* Turn off line logging, if command line matches */
	nolines = matchaction(Lineoff, cmd, match);

	for(i = 0; patterns[i].action; i++){
			/* Lineoff patterns were already done above */
		if(i == Lineoff)
			continue;
			/* don't apply "Line" patterns if excluded above */
		if(nolines && i == SaveLine)
			continue;
			/* apply patterns to the sender/recips, header and body */
		if(matchaction(i, cmd, match))
			break;
		if(matchaction(i, header+1, match))
			break;
		if(i == HoldHeader)
			continue;
		if(matchaction(i, body+1, match))
			break;
	}
	if(cflag && patterns[i].action == 0)	/* no match found - save msg */
		cout = opencopy(sender);

	exits(qmail(args, buf, n, cout));
}

char*
qmail(char **argv, char *buf, int n, Biobuf *cout)
{
	Waitmsg *status;
	int i, pid, pipefd[2];
	char path[512];
	Biobuf *bp;

	pid = 0;
	if(tflag == 0){
		if(pipe(pipefd) < 0)
			exits("pipe");
		pid = fork();
		if(pid == 0){
			dup(pipefd[0], 0);
			for(i = sysfiles(); i >= 3; i--)
				close(i);
			snprint(path, sizeof(path), "%s/qer", UPASBIN);
			*argv=path;
			exec(path, argv);
			exits("exec");
		}
		Binit(&bout, pipefd[1], OWRITE);
		bp = &bout;
	} else
		bp = Bopen("/dev/null", OWRITE);

	while(n > 0){
		Bwrite(bp, buf, n);
		if(cout)
			Bwrite(cout, buf, n);
		n = Bread(&bin, buf, sizeof(buf)-1);
	}
	Bterm(bp);
	if(cout)
		Bterm(cout);
	if(tflag)
		return 0;

	close(pipefd[1]);
	close(pipefd[0]);
	for(;;){
		status = wait();
		if(status == nil || status->pid == pid)
			break;
		free(status);
	}
	if(status == nil)
		strcpy(buf, "wait failed");
	else{
		strcpy(buf, status->msg);
		free(status);
	}
	return buf;
}

char*
canon(Biobuf *bp, char *header, char *body, int *n)
{
	int hsize;
	char *raw;

	hsize = 0;
	*header = 0;
	*body = 0;
	raw = readmsg(bp, &hsize, n);
	if(raw){
		if(convert(raw, raw+hsize, header, Hdrsize, 0))
			conv64(raw+hsize, raw+*n, body, Bodysize);	/* base64 */
		else
			convert(raw+hsize, raw+*n, body, Bodysize, 1);	/* text */
	}
	return raw;
}

int
matchaction(int action, char *message, Resub *m)
{
	char *name;
	Pattern *p;

	if(message == 0 || *message == 0)
		return 0;

	name = patterns[action].action;
	p = patterns[action].strings;
	if(p)
		if(matcher(name, p, message, m))
			return 1;

	for(p = patterns[action].regexps; p; p = p->next)
		if(matcher(name, p, message, m))
			return 1;
	return 0;
}

int
matcher(char *action, Pattern *p, char *message, Resub *m)
{
	char *cp;
	String *s;

	for(cp = message; matchpat(p, cp, m); cp = m->ep){
		switch(p->action){
		case SaveLine:
			if(vflag)
				xprint(2, action, m);
			saveline(linefile, sender, m);
			break;
		case HoldHeader:
		case Hold:
			if(nflag)
				continue;
			if(vflag)
				xprint(2, action, m);
			*qdir = holdqueue;
			if(hflag && qname){
				cp = strchr(sender, '!');
				if(cp){
					*cp = 0;
					*qname = strdup(sender);
					*cp = '!';
				} else
					*qname = strdup(sender);
			}
			return 1;
		case Dump:
			if(vflag)
				xprint(2, action, m);
			*(m->ep) = 0;
			if(!tflag){
				s = s_new();
				s_append(s, sender);
				s = unescapespecial(s);
				syslog(0, "smtpd", "Dumped %s [%s] to %s", s_to_c(s), m->sp,
					s_to_c(s_restart(recips)));
				s_free(s);
			}
			tflag = 1;
			if(sflag)
				cout = opendump(sender);
			return 1;
		default:
			break;
		}
	}
	return 0;
}

void
saveline(char *file, char *sender, Resub *rp)
{
	char *p, *q;
	int i, c;
	Biobuf *bp;

	if(rp->sp == 0 || rp->ep == 0)
		return;
		/* back up approx 20 characters to whitespace */
	for(p = rp->sp, i = 0; *p && i < 20; i++, p--)
			;
	while(*p && *p != ' ')
		p--;
	p++;

		/* grab about 20 more chars beyond the end of the match */
	for(q = rp->ep, i = 0; *q && i < 20; i++, q++)
			;
	while(*q && *q != ' ')
		q++;

	c = *q;
	*q = 0;
	bp = sysopen(file, "al", 0644);
	if(bp){
		Bprint(bp, "%s-> %s\n", sender, p);
		Bterm(bp);
	}
	else if(debug)
		fprint(2, "can't save line: (%s) %s\n", sender, p);
	*q = c;
}

Biobuf*
opendump(char *sender)
{
	int i;
	ulong h;
	char buf[512];
	Biobuf *b;
	char *cp;

	cp = ctime(time(0));
	cp[7] = 0;
	cp[10] = 0;
	if(cp[8] == ' ')
		sprint(buf, "%s/queue.dump/%s%c", SPOOL, cp+4, cp[9]);
	else
		sprint(buf, "%s/queue.dump/%s%c%c", SPOOL, cp+4, cp[8], cp[9]);
	cp = buf+strlen(buf);
	if(access(buf, 0) < 0 && sysmkdir(buf, 0777) < 0){
		syslog(0, "smtpd", "couldn't dump mail from %s: %r", sender);
		return 0;
	}

	h = 0;
	while(*sender)
		h = h*257 + *sender++;
	for(i = 0; i < 50; i++){
		h += lrand();
		sprint(cp, "/%lud", h);
		b = sysopen(buf, "wlc", 0644);
		if(b){
			if(vflag)
				fprint(2, "saving in %s\n", buf);
			return b;
		}
	}
	return 0;
}

Biobuf*
opencopy(char *sender)
{
	int i;
	ulong h;
	char buf[512];
	Biobuf *b;

	h = 0;
	while(*sender)
		h = h*257 + *sender++;
	for(i = 0; i < 50; i++){
		h += lrand();
		sprint(buf, "%s/%lud", copydir, h);
		b = sysopen(buf, "wlc", 0600);
		if(b)
			return b;
	}
	return 0;
}

int
optoutofspamfilter(char *addr)
{
	char *p, *f;
	int rv;

	p = strchr(addr, '!');
	if(p)
		p++;
	else
		p = addr;

	rv = 0;
	f = smprint("/mail/box/%s/nospamfiltering", p);
	if(f != nil){
		rv = access(f, 0)==0;
		free(f);
	}

	return rv;
}
