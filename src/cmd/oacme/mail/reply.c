#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
#include <ctype.h>
#include <plumb.h>
#include <9pclient.h>
#include "dat.h"

static int	replyid;

int
quote(Message *m, CFid *fid, char *dir, char *quotetext)
{
	char *body, *type;
	int i, n, nlines;
	char **lines;

	if(quotetext){
		body = quotetext;
		n = strlen(body);
		type = nil;
	}else{
		/* look for first textual component to quote */
		type = readfile(dir, "type", &n);
		if(type == nil){
			print("no type in %s\n", dir);
			return 0;
		}
		if(strncmp(type, "multipart/", 10)==0 || strncmp(type, "message/", 8)==0){
			dir = estrstrdup(dir, "1/");
			if(quote(m, fid, dir, nil)){
				free(type);
				free(dir);
				return 1;
			}
			free(dir);
		}
		if(strncmp(type, "text", 4) != 0){
			free(type);
			return 0;
		}
		body = readbody(m->type, dir, &n);
		if(body == nil)
			return 0;
	}
	nlines = 0;
	for(i=0; i<n; i++)
		if(body[i] == '\n')
			nlines++;
	nlines++;
	lines = emalloc(nlines*sizeof(char*));
	nlines = getfields(body, lines, nlines, 0, "\n");
	/* delete leading and trailing blank lines */
	i = 0;
	while(i<nlines && lines[i][0]=='\0')
		i++;
	while(i<nlines && lines[nlines-1][0]=='\0')
		nlines--;
	while(i < nlines){
		fsprint(fid, ">%s%s\n", lines[i][0]=='>'? "" : " ", lines[i]);
		i++;
	}
	free(lines);
	free(body);	/* will free quotetext if non-nil */
	free(type);
	return 1;
}

void
mkreply(Message *m, char *label, char *to, Plumbattr *attr, char *quotetext)
{
	char buf[100];
	CFid *fd;
	Message *r;
	char *dir, *t;
	int quotereply;
	Plumbattr *a;

	quotereply = (label[0] == 'Q');

	if(quotereply && m && m->replywinid > 0){
		snprint(buf, sizeof buf, "%d/body", m->replywinid);
		if((fd = fsopen(acmefs, buf, OWRITE)) != nil){
			dir = estrstrdup(mbox.name, m->name);
			quote(m, fd, dir, quotetext);
			free(dir);
			return;
		}
	}

	r = emalloc(sizeof(Message));
	r->isreply = 1;
	if(m != nil)
		r->replyname = estrdup(m->name);
	r->next = replies.head;
	r->prev = nil;
	if(replies.head != nil)
		replies.head->prev = r;
	replies.head = r;
	if(replies.tail == nil)
		replies.tail = r;
	r->name = emalloc(strlen(mbox.name)+strlen(label)+10);
	sprint(r->name, "%s%s%d", mbox.name, label, ++replyid);
	r->w = newwindow();
	if(m)
		m->replywinid = r->w->id;
	winname(r->w, r->name);
	ctlprint(r->w->ctl, "cleartag");
	wintagwrite(r->w, "fmt Look Post Undo", 4+5+5+4);
	r->tagposted = 1;
	threadcreate(mesgctl, r, STACK);
	winopenbody(r->w, OWRITE);
	if(to!=nil && to[0]!='\0')
		fsprint(r->w->body, "%s\n", to);
	for(a=attr; a; a=a->next)
		fsprint(r->w->body, "%s: %s\n", a->name, a->value);
	dir = nil;
	if(m != nil){
		dir = estrstrdup(mbox.name, m->name);
		if(to == nil && attr == nil){
			/* Reply goes to replyto; Reply all goes to From and To and CC */
			if(strstr(label, "all") == nil)
				fsprint(r->w->body, "To: %s\n", m->replyto);
			else{	/* Replyall */
				if(strlen(m->from) > 0)
					fsprint(r->w->body, "To: %s\n", m->from);
				if(strlen(m->to) > 0)
					fsprint(r->w->body, "To: %s\n", m->to);
				if(strlen(m->cc) > 0)
					fsprint(r->w->body, "CC: %s\n", m->cc);
			}
		}
		if(strlen(m->subject) > 0){
			t = "Subject: Re: ";
			if(strlen(m->subject) >= 3)
				if(tolower(m->subject[0])=='r' && tolower(m->subject[1])=='e' && m->subject[2]==':')
					t = "Subject: ";
			fsprint(r->w->body, "%s%s\n", t, m->subject);
		}
		if(!quotereply){
			fsprint(r->w->body, "Include: %sraw\n", dir);
			free(dir);
		}
	}
	fsprint(r->w->body, "\n");
	if(m == nil)
		fsprint(r->w->body, "\n");
	else if(quotereply){
		quote(m, r->w->body, dir, quotetext);
		free(dir);
	}
	winclosebody(r->w);
	if(m==nil && (to==nil || to[0]=='\0'))
		winselect(r->w, "0", 0);
	else
		winselect(r->w, "$", 0);
	winclean(r->w);
	windormant(r->w);
}

