#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
#include <9pclient.h>
#include <plumb.h>
#include <ctype.h>
#include "dat.h"

char	*maildir = "Mail/";			/* mountpoint of mail file system */
char *mboxname = "mbox";			/* mailboxdir/mboxname is mail spool file */
char	*mailboxdir = nil;				/* nil == /mail/box/$user */
char *fsname;						/* filesystem for mailboxdir/mboxname is at maildir/fsname */
char	*user;
char	*outgoing;
char *srvname;

Window	*wbox;
Message	mbox;
Message	replies;
char		*home;
CFid		*plumbsendfd;
CFid		*plumbseemailfd;
CFid		*plumbshowmailfd;
CFid		*plumbsendmailfd;
Channel	*cplumb;
Channel	*cplumbshow;
Channel	*cplumbsend;
int		wctlfd;
void		mainctl(void*);
void		plumbproc(void*);
void		plumbshowproc(void*);
void		plumbsendproc(void*);
void		plumbthread(void);
void		plumbshowthread(void*);
void		plumbsendthread(void*);

int			shortmenu;

CFsys *mailfs;
CFsys *acmefs;

void
usage(void)
{
	fprint(2, "usage: Mail [-sS] [-n srvname] [-o outgoing] [mailboxname [directoryname]]\n");
	threadexitsall("usage");
}

void
removeupasfs(void)
{
	char buf[256];

	if(strcmp(mboxname, "mbox") == 0)
		return;
	snprint(buf, sizeof buf, "close %s", mboxname);
	fswrite(mbox.ctlfd, buf, strlen(buf));
}

int
ismaildir(char *s)
{
	Dir *d;
	int ret;

	d = fsdirstat(mailfs, s);
	if(d == nil)
		return 0;
	ret = d->qid.type & QTDIR;
	free(d);
	return ret;
}