void
delreply(Message *m)
{
	if(m->next == nil)
		replies.tail = m->prev;
	else
		m->next->prev = m->prev;
	if(m->prev == nil)
		replies.head = m->next;
	else
		m->prev->next = m->next;
	mesgfreeparts(m);
	free(m);
}

/* copy argv to stack and free the incoming strings, so we don't leak argument vectors */
void
buildargv(char **inargv, char *argv[NARGS+1], char args[NARGCHAR])
{
	int i, n;
	char *s, *a;

	s = args;
	for(i=0; i<NARGS; i++){
		a = inargv[i];
		if(a == nil)
			break;
		n = strlen(a)+1;
		if((s-args)+n >= NARGCHAR)	/* too many characters */
			break;
		argv[i] = s;
		memmove(s, a, n);
		s += n;
		free(a);
	}
	argv[i] = nil;
}

void
execproc(void *v)
{
	struct Exec *e;
	int p[2], q[2];
	char *prog;
	char *argv[NARGS+1], args[NARGCHAR];
	int fd[3];

	e = v;
	p[0] = e->p[0];
	p[1] = e->p[1];
	q[0] = e->q[0];
	q[1] = e->q[1];
	prog = e->prog;	/* known not to be malloc'ed */

	fd[0] = dup(p[0], -1);
	if(q[0])
		fd[1] = dup(q[1], -1);
	else
		fd[1] = dup(1, -1);
	fd[2] = dup(2, -2);
	sendul(e->sync, 1);
	buildargv(e->argv, argv, args);
	free(e->argv);
	chanfree(e->sync);
	free(e);

	threadexec(nil, fd, prog, argv);
	close(fd[0]);
	close(fd[1]);
	close(fd[2]);

	fprint(2, "Mail: can't exec %s: %r\n", prog);
	threadexits("can't exec");
}

enum{
	ATTACH,
	BCC,
	CC,
	FROM,
	INCLUDE,
	TO
};

char *headers[] = {
	"attach:",
	"bcc:",
	"cc:",
	"from:",
	"include:",
	"to:",
	nil
};

int
whichheader(char *h)
{
	int i;

	for(i=0; headers[i]!=nil; i++)
		if(cistrcmp(h, headers[i]) == 0)
			return i;
	return -1;
}

char *tolist[200];
char	*cclist[200];
char	*bcclist[200];
int ncc, nbcc, nto;
char	*attlist[200];
char	included[200];

int
addressed(char *name)
{
	int i;

	for(i=0; i<nto; i++)
		if(strcmp(name, tolist[i]) == 0)
			return 1;
	for(i=0; i<ncc; i++)
		if(strcmp(name, cclist[i]) == 0)
			return 1;
	for(i=0; i<nbcc; i++)
		if(strcmp(name, bcclist[i]) == 0)
			return 1;
	return 0;
}

char*
skipbl(char *s, char *e)
{
	while(s < e){
		if(*s!=' ' && *s!='\t' && *s!=',')
			break;
		s++;
	}
	return s;
}

char*
findbl(char *s, char *e)
{
	while(s < e){
		if(*s==' ' || *s=='\t' || *s==',')
			break;
		s++;
	}
	return s;
}

/*
 * comma-separate possibly blank-separated strings in line; e points before newline
 */
void
commas(char *s, char *e)
{
	char *t;

	/* may have initial blanks */
	s = skipbl(s, e);
	while(s < e){
		s = findbl(s, e);
		if(s == e)
			break;
		t = skipbl(s, e);
		if(t == e)	/* no more words */
			break;
		/* patch comma */
		*s++ = ',';
		while(s < t)
			*s++ = ' ';
	}
}

int
print2(int fd, int ofd, char *fmt, ...)
{
	int m, n;
	char *s;
	va_list arg;

	va_start(arg, fmt);
	s = vsmprint(fmt, arg);
	va_end(arg);
	if(s == nil)
		return -1;
	m = strlen(s);
	n = write(fd, s, m);
	if(ofd > 0)
		write(ofd, s, m);
	return n;
}

void
write2(int fd, int ofd, char *buf, int n, int nofrom)
{
	char *from, *p;
	int m;

	write(fd, buf, n);

	if(ofd <= 0)
		return;

	if(nofrom == 0){
		write(ofd, buf, n);
		return;
	}

	/* need to escape leading From lines to avoid corrupting 'outgoing' mailbox */
	for(p=buf; *p; p+=m){
		from = cistrstr(p, "from");
		if(from == nil)
			m = n;
		else
			m = from - p;
		if(m > 0)
			write(ofd, p, m);
		if(from){
			if(p==buf || from[-1]=='\n')
				write(ofd, " ", 1);	/* escape with space if From is at start of line */
			write(ofd, from, 4);
			m += 4;
		}
		n -= m;
	}
}