void
threadmain(int argc, char *argv[])
{
	char *s, *name;
	char err[ERRMAX], *cmd;
	int i, newdir;
	Fmt fmt;

	doquote = needsrcquote;
	quotefmtinstall();

	/* open these early so we won't miss notification of new mail messages while we read mbox */
	if((plumbsendfd = plumbopenfid("send", OWRITE|OCEXEC)) == nil)
		fprint(2, "warning: open plumb/send: %r\n");
	if((plumbseemailfd = plumbopenfid("seemail", OREAD|OCEXEC)) == nil)
		fprint(2, "warning: open plumb/seemail: %r\n");
	if((plumbshowmailfd = plumbopenfid("showmail", OREAD|OCEXEC)) == nil)
		fprint(2, "warning: open plumb/showmail: %r\n");

	shortmenu = 0;
	srvname = "mail";
	ARGBEGIN{
	case 's':
		shortmenu = 1;
		break;
	case 'S':
		shortmenu = 2;
		break;
	case 'o':
		outgoing = EARGF(usage());
		break;
	case 'm':
		smprint(maildir, "%s/", EARGF(usage()));
		break;
	case 'n':
		srvname = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	acmefs = nsmount("acme",nil);
	if(acmefs == nil)
		error("cannot mount acme: %r");
	mailfs = nsmount(srvname, nil);
	if(mailfs == nil)
		error("cannot mount %s: %r", srvname);

	name = "mbox";

	newdir = 1;
	if(argc > 0){
		i = strlen(argv[0]);
		if(argc>2 || i==0)
			usage();
		/* see if the name is that of an existing /mail/fs directory */
		if(argc==1 && argv[0][0] != '/' && ismaildir(argv[0])){
			name = argv[0];
			mboxname = estrdup(name);
			newdir = 0;
		}else{
			if(argv[0][i-1] == '/')
				argv[0][i-1] = '\0';
			s = strrchr(argv[0], '/');
			if(s == nil)
				mboxname = estrdup(argv[0]);
			else{
				*s++ = '\0';
				if(*s == '\0')
					usage();
				mailboxdir = argv[0];
				mboxname = estrdup(s);
			}
			if(argc > 1)
				name = argv[1];
			else
				name = mboxname;
		}
	}

	user = getenv("user");
	if(user == nil)
		user = "none";
	home = getenv("home");
	if(home == nil)
		home = getenv("HOME");
	if(home == nil)
		error("can't find $home");
	if(mailboxdir == nil)
		mailboxdir = estrstrdup(home, "/mail");
	if(outgoing == nil)
		outgoing = estrstrdup(mailboxdir, "/outgoing");

	mbox.ctlfd = fsopen(mailfs, estrstrdup(mboxname, "/ctl"), OWRITE);
	if(mbox.ctlfd == nil)
		error("can't open %s: %r", estrstrdup(mboxname, "/ctl"));

	fsname = estrdup(name);
	if(newdir && argc > 0){
		s = emalloc(5+strlen(mailboxdir)+strlen(mboxname)+strlen(name)+10+1);
		for(i=0; i<10; i++){
			sprint(s, "open %s/%s %s", mailboxdir, mboxname, fsname);
			if(fswrite(mbox.ctlfd, s, strlen(s)) >= 0)
				break;
			err[0] = '\0';
			errstr(err, sizeof err);
			if(strstr(err, "mbox name in use") == nil)
				error("can't create directory %s for mail: %s", name, err);
			free(fsname);
			fsname = emalloc(strlen(name)+10);
			sprint(fsname, "%s-%d", name, i);
		}
		if(i == 10)
			error("can't open %s/%s: %r", mailboxdir, mboxname);
		free(s);
	}

	s = estrstrdup(fsname, "/");
	mbox.name = estrstrdup(maildir, s);
	mbox.level= 0;
	readmbox(&mbox, maildir, s);
	home = getenv("home");
	if(home == nil)
		home = "/";

	wbox = newwindow();
	winname(wbox, mbox.name);
	wintagwrite(wbox, "Put Mail Delmesg ", 3+1+4+1+7+1);
	threadcreate(mainctl, wbox, STACK);

	fmtstrinit(&fmt);
	fmtprint(&fmt, "Mail");
	if(shortmenu)
		fmtprint(&fmt, " -%c", "sS"[shortmenu-1]);
	if(outgoing)
		fmtprint(&fmt, " -o %s", outgoing);
	fmtprint(&fmt, " %s", name);
	cmd = fmtstrflush(&fmt);
	if(cmd == nil)
		sysfatal("out of memory");
	winsetdump(wbox, "/acme/mail", cmd);
	mbox.w = wbox;

	mesgmenu(wbox, &mbox);
	winclean(wbox);

/*	wctlfd = open("/dev/wctl", OWRITE|OCEXEC);	/* for acme window */
	wctlfd = -1;
	cplumb = chancreate(sizeof(Plumbmsg*), 0);
	cplumbshow = chancreate(sizeof(Plumbmsg*), 0);
	if(strcmp(name, "mbox") == 0){
		/*
		 * Avoid creating multiple windows to send mail by only accepting
		 * sendmail plumb messages if we're reading the main mailbox.
		 */
		plumbsendmailfd = plumbopenfid("sendmail", OREAD|OCEXEC);
		cplumbsend = chancreate(sizeof(Plumbmsg*), 0);
		proccreate(plumbsendproc, nil, STACK);
		threadcreate(plumbsendthread, nil, STACK);
	}
	/* start plumb reader as separate proc ... */
	proccreate(plumbproc, nil, STACK);
	proccreate(plumbshowproc, nil, STACK);
	threadcreate(plumbshowthread, nil, STACK);
	fswrite(mbox.ctlfd, "refresh", 7);
	/* ... and use this thread to read the messages */
	plumbthread();
}

void
plumbproc(void* v)
{
	Plumbmsg *m;

	threadsetname("plumbproc");
	for(;;){
		m = plumbrecvfid(plumbseemailfd);
		sendp(cplumb, m);
		if(m == nil)
			threadexits(nil);
	}
}

void
plumbshowproc(void* v)
{
	Plumbmsg *m;

	threadsetname("plumbshowproc");
	for(;;){
		m = plumbrecvfid(plumbshowmailfd);
		sendp(cplumbshow, m);
		if(m == nil)
			threadexits(nil);
	}
}

void
plumbsendproc(void* v)
{
	Plumbmsg *m;

	threadsetname("plumbsendproc");
	for(;;){
		m = plumbrecvfid(plumbsendmailfd);
		sendp(cplumbsend, m);
		if(m == nil)
			threadexits(nil);
	}
}

void
newmesg(char *name, char *digest)
{
	Dir *d;

	if(strncmp(name, mbox.name, strlen(mbox.name)) != 0)
		return;	/* message is about another mailbox */
	if(mesglookupfile(&mbox, name, digest) != nil)
		return;
	if(strncmp(name, "Mail/", 5) == 0)
		name += 5;
	d = fsdirstat(mailfs, name);
	if(d == nil)
		return;
	if(mesgadd(&mbox, mbox.name, d, digest))
		mesgmenunew(wbox, &mbox);
	free(d);
}

void
showmesg(char *name, char *digest)
{
	char *n;
	char *mb;

	mb = mbox.name;
	if(strncmp(name, mb, strlen(mb)) != 0)
		return;	/* message is about another mailbox */
	n = estrdup(name+strlen(mb));
	if(n[strlen(n)-1] != '/')
		n = egrow(n, "/", nil);
	mesgopen(&mbox, mbox.name, name+strlen(mb), nil, 1, digest);
	free(n);
}

void
delmesg(char *name, char *digest, int dodel, char *save)
{
	Message *m;

	m = mesglookupfile(&mbox, name, digest);
	if(m != nil){
		if(save)
			mesgcommand(m, estrstrdup("Save ", save));
		if(dodel)
			mesgmenumarkdel(wbox, &mbox, m, 1);
		else{
			/* notification came from plumber - message is gone */
			mesgmenudel(wbox, &mbox, m);
			if(!m->opened)
				mesgdel(&mbox, m);
		}
	}
}

void
plumbthread(void)
{
	Plumbmsg *m;
	Plumbattr *a;
	char *type, *digest;

	threadsetname("plumbthread");
	while((m = recvp(cplumb)) != nil){
		a = m->attr;
		digest = plumblookup(a, "digest");
		type = plumblookup(a, "mailtype");
		if(type == nil)
			fprint(2, "Mail: plumb message with no mailtype attribute\n");
		else if(strcmp(type, "new") == 0)
			newmesg(m->data, digest);
		else if(strcmp(type, "delete") == 0)
			delmesg(m->data, digest, 0, nil);
		else
			fprint(2, "Mail: unknown plumb attribute %s\n", type);
		plumbfree(m);
	}
	threadexits(nil);
}

void
plumbshowthread(void *v)
{
	Plumbmsg *m;

	USED(v);
	threadsetname("plumbshowthread");
	while((m = recvp(cplumbshow)) != nil){
		showmesg(m->data, plumblookup(m->attr, "digest"));
		plumbfree(m);
	}
	threadexits(nil);
}

void
plumbsendthread(void *v)
{
	Plumbmsg *m;

	USED(v);
	threadsetname("plumbsendthread");
	while((m = recvp(cplumbsend)) != nil){
		mkreply(nil, "Mail", m->data, m->attr, nil);
		plumbfree(m);
	}
	threadexits(nil);
}

int
mboxcommand(Window *w, char *s)
{
	char *args[10], **targs, *save;
	Window *sbox;
	Message *m, *next;
	int ok, nargs, i, j;
	CFid *searchfd;
	char buf[128], *res;

	nargs = tokenize(s, args, nelem(args));
	if(nargs == 0)
		return 0;
	if(strcmp(args[0], "Mail") == 0){
		if(nargs == 1)
			mkreply(nil, "Mail", "", nil, nil);
		else
			mkreply(nil, "Mail", args[1], nil, nil);
		return 1;
	}
	if(strcmp(s, "Del") == 0){
		if(mbox.dirty){
			mbox.dirty = 0;
			fprint(2, "mail: mailbox not written\n");
			return 1;
		}
		if(w != mbox.w){
			windel(w, 1);
			return 1;
		}
		ok = 1;
		for(m=mbox.head; m!=nil; m=next){
			next = m->next;
			if(m->w){
				if(windel(m->w, 0))
					m->w = nil;
				else
					ok = 0;
			}
		}
		for(m=replies.head; m!=nil; m=next){
			next = m->next;
			if(m->w){
				if(windel(m->w, 0))
					m->w = nil;
				else
					ok = 0;
			}
		}
		if(ok){
			windel(w, 1);
			removeupasfs();
			threadexitsall(nil);
		}
		return 1;
	}
	if(strcmp(s, "Put") == 0){
		rewritembox(wbox, &mbox);
		return 1;
	}
	if(strcmp(s, "Get") == 0){
		fswrite(mbox.ctlfd, "refresh", 7);
		return 1;
	}
	if(strcmp(s, "Delmesg") == 0){
		save = nil;
		if(nargs > 1)
			save = args[1];
		s = winselection(w);
		if(s == nil)
			return 1;
		nargs = 1;
		for(i=0; s[i]; i++)
			if(s[i] == '\n')
				nargs++;
		targs = emalloc(nargs*sizeof(char*));	/* could be too many for a local array */
		nargs = getfields(s, targs, nargs, 1, "\n");
		for(i=0; i<nargs; i++){
			if(!isdigit(targs[i][0]))
				continue;
			j = atoi(targs[i]);	/* easy way to parse the number! */
			if(j == 0)
				continue;
			snprint(buf, sizeof buf, "%s%d", mbox.name, j);
			delmesg(buf, nil, 1, save);
		}
		free(s);
		free(targs);
		return 1;
	}
	if(strcmp(s, "Search") == 0){
		if(nargs <= 1)
			return 1;
		s = estrstrdup(mboxname, "/search");
		searchfd = fsopen(mailfs, s, ORDWR);
		if(searchfd == nil)
			return 1;
		save = estrdup(args[1]);
		for(i=2; i<nargs; i++)
			save = eappend(save, " ", args[i]);
		fswrite(searchfd, save, strlen(save));
		fsseek(searchfd, 0, 0);
		j = fsread(searchfd, buf, sizeof buf - 1);
 		if(j == 0){
			fprint(2, "[%s] search %s: no results found\n", mboxname, save);
			fsclose(searchfd);
			free(save);
			return 1;
		}
		free(save);
		buf[j] = '\0';
		res = estrdup(buf);
		j = fsread(searchfd, buf, sizeof buf - 1);
		for(; j != 0; j = fsread(searchfd, buf, sizeof buf - 1), buf[j] = '\0')
			res = eappend(res, "", buf);
		fsclose(searchfd);

		sbox = newwindow();
		winname(sbox, s);
		free(s);
		threadcreate(mainctl, sbox, STACK);
		winopenbody(sbox, OWRITE);

		/* show results in reverse order */
		m = mbox.tail;
		save = nil;
		for(s=strrchr(res, ' '); s!=nil || save!=res; s=strrchr(res, ' ')){
			if(s != nil){
				save = s+1;
				*s = '\0';
			}
			else save = res;
			save = estrstrdup(save, "/");
			for(; m && strcmp(save, m->name) != 0; m=m->prev);
			free(save);
			if(m == nil)
				break;
			fsprint(sbox->body, "%s%s\n", m->name, info(m, 0, 0));
			m = m->prev;
		}
		free(res);
		winclean(sbox);
		winclosebody(sbox);
		return 1;
	}
	return 0;
}

void
mainctl(void *v)
{
	Window *w;
	Event *e, *e2, *eq, *ea;
	int na, nopen;
	char *s, *t, *buf;

	w = v;
	winincref(w);
	proccreate(wineventproc, w, STACK);

	for(;;){
		e = recvp(w->cevent);
		switch(e->c1){
		default:
		Unknown:
			print("unknown message %c%c\n", e->c1, e->c2);
			break;

		case 'E':	/* write to body; can't affect us */
			break;

		case 'F':	/* generated by our actions; ignore */
			break;

		case 'K':	/* type away; we don't care */
			break;

		case 'M':
			switch(e->c2){
			case 'x':
			case 'X':
				ea = nil;
				e2 = nil;
				if(e->flag & 2)
					e2 = recvp(w->cevent);
				if(e->flag & 8){
					ea = recvp(w->cevent);
					na = ea->nb;
					recvp(w->cevent);
				}else
					na = 0;
				s = e->b;
				/* if it's a known command, do it */
				if((e->flag&2) && e->nb==0)
					s = e2->b;
				if(na){
					t = emalloc(strlen(s)+1+na+1);
					sprint(t, "%s %s", s, ea->b);
					s = t;
				}
				/* if it's a long message, it can't be for us anyway */
				if(!mboxcommand(w, s))	/* send it back */
					winwriteevent(w, e);
				if(na)
					free(s);
				break;

			case 'l':
			case 'L':
				buf = nil;
				eq = e;
				if(e->flag & 2){
					e2 = recvp(w->cevent);
					eq = e2;
				}
				s = eq->b;
				if(eq->q1>eq->q0 && eq->nb==0){
					buf = emalloc((eq->q1-eq->q0)*UTFmax+1);
					winread(w, eq->q0, eq->q1, buf);
					s = buf;
				}
				nopen = 0;
				do{
					/* skip 'deleted' string if present' */
					if(strncmp(s, deleted, strlen(deleted)) == 0)
						s += strlen(deleted);
					/* skip mail box name if present */
					if(strncmp(s, mbox.name, strlen(mbox.name)) == 0)
						s += strlen(mbox.name);
					nopen += mesgopen(&mbox, mbox.name, s, nil, 0, nil);
					while(*s!='\0' && *s++!='\n')
						;
				}while(*s);
				if(nopen == 0)	/* send it back */
					winwriteevent(w, e);
				free(buf);
				break;

			case 'I':	/* modify away; we don't care */
			case 'D':
			case 'd':
			case 'i':
				break;

			default:
				goto Unknown;
			}
		}
	}
}