void
mesgsend(Message *m)
{
	char *s, *body, *to;
	int i, j, h, n, natt, p[2];
	struct Exec *e;
	Channel *sync;
	int first, nfld, delit, ofd;
	char *copy, *fld[100], *now;

	body = winreadbody(m->w, &n);
	/* assemble to: list from first line, to: line, and cc: line */
	nto = 0;
	natt = 0;
	ncc = 0;
	nbcc = 0;
	first = 1;
	to = body;
	for(;;){
		for(s=to; *s!='\n'; s++)
			if(*s == '\0'){
				free(body);
				return;
			}
		if(s++ == to)	/* blank line */
			break;
		/* make copy of line to tokenize */
		copy = emalloc(s-to);
		memmove(copy, to, s-to);
		copy[s-to-1] = '\0';
		nfld = tokenizec(copy, fld, nelem(fld), ", \t");
		if(nfld == 0){
			free(copy);
			break;
		}
		n -= s-to;
		switch(h = whichheader(fld[0])){
		case TO:
		case FROM:
			delit = 1;
			commas(to+strlen(fld[0]), s-1);
			for(i=1; i<nfld && nto<nelem(tolist); i++)
				if(!addressed(fld[i]))
					tolist[nto++] = estrdup(fld[i]);
			break;
		case BCC:
			delit = 1;
			commas(to+strlen(fld[0]), s-1);
			for(i=1; i<nfld && nbcc<nelem(bcclist); i++)
				if(!addressed(fld[i]))
					bcclist[nbcc++] = estrdup(fld[i]);
			break;
		case CC:
			delit = 1;
			commas(to+strlen(fld[0]), s-1);
			for(i=1; i<nfld && ncc<nelem(cclist); i++)
				if(!addressed(fld[i]))
					cclist[ncc++] = estrdup(fld[i]);
			break;
		case ATTACH:
		case INCLUDE:
			delit = 1;
			for(i=1; i<nfld && natt<nelem(attlist); i++){
				attlist[natt] = estrdup(fld[i]);
				included[natt++] = (h == INCLUDE);
			}
			break;
		default:
			if(first){
				delit = 1;
				for(i=0; i<nfld && nto<nelem(tolist); i++)
					tolist[nto++] = estrdup(fld[i]);
			}else	/* ignore it */
				delit = 0;
			break;
		}
		if(delit){
			/* delete line from body */
			memmove(to, s, n+1);
		}else
			to = s;
		free(copy);
		first = 0;
	}

	ofd = open(outgoing, OWRITE|OCEXEC);	/* no error check necessary */
	if(ofd > 0){
		/* From dhog Fri Aug 24 22:13:00 EDT 2001 */
		now = ctime(time(0));
		seek(ofd, 0, 2);
		fprint(ofd, "From %s %s", user, now);
		fprint(ofd, "From: %s\n", user);
		fprint(ofd, "Date: %s", now);
		for(i=0; i<natt; i++)
			if(included[i])
				fprint(ofd, "Include: %s\n", attlist[i]);
			else
				fprint(ofd, "Attach: %s\n", attlist[i]);
		/* needed because mail is by default Latin-1 */
		fprint(ofd, "Content-Type: text/plain; charset=\"UTF-8\"\n");
		fprint(ofd, "Content-Transfer-Encoding: 8bit\n");
	}

	e = emalloc(sizeof(struct Exec));
	if(pipe(p) < 0)
		error("can't create pipe: %r");
	e->p[0] = p[0];
	e->p[1] = p[1];
	e->prog = unsharp("#9/bin/upas/marshal");
	e->argv = emalloc((1+1+2+4*natt+1)*sizeof(char*));
	e->argv[0] = estrdup("marshal");
	e->argv[1] = estrdup("-8");
	j = 2;
	if(m->replyname){
		e->argv[j++] = estrdup("-R");
		e->argv[j++] = estrstrdup(mbox.name, m->replyname);
	}
	for(i=0; i<natt; i++){
		if(included[i])
			e->argv[j++] = estrdup("-A");
		else
			e->argv[j++] = estrdup("-a");
		e->argv[j++] = estrdup(attlist[i]);
	}
	sync = chancreate(sizeof(int), 0);
	e->sync = sync;
	proccreate(execproc, e, EXECSTACK);
	recvul(sync);
	/* close(p[0]); */

	/* using marshal -8, so generate rfc822 headers */
	if(nto > 0){
		print2(p[1], ofd, "To: ");
		for(i=0; i<nto-1; i++)
			print2(p[1], ofd, "%s, ", tolist[i]);
		print2(p[1], ofd, "%s\n", tolist[i]);
	}
	if(ncc > 0){
		print2(p[1], ofd, "CC: ");
		for(i=0; i<ncc-1; i++)
			print2(p[1], ofd, "%s, ", cclist[i]);
		print2(p[1], ofd, "%s\n", cclist[i]);
	}
	if(nbcc > 0){
		print2(p[1], ofd, "BCC: ");
		for(i=0; i<nbcc-1; i++)
			print2(p[1], ofd, "%s, ", bcclist[i]);
		print2(p[1], ofd, "%s\n", bcclist[i]);
	}

	i = strlen(body);
	if(i > 0)
		write2(p[1], ofd, body, i, 1);

	/* guarantee a blank line, to ensure attachments are separated from body */
	if(i==0 || body[i-1]!='\n')
		write2(p[1], ofd, "\n\n", 2, 0);
	else if(i>1 && body[i-2]!='\n')
		write2(p[1], ofd, "\n", 1, 0);

	/* these look like pseudo-attachments in the "outgoing" box */
	if(ofd>0 && natt>0){
		for(i=0; i<natt; i++)
			if(included[i])
				fprint(ofd, "=====> Include: %s\n", attlist[i]);
			else
				fprint(ofd, "=====> Attach: %s\n", attlist[i]);
	}
	if(ofd > 0)
		write(ofd, "\n", 1);

	for(i=0; i<natt; i++)
		free(attlist[i]);
	close(ofd);
	close(p[1]);
	free(body);

	if(m->replyname != nil)
		mesgmenumark(mbox.w, m->replyname, "\t[replied]");
	if(m->name[0] == '/')
		s = estrdup(m->name);
	else
		s = estrstrdup(mbox.name, m->name);
	s = egrow(s, "-R", nil);
	winname(m->w, s);
	free(s);
	winclean(m->w);
	/* mark message unopened because it's no longer the original message */
	m->opened = 0;
}
