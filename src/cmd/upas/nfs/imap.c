/*
 * Locking here is not quite right.
 * Calling qlock(&z->lk) can block the proc,
 * and when it comes back, boxes and msgs might have been freed
 * (if the refresh proc was holding the lock and in the middle of a
 * redial).  I've tried to be careful about not assuming boxes continue
 * to exist across imap commands, but maybe this isn't really tenable.
 * Maybe instead we should ref count the boxes and messages.
 */

#include "a.h"
#include <libsec.h>

struct Imap
{
	int		connected;
	int		autoreconnect;
	int		ticks;	/* until boom! */
	char*	server;
	char*	root;
	char*	user;
	int		mode;
	int		fd;
	Biobuf	b;
	Ioproc*	io;
	QLock	lk;
	QLock	rlk;
	Rendez	r;

	Box*		inbox;
	Box*		box;
	Box*		nextbox;

	/* SEARCH results */
	uint		*uid;
	uint		nuid;

	uint		reply;
};

static struct {
	char *name;
	int flag;
} flagstab[] =
{
	"Junk",	FlagJunk,
	"NonJunk",	FlagNonJunk,
	"\\Answered",	FlagReplied,
	"\\Flagged",	FlagFlagged,
	"\\Deleted",	FlagDeleted,
	"\\Draft",		FlagDraft,
	"\\Recent",	FlagRecent,
	"\\Seen",		FlagSeen,
	"\\NoInferiors",	FlagNoInferiors,
	"\\NoSelect",	FlagNoSelect,
	"\\Marked",	FlagMarked,
	"\\UnMarked",	FlagUnMarked
};

int			chattyimap;

static char	*tag = "#";

static void	checkbox(Imap*, Box*);
static char*	copyaddrs(Sx*);
static void	freeup(UserPasswd*);
static int		getbox(Imap*, Box*);
static int		getboxes(Imap*);
static char*	gsub(char*, char*, char*);
static int		imapcmd(Imap*, Box*, char*, ...);
static Sx*		imapcmdsxlit(Imap*, Box*, char*, ...);
static Sx*		imapcmdsx(Imap*, Box*, char*, ...);
static Sx*		imapcmdsx0(Imap*, char*, ...);
static Sx*		imapvcmdsx(Imap*, Box*, char*, va_list, int);
static Sx*		imapvcmdsx0(Imap*, char*, va_list, int);
static int		imapdial(char*, int);
static int		imaplogin(Imap*);
static int		imapquote(Fmt*);
static int		imapreconnect(Imap*);
static void	imaprefreshthread(void*);
static void	imaptimerproc(void*);
static Sx*		imapwaitsx(Imap*);
static int		isatom(Sx *v, char *name);
static int		islist(Sx *v);
static int		isnil(Sx *v);
static int		isnumber(Sx *sx);
static int		isstring(Sx *sx);
static int		ioimapdial(Ioproc*, char*, int);
static char*	nstring(Sx*);
static void	unexpected(Imap*, Sx*);
static Sx*		zBrdsx(Imap*);

/*
 * Imap connection maintenance and login.
 */

Imap*
imapconnect(char *server, int mode, char *root, char *user)
{
	Imap *z;

	fmtinstall('H', encodefmt);
	fmtinstall('Z', imapquote);

	z = emalloc(sizeof *z);
	z->server = estrdup(server);
	z->mode = mode;
	z->user = user;
	if(root)
		if(root[0] != 0 && root[strlen(root)-1] != '/')
			z->root = smprint("%s/", root);
		else
			z->root = root;
	else
		z->root = "";
	z->fd = -1;
	z->autoreconnect = 0;
	z->io = ioproc();

	qlock(&z->lk);
	if(imapreconnect(z) < 0){
		free(z);
		return nil;
	}

	z->r.l = &z->rlk;
	z->autoreconnect = 1;
	qunlock(&z->lk);

	proccreate(imaptimerproc, z, STACK);
	mailthread(imaprefreshthread, z);

	return z;
}

void
imaphangup(Imap *z, int ticks)
{
	z->ticks = ticks;
	if(ticks == 0){
		close(z->fd);
		z->fd = -1;
	}
}

static int
imapreconnect(Imap *z)
{
	Sx *sx;

	z->autoreconnect = 0;
	z->box = nil;
	z->inbox = nil;

	if(z->fd >= 0){
		close(z->fd);
		z->fd = -1;
	}

	if(chattyimap)
		fprint(2, "dial %s...\n", z->server);
	if((z->fd = ioimapdial(z->io, z->server, z->mode)) < 0)
		return -1;
	z->connected = 1;
	Binit(&z->b, z->fd, OREAD);
	if((sx = zBrdsx(z)) == nil){
		werrstr("no greeting");
		goto err;
	}
	if(chattyimap)
		fprint(2, "<I %#$\n", sx);
	if(sx->nsx >= 2 && isatom(sx->sx[0], "*") && isatom(sx->sx[1], "PREAUTH")){
		freesx(sx);
		goto preauth;
	}
	if(!oksx(sx)){
		werrstr("bad greeting - %#$", sx);
		goto err;
	}
	freesx(sx);
	sx = nil;
	if(imaplogin(z) < 0)
		goto err;
preauth:
	if(getboxes(z) < 0 || getbox(z, z->inbox) < 0)
		goto err;
	z->autoreconnect = 1;
	return 0;

err:
	if(z->fd >= 0){
		close(z->fd);
		z->fd = -1;
	}
	if(sx)
		freesx(sx);
	z->autoreconnect = 1;
	z->connected = 0;
	return -1;
}

static int
imaplogin(Imap *z)
{
	Sx *sx;
	UserPasswd *up;

	if(z->user != nil)
		up = auth_getuserpasswd(auth_getkey, "proto=pass role=client service=imap server=%q user=%q", z->server, z->user);
	else
		up = auth_getuserpasswd(auth_getkey, "proto=pass role=client service=imap server=%q", z->server);
	if(up == nil){
		werrstr("getuserpasswd - %r");
		return -1;
	}

	sx = imapcmdsx(z, nil, "LOGIN %#Z %#Z", up->user, up->passwd);
	freeup(up);
	if(sx == nil)
		return -1;
	if(!oksx(sx)){
		freesx(sx);
		werrstr("login rejected - %#$", sx);
		return -1;
	}
	return 0;
}

static int
getboxes(Imap *z)
{
	int i;
	Box **r, **w, **e;

	for(i=0; i<nboxes; i++){
		boxes[i]->mark = 1;
		boxes[i]->exists = 0;
		boxes[i]->maxseen = 0;
	}
	if(imapcmd(z, nil, "LIST %Z *", z->root) < 0)
		return -1;
	if(z->root != nil && imapcmd(z, nil, "LIST %Z INBOX", "") < 0)
		return -1;
	if(z->nextbox && z->nextbox->mark)
		z->nextbox = nil;
	for(r=boxes, w=boxes, e=boxes+nboxes; r<e; r++){
		if((*r)->mark)
{fprint(2, "*** free box %s %s\n", (*r)->name, (*r)->imapname);
			boxfree(*r);
}
		else
			*w++ = *r;
	}
	nboxes = w - boxes;
	return 0;
}

static int
getbox(Imap *z, Box *b)
{
	int i;
	Msg **r, **w, **e;

	if(b == nil)
		return 0;

	for(i=0; i<b->nmsg; i++)
		b->msg[i]->imapid = 0;
	if(imapcmd(z, b, "UID FETCH 1:* FLAGS") < 0)
		return -1;
	for(r=b->msg, w=b->msg, e=b->msg+b->nmsg; r<e; r++){
		if((*r)->imapid == 0)
			msgfree(*r);
		else{
			(*r)->ix = w-b->msg;
			*w++ = *r;
		}
	}
	b->nmsg = w - b->msg;
	b->imapinit = 1;
	checkbox(z, b);
	return 0;
}

static void
freeup(UserPasswd *up)
{
	memset(up->user, 0, strlen(up->user));
	memset(up->passwd, 0, strlen(up->passwd));
	free(up);
}

static void
imaptimerproc(void *v)
{
	Imap *z;

	z = v;
	for(;;){
		sleep(60*1000);
		qlock(z->r.l);
		rwakeup(&z->r);
		qunlock(z->r.l);
	}
}

static void
checkbox(Imap *z, Box *b)
{
	if(imapcmd(z, b, "NOOP") >= 0){
		if(!b->imapinit)
			getbox(z, b);
		if(!b->imapinit)
			return;
		if(b==z->box && b->exists > b->maxseen){
			imapcmd(z, b, "UID FETCH %d:* FULL",
				b->uidnext);
		}
	}
}

static void
imaprefreshthread(void *v)
{
	Imap *z;

	z = v;
	for(;;){
		qlock(z->r.l);
		rsleep(&z->r);
		qunlock(z->r.l);

		qlock(&z->lk);
		if(z->inbox)
			checkbox(z, z->inbox);
		qunlock(&z->lk);
	}
}

/*
 * Run a single command and return the Sx.  Does NOT redial.
 */
static Sx*
imapvcmdsx0(Imap *z, char *fmt, va_list arg, int dotag)
{
	char *s;
	Fmt f;
	int len;
	Sx *sx;

	if(canqlock(&z->lk))
		abort();

	if(z->fd < 0 || !z->connected)
		return nil;

	fmtstrinit(&f);
	if(dotag)
		fmtprint(&f, "%s ", tag);
	fmtvprint(&f, fmt, arg);
	fmtprint(&f, "\r\n");
	s = fmtstrflush(&f);
	len = strlen(s);
	s[len-2] = 0;
	if(chattyimap)
		fprint(2, "I> %s\n", s);
	s[len-2] = '\r';
	if(iowrite(z->io, z->fd, s, len) < 0){
		z->connected = 0;
		free(s);
		return nil;
	}
	sx = imapwaitsx(z);
	free(s);
	return sx;
}

static Sx*
imapcmdsx0(Imap *z, char *fmt, ...)
{
	va_list arg;
	Sx *sx;

	va_start(arg, fmt);
	sx = imapvcmdsx0(z, fmt, arg, 1);
	va_end(arg);
	return sx;
}

/*
 * Run a single command on box b.  Does redial.
 */
static Sx*
imapvcmdsx(Imap *z, Box *b, char *fmt, va_list arg, int dotag)
{
	int tries;
	Sx *sx;

	tries = 0;
	z->nextbox = b;

	if(z->fd < 0 || !z->connected){
reconnect:
		if(!z->autoreconnect)
			return nil;
		if(imapreconnect(z) < 0)
			return nil;
		if(b && z->nextbox == nil)	/* box disappeared on reconnect */
			return nil;
	}

	if(b && b != z->box){
		if(z->box)
			z->box->imapinit = 0;
		z->box = b;
		if((sx=imapcmdsx0(z, "SELECT %Z", b->imapname)) == nil){
			z->box = nil;
			if(tries++ == 0 && (z->fd < 0 || !z->connected))
				goto reconnect;
			return nil;
		}
		freesx(sx);
	}

	if((sx=imapvcmdsx0(z, fmt, arg, dotag)) == nil){
		if(tries++ == 0 && (z->fd < 0 || !z->connected))
			goto reconnect;
		return nil;
	}
	return sx;
}

static int
imapcmd(Imap *z, Box *b, char *fmt, ...)
{
	Sx *sx;
	va_list arg;

	va_start(arg, fmt);
	sx = imapvcmdsx(z, b, fmt, arg, 1);
	va_end(arg);
	if(sx == nil)
		return -1;
	if(sx->nsx < 2 || !isatom(sx->sx[1], "OK")){
		werrstr("%$", sx);
		freesx(sx);
		return -1;
	}
	freesx(sx);
	return 0;
}

static Sx*
imapcmdsx(Imap *z, Box *b, char *fmt, ...)
{
	Sx *sx;
	va_list arg;

	va_start(arg, fmt);
	sx = imapvcmdsx(z, b, fmt, arg, 1);
	va_end(arg);
	return sx;
}

static Sx*
imapcmdsxlit(Imap *z, Box *b, char *fmt, ...)
{
	Sx *sx;
	va_list arg;

	va_start(arg, fmt);
	sx = imapvcmdsx(z, b, fmt, arg, 0);
	va_end(arg);
	return sx;
}

static Sx*
imapwaitsx(Imap *z)
{
	Sx *sx;

	while((sx = zBrdsx(z)) != nil){
		if(chattyimap)
			fprint(2, "<| %#$\n", sx);
		if(sx->nsx >= 1 && sx->sx[0]->type == SxAtom && cistrcmp(sx->sx[0]->data, tag) == 0)
			return sx;
		if(sx->nsx >= 1 && sx->sx[0]->type == SxAtom && cistrcmp(sx->sx[0]->data, "+") == 0){
			z->reply = 1;
			return sx;
		}
		if(sx->nsx >= 1 && sx->sx[0]->type == SxAtom && strcmp(sx->sx[0]->data, "*") == 0)
			unexpected(z, sx);
		if(sx->type == SxList && sx->nsx == 0){
			freesx(sx);
			break;
		}
		freesx(sx);
	}
	z->connected = 0;
	return nil;
}

/*
 * Imap interface to mail file system.
 */

static void
_bodyname(char *buf, char *ebuf, Part *p, char *extra)
{
	if(buf >= ebuf){
		fprint(2, "***** BUFFER TOO SMALL\n");
		return;
	}
	*buf = 0;
	if(p->parent){
		_bodyname(buf, ebuf, p->parent, "");
		buf += strlen(buf);
		seprint(buf, ebuf, ".%d", p->pix+1);
	}
	buf += strlen(buf);
	seprint(buf, ebuf, "%s", extra);
}

static char*
bodyname(Part *p, char *extra)
{
	static char buf[256];
	memset(buf, 0, sizeof buf);	/* can't see why this is necessary, but it is */
	_bodyname(buf, buf+sizeof buf, p, extra);
	return buf+1;	/* buf[0] == '.' */
}

static void
fetch1(Imap *z, Part *p, char *s)
{
	qlock(&z->lk);
	imapcmd(z, p->msg->box, "UID FETCH %d BODY[%s]",
		p->msg->imapuid, bodyname(p, s));
	qunlock(&z->lk);
}

void
imapfetchrawheader(Imap *z, Part *p)
{
	fetch1(z, p, ".HEADER");
}

void
imapfetchrawmime(Imap *z, Part *p)
{
	fetch1(z, p, ".MIME");
}

void
imapfetchrawbody(Imap *z, Part *p)
{
	fetch1(z, p, ".TEXT");
}

void
imapfetchraw(Imap *z, Part *p)
{
	fetch1(z, p, "");
}

static int
imaplistcmd(Imap *z, Box *box, char *before, Msg **m, uint nm, char *after)
{
	int i, r;
	char *cmd;
	Fmt fmt;

	if(nm == 0)
		return 0;

	fmtstrinit(&fmt);
	fmtprint(&fmt, "%s ", before);
	for(i=0; i<nm; i++){
		if(i > 0)
			fmtrune(&fmt, ',');
		fmtprint(&fmt, "%ud", m[i]->imapuid);
	}
	fmtprint(&fmt, " %s", after);
	cmd = fmtstrflush(&fmt);

	r = 0;
	if(imapcmd(z, box, "%s", cmd) < 0)
		 r = -1;
	free(cmd);
	return r;
}

int
imapcopylist(Imap *z, char *nbox, Msg **m, uint nm)
{
	int rv;
	char *name, *p;

	if(nm == 0)
		return 0;

	qlock(&z->lk);
	if(strcmp(nbox, "mbox") == 0)
		name = estrdup("INBOX");
	else{
		p = esmprint("%s%s", z->root, nbox);
		name = esmprint("%Z", p);
		free(p);
	}
	rv = imaplistcmd(z, m[0]->box, "UID COPY", m, nm, name);
	free(name);
	qunlock(&z->lk);
	return rv;
}

int
imapremovelist(Imap *z, Msg **m, uint nm)
{
	int rv;

	if(nm == 0)
		return 0;

	qlock(&z->lk);
	rv = imaplistcmd(z, m[0]->box, "UID STORE", m, nm, "+FLAGS.SILENT (\\Deleted)");
	/* careful - box might be gone; use z->box instead */
	if(rv == 0 && z->box)
		rv = imapcmd(z, z->box, "EXPUNGE");
	qunlock(&z->lk);
	return rv;
}

int
imapflaglist(Imap *z, int op, int flag, Msg **m, uint nm)
{
	char *mod, *s, *sep;
	int i, rv;
	Fmt fmt;

	if(op > 0)
		mod = "+";
	else if(op == 0)
		mod = "";
	else
		mod = "-";

	fmtstrinit(&fmt);
	fmtprint(&fmt, "%sFLAGS (", mod);
	sep = "";
	for(i=0; i<nelem(flagstab); i++){
		if(flagstab[i].flag & flag){
			fmtprint(&fmt, "%s%s", sep, flagstab[i].name);
			sep = " ";
		}
	}
	fmtprint(&fmt, ")");
	s = fmtstrflush(&fmt);

	qlock(&z->lk);
	rv = imaplistcmd(z, m[0]->box, "UID STORE", m, nm, s);
	qunlock(&z->lk);
	free(s);
	return rv;
}

int
imapsearchbox(Imap *z, Box *b, char *search, Msg ***mm)
{
	uint *uid;
	int i, nuid;
	Msg **m;
	int nm;
	Sx *sx;

	qlock(&z->lk);
	sx = imapcmdsx(z, b, "UID SEARCH CHARSET UTF-8 TEXT {%d}", strlen(search));
	freesx(sx);
	if(!z->reply){
		qunlock(&z->lk);
		return -1;
	}
	if((sx = imapcmdsxlit(z, b, "%s", search)) == nil){
		qunlock(&z->lk);
		return -1;
	}
	if(sx->nsx < 2 || !isatom(sx->sx[1], "OK")){
		werrstr("%$", sx);
		freesx(sx);
		qunlock(&z->lk);
		return -1;
	}
	freesx(sx);

	uid = z->uid;
	nuid = z->nuid;
	z->uid = nil;
	z->nuid = 0;
	z->reply = 0;
	qunlock(&z->lk);

	m = emalloc(nuid*sizeof m[0]);
	nm = 0;
	for(i=0; i<nuid; i++)
		if((m[nm] = msgbyimapuid(b, uid[i], 0)) != nil)
			nm++;
	*mm = m;
	free(uid);
	return nm;
}

void
imapcheckbox(Imap *z, Box *b)
{
	if(b == nil)
		return;
	qlock(&z->lk);
	checkbox(z, b);
	qunlock(&z->lk);
}

/*
 * Imap utility routines
 */
static long
_ioimapdial(va_list *arg)
{
	char *server;
	int mode;

	server = va_arg(*arg, char*);
	mode = va_arg(*arg, int);
	return imapdial(server, mode);
}
static int
ioimapdial(Ioproc *io, char *server, int mode)
{
	return iocall(io, _ioimapdial, server, mode);
}

static long
_ioBrdsx(va_list *arg)
{
	Biobuf *b;
	Sx **sx;

	b = va_arg(*arg, Biobuf*);
	sx = va_arg(*arg, Sx**);
	*sx = Brdsx(b);
	if((*sx) && (*sx)->type == SxList && (*sx)->nsx == 0){
		freesx(*sx);
		*sx = nil;
	}
	return 0;
}
static Sx*
ioBrdsx(Ioproc *io, Biobuf *b)
{
	Sx *sx;

	iocall(io, _ioBrdsx, b, &sx);
	return sx;
}

static Sx*
zBrdsx(Imap *z)
{
	if(z->ticks && --z->ticks==0){
		close(z->fd);
		z->fd = -1;
		return nil;
	}
	return ioBrdsx(z->io, &z->b);
}

static int
imapdial(char *server, int mode)
{
	int p[2];
	int fd[3];
	char *tmp;
	char *fpath;

	switch(mode){
	default:
	case Unencrypted:
		return dial(netmkaddr(server, "tcp", "143"), nil, nil, nil);

	case Starttls:
		werrstr("starttls not supported");
		return -1;

	case Tls:
		if(pipe(p) < 0)
			return -1;
		fd[0] = dup(p[0], -1);
		fd[1] = dup(p[0], -1);
		fd[2] = dup(2, -1);
#ifdef PLAN9PORT
		tmp = esmprint("%s:993", server);
		fpath = searchpath("stunnel3");
		if (!fpath) {
			werrstr("stunnel not found. it is required for tls support.");
			return -1;
		}
		if(threadspawnl(fd, fpath, "stunnel", "-c", "-r", tmp, nil) < 0) {
#else
		tmp = esmprint("tcp!%s!993", server);
		if(threadspawnl(fd, "/bin/tlsclient", "tlsclient", tmp, nil) < 0){
#endif
			free(tmp);
			close(p[0]);
			close(p[1]);
			close(fd[0]);
			close(fd[1]);
			close(fd[2]);
			return -1;
		}
		free(tmp);
		close(p[0]);
		return p[1];

	case Cmd:
		if(pipe(p) < 0)
			return -1;
		fd[0] = dup(p[0], -1);
		fd[1] = dup(p[0], -1);
		fd[2] = dup(2, -1);
		if(threadspawnl(fd, "/usr/local/plan9/bin/rc", "rc", "-c", server, nil) < 0){
			close(p[0]);
			close(p[1]);
			close(fd[0]);
			close(fd[1]);
			close(fd[2]);
			return -1;
		}
		close(p[0]);
		return p[1];
	}
}

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

static int
imapquote(Fmt *f)
{
	char *s, *t;
	int w, quotes;
	Rune r;

	s = va_arg(f->args, char*);
	if(s == nil || *s == '\0')
		return fmtstrcpy(f, "\"\"");

	quotes = 0;
	if(f->flags&FmtSharp)
		quotes = 1;
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

static int
fmttype(char c)
{
	switch(c){
	case 'A':
		return SxAtom;
	case 'L':
		return SxList;
	case 'N':
		return SxNumber;
	case 'S':
		return SxString;
	default:
		return -1;
	}
}

/*
 * Check S expression against format string.
 */
static int
sxmatch(Sx *sx, char *fmt)
{
	int i;

	for(i=0; fmt[i]; i++){
		if(fmt[i] == '*')
			fmt--;	/* like i-- but better */
		if(i == sx->nsx && fmt[i+1] == '*')
			return 1;
		if(i >= sx->nsx)
			return 0;
		if(sx->sx[i] == nil)
			return 0;
		if(sx->sx[i]->type == SxAtom && strcmp(sx->sx[i]->data, "NIL") == 0){
			if(fmt[i] == 'L'){
				free(sx->sx[i]->data);
				sx->sx[i]->data = nil;
				sx->sx[i]->type = SxList;
				sx->sx[i]->sx = nil;
				sx->sx[i]->nsx = 0;
			}
			else if(fmt[i] == 'S'){
				free(sx->sx[i]->data);
				sx->sx[i]->data = nil;
				sx->sx[i]->type = SxString;
			}
		}
		if(sx->sx[i]->type == SxAtom && fmt[i]=='S')
			sx->sx[i]->type = SxString;
		if(sx->sx[i]->type != fmttype(fmt[i])){
			fprint(2, "sxmatch: %$ not %c\n", sx->sx[i], fmt[i]);
			return 0;
		}
	}
	if(i != sx->nsx)
		return 0;
	return 1;
}

/*
 * Check string against format string.
 */
static int
stringmatch(char *fmt, char *s)
{
	for(; *fmt && *s; fmt++, s++){
		switch(*fmt){
		case '0':
			if(*s == ' ')
				break;
			/* fall through */
		case '1':
			if(*s < '0' || *s > '9')
				return 0;
			break;
		case 'A':
			if(*s < 'A' || *s > 'Z')
				return 0;
			break;
		case 'a':
			if(*s < 'a' || *s > 'z')
				return 0;
			break;
		case '+':
			if(*s != '-' && *s != '+')
				return 0;
			break;
		default:
			if(*s != *fmt)
				return 0;
			break;
		}
	}
	if(*fmt || *s)
		return 0;
	return 1;
}

/*
 * Parse simple S expressions and IMAP elements.
 */
static int
isatom(Sx *v, char *name)
{
	int n;

	if(v == nil || v->type != SxAtom)
		return 0;
	n = strlen(name);
	if(cistrncmp(v->data, name, n) == 0)
		if(v->data[n] == 0 || (n>0 && v->data[n-1] == '['))
			return 1;
	return 0;
}

static int
isstring(Sx *sx)
{
	if(sx->type == SxAtom)
		sx->type = SxString;
	return sx->type == SxString;
}

static int
isnumber(Sx *sx)
{
	return sx->type == SxNumber;
}

static int
isnil(Sx *v)
{
	return v == nil ||
		(v->type==SxList && v->nsx == 0) ||
		(v->type==SxAtom && strcmp(v->data, "NIL") == 0);
}

static int
islist(Sx *v)
{
	return isnil(v) || v->type==SxList;
}

static uint
parseflags(Sx *v)
{
	int f, i, j;

	if(v->type != SxList){
		warn("malformed flags: %$", v);
		return 0;
	}
	f = 0;
	for(i=0; i<v->nsx; i++){
		if(v->sx[i]->type != SxAtom)
			continue;
		for(j=0; j<nelem(flagstab); j++)
			if(cistrcmp(v->sx[i]->data, flagstab[j].name) == 0)
				f |= flagstab[j].flag;
	}
	return f;
}

static char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
static int
parsemon(char *s)
{
	int i;

	for(i=0; months[i]; i+=3)
		if(memcmp(s, months+i, 3) == 0)
			return i/3;
	return -1;
}

static uint
parsedate(Sx *v)
{
	Tm tm;
	uint t;
	int delta;
	char *p;

	if(v->type != SxString || !stringmatch("01-Aaa-1111 01:11:11 +1111", v->data)){
	bad:
		warn("bad date: %$", v);
		return 0;
	}

	/* cannot use atoi because 09 is malformed octal! */
	memset(&tm, 0, sizeof tm);
	p = v->data;
	tm.mday = strtol(p, 0, 10);
	tm.mon = parsemon(p+3);
	if(tm.mon == -1)
		goto bad;
	tm.year = strtol(p+7, 0, 10) - 1900;
	tm.hour = strtol(p+12, 0, 10);
	tm.min = strtol(p+15, 0, 10);
	tm.sec = strtol(p+18, 0, 10);
	strcpy(tm.zone, "GMT");

	t = tm2sec(&tm);
	delta = ((p[22]-'0')*10+p[23]-'0')*3600 + ((p[24]-'0')*10+p[25]-'0')*60;
	if(p[21] == '-')
		delta = -delta;

	t -= delta;
	return t;
}

static uint
parsenumber(Sx *v)
{
	if(v->type != SxNumber)
		return 0;
	return v->number;
}

static void
hash(DigestState *ds, char *tag, char *val)
{
	if(val == nil)
		val = "";
	md5((uchar*)tag, strlen(tag)+1, nil, ds);
	md5((uchar*)val, strlen(val)+1, nil, ds);
}

static Hdr*
parseenvelope(Sx *v)
{
	Hdr *hdr;
	uchar digest[16];
	DigestState ds;

	if(v->type != SxList || !sxmatch(v, "SSLLLLLLSS")){
		warn("bad envelope: %$", v);
		return nil;
	}

	hdr = emalloc(sizeof *hdr);
	hdr->date = nstring(v->sx[0]);
	hdr->subject = unrfc2047(nstring(v->sx[1]));
	hdr->from = copyaddrs(v->sx[2]);
	hdr->sender = copyaddrs(v->sx[3]);
	hdr->replyto = copyaddrs(v->sx[4]);
	hdr->to = copyaddrs(v->sx[5]);
	hdr->cc = copyaddrs(v->sx[6]);
	hdr->bcc = copyaddrs(v->sx[7]);
	hdr->inreplyto = unrfc2047(nstring(v->sx[8]));
	hdr->messageid = unrfc2047(nstring(v->sx[9]));

	memset(&ds, 0, sizeof ds);
	hash(&ds, "date", hdr->date);
	hash(&ds, "subject", hdr->subject);
	hash(&ds, "from", hdr->from);
	hash(&ds, "sender", hdr->sender);
	hash(&ds, "replyto", hdr->replyto);
	hash(&ds, "to", hdr->to);
	hash(&ds, "cc", hdr->cc);
	hash(&ds, "bcc", hdr->bcc);
	hash(&ds, "inreplyto", hdr->inreplyto);
	hash(&ds, "messageid", hdr->messageid);
	md5(0, 0, digest, &ds);
	hdr->digest = esmprint("%.16H", digest);

	return hdr;
}

static void
strlwr(char *s)
{
	char *t;

	if(s == nil)
		return;
	for(t=s; *t; t++)
		if('A' <= *t && *t <= 'Z')
			*t += 'a' - 'A';
}

static void
nocr(char *s)
{
	char *r, *w;

	if(s == nil)
		return;
	for(r=w=s; *r; r++)
		if(*r != '\r')
			*w++ = *r;
	*w = 0;
}

/*
 * substitute all occurrences of a with b in s.
 */
static char*
gsub(char *s, char *a, char *b)
{
	char *p, *t, *w, *last;
	int n;

	n = 0;
	for(p=s; (p=strstr(p, a)) != nil; p+=strlen(a))
		n++;
	if(n == 0)
		return s;
	t = emalloc(strlen(s)+n*strlen(b)+1);
	w = t;
	for(p=s; last=p, (p=strstr(p, a)) != nil; p+=strlen(a)){
		memmove(w, last, p-last);
		w += p-last;
		memmove(w, b, strlen(b));
		w += strlen(b);
	}
	strcpy(w, last);
	free(s);
	return t;
}

/*
 * Table-driven IMAP "unexpected response" parser.
 * All the interesting data is in the unexpected responses.
 */
static void xlist(Imap*, Sx*);
static void xrecent(Imap*, Sx*);
static void xexists(Imap*, Sx*);
static void xok(Imap*, Sx*);
static void xflags(Imap*, Sx*);
static void xfetch(Imap*, Sx*);
static void xexpunge(Imap*, Sx*);
static void xbye(Imap*, Sx*);
static void xsearch(Imap*, Sx*);

static struct {
	int		num;
	char		*name;
	char		*fmt;
	void		(*fn)(Imap*, Sx*);
} unextab[] = {
	0,	"BYE",		nil,			xbye,
	0,	"FLAGS",		"AAL",		xflags,
	0,	"LIST",		"AALSS",		xlist,
	0,	"OK",		nil,			xok,
	0,	"SEARCH",	"AAN*",		xsearch,

	1,	"EXISTS",		"ANA",		xexists,
	1,	"EXPUNGE",	"ANA",		xexpunge,
	1,	"FETCH",		"ANAL",		xfetch,
	1,	"RECENT",	"ANA",		xrecent
};

static void
unexpected(Imap *z, Sx *sx)
{
	int i, num;
	char *name;

	if(sx->nsx >= 3 && sx->sx[1]->type == SxNumber && sx->sx[2]->type == SxAtom){
		num = 1;
		name = sx->sx[2]->data;
	}else if(sx->nsx >= 2 && sx->sx[1]->type == SxAtom){
		num = 0;
		name = sx->sx[1]->data;
	}else
		return;

	for(i=0; i<nelem(unextab); i++){
		if(unextab[i].num == num && cistrcmp(unextab[i].name, name) == 0){
			if(unextab[i].fmt && !sxmatch(sx, unextab[i].fmt)){
				warn("malformed %s: %$", name, sx);
				continue;
			}
			unextab[i].fn(z, sx);
		}
	}
}

static int
alldollars(char *s)
{
	for(; *s; s++)
		if(*s != '$')
			return 0;
	return 1;
}

static void
xlist(Imap *z, Sx *sx)
{
	int inbox;
	char *s, *t;
	Box *box;

	s = estrdup(sx->sx[4]->data);
	if(sx->sx[3]->data && strcmp(sx->sx[3]->data, "/") != 0){
		s = gsub(s, "/", "_");
		s = gsub(s, sx->sx[3]->data, "/");
	}

	/*
	 * INBOX is the special imap name for the main mailbox.
	 * All other mailbox names have the root prefix removed, if applicable.
	 */
	inbox = 0;
	if(cistrcmp(s, "INBOX") == 0){
		inbox = 1;
		free(s);
		s = estrdup("mbox");
	} else if(z->root && strstr(s, z->root) == s) {
		t = estrdup(s+strlen(z->root));
		free(s);
		s = t;
	}

	/*
	 * Plan 9 calls the main mailbox mbox.
	 * Rename any existing mbox by appending a $.
	 */
	if(!inbox && strncmp(s, "mbox", 4) == 0 && alldollars(s+4)){
		t = emalloc(strlen(s)+2);
		strcpy(t, s);
		strcat(t, "$");
		free(s);
		s = t;
	}

	box = boxcreate(s);
	if(box == nil)
		return;
	box->imapname = estrdup(sx->sx[4]->data);
	if(inbox)
		z->inbox = box;
	box->mark = 0;
	box->flags = parseflags(sx->sx[2]);
}

static void
xrecent(Imap *z, Sx *sx)
{
	if(z->box)
		z->box->recent = sx->sx[1]->number;
}

static void
xexists(Imap *z, Sx *sx)
{
	if(z->box){
		z->box->exists = sx->sx[1]->number;
		if(z->box->exists < z->box->maxseen)
			z->box->maxseen = z->box->exists;
	}
}

static void
xflags(Imap *z, Sx *sx)
{
	USED(z);
	USED(sx);
	/*
	 * This response contains in sx->sx[2] the list of flags
	 * that can be validly attached to messages in z->box.
	 * We don't have any use for this list, since we
	 * use only the standard flags.
	 */
}

static void
xbye(Imap *z, Sx *sx)
{
	USED(sx);
	close(z->fd);
	z->fd = -1;
	z->connected = 0;
}

static void
xexpunge(Imap *z, Sx *sx)
{
	int i, n;
	Box *b;

	if((b=z->box) == nil)
		return;
	n = sx->sx[1]->number;
	for(i=0; i<b->nmsg; i++){
		if(b->msg[i]->imapid == n){
			msgplumb(b->msg[i], 1);
			msgfree(b->msg[i]);
			b->nmsg--;
			memmove(b->msg+i, b->msg+i+1, (b->nmsg-i)*sizeof b->msg[0]);
			i--;
			b->maxseen--;
			b->exists--;
			continue;
		}
		if(b->msg[i]->imapid > n)
			b->msg[i]->imapid--;
		b->msg[i]->ix = i;
	}
}

static void
xsearch(Imap *z, Sx *sx)
{
	int i;

	free(z->uid);
	z->uid = emalloc((sx->nsx-2)*sizeof z->uid[0]);
	z->nuid = sx->nsx-2;
	for(i=0; i<z->nuid; i++)
		z->uid[i] = sx->sx[i+2]->number;
}

/*
 * Table-driven FETCH message info parser.
 */
static void xmsgflags(Msg*, Sx*, Sx*);
static void xmsgdate(Msg*, Sx*, Sx*);
static void xmsgrfc822size(Msg*, Sx*, Sx*);
static void xmsgenvelope(Msg*, Sx*, Sx*);
static void xmsgbody(Msg*, Sx*, Sx*);
static void xmsgbodydata(Msg*, Sx*, Sx*);

static struct {
	char *name;
	void (*fn)(Msg*, Sx*, Sx*);
} msgtab[] = {
	"FLAGS", xmsgflags,
	"INTERNALDATE", xmsgdate,
	"RFC822.SIZE", xmsgrfc822size,
	"ENVELOPE", xmsgenvelope,
	"BODY", xmsgbody,
	"BODY[", xmsgbodydata
};

static void
xfetch(Imap *z, Sx *sx)
{
	int i, j, n, uid;
	Msg *msg;

	if(z->box == nil){
		warn("FETCH but no open box: %$", sx);
		return;
	}

	/* * 152 FETCH (UID 185 FLAGS () ...) */
	if(sx->sx[3]->nsx%2){
		warn("malformed FETCH: %$", sx);
		return;
	}

	n = sx->sx[1]->number;
	sx = sx->sx[3];
	for(i=0; i<sx->nsx; i+=2){
		if(isatom(sx->sx[i], "UID")){
			if(sx->sx[i+1]->type == SxNumber){
				uid = sx->sx[i+1]->number;
				goto haveuid;
			}
		}
	}
/* This happens: too bad.
	warn("FETCH without UID: %$", sx);
*/
	return;

haveuid:
	msg = msgbyimapuid(z->box, uid, 1);
	if(msg->imapid && msg->imapid != n)
		warn("msg id mismatch: want %d have %d", msg->id, n);
	msg->imapid = n;
	for(i=0; i<sx->nsx; i+=2){
		for(j=0; j<nelem(msgtab); j++)
			if(isatom(sx->sx[i], msgtab[j].name))
				msgtab[j].fn(msg, sx->sx[i], sx->sx[i+1]);
	}
	msgplumb(msg, 0);
}

static void
xmsgflags(Msg *msg, Sx *k, Sx *v)
{
	USED(k);
	msg->flags = parseflags(v);
}

static void
xmsgdate(Msg *msg, Sx *k, Sx *v)
{
	USED(k);
	msg->date = parsedate(v);
}

static void
xmsgrfc822size(Msg *msg, Sx *k, Sx *v)
{
	USED(k);
	msg->size = parsenumber(v);
}

static char*
nstring(Sx *v)
{
	char *p;

	if(isnil(v))
		return estrdup("");
	p = v->data;
	v->data = nil;
	return p;
}

static char*
copyaddrs(Sx *v)
{
	char *s, *sep;
	char *name, *email, *host, *mbox;
	int i;
	Fmt fmt;

	if(v->nsx == 0)
		return nil;

	fmtstrinit(&fmt);
	sep = "";
	for(i=0; i<v->nsx; i++){
		if(!sxmatch(v->sx[i], "SSSS"))
			warn("bad address: %$", v->sx[i]);
		name = unrfc2047(nstring(v->sx[i]->sx[0]));
		/* ignore sx[1] - route */
		mbox = unrfc2047(nstring(v->sx[i]->sx[2]));
		host = unrfc2047(nstring(v->sx[i]->sx[3]));
		if(mbox == nil || host == nil){	/* rfc822 group syntax */
			free(name);
			free(mbox);
			free(host);
			continue;
		}
		email = esmprint("%s@%s", mbox, host);
		free(mbox);
		free(host);
		fmtprint(&fmt, "%s%q %q", sep, name ? name : "", email ? email : "");
		free(name);
		free(email);
		sep = " ";
	}
	s = fmtstrflush(&fmt);
	if(s == nil)
		sysfatal("out of memory");
	return s;
}

static void
xmsgenvelope(Msg *msg, Sx *k, Sx *v)
{
	USED(k);
	hdrfree(msg->part[0]->hdr);
	msg->part[0]->hdr = parseenvelope(v);
}

static struct {
	char *name;
	int offset;
} paramtab[] = {
	"charset",	offsetof(Part, charset),
	"name",		offsetof(Part, filename)
};

static void
parseparams(Part *part, Sx *v)
{
	int i, j;
	char *s, *t, **p;

	if(isnil(v))
		return;
	if(v->nsx%2){
		warn("bad message params: %$", v);
		return;
	}
	for(i=0; i<v->nsx; i+=2){
		s = nstring(v->sx[i]);
		t = nstring(v->sx[i+1]);
		for(j=0; j<nelem(paramtab); j++){
			if(cistrcmp(paramtab[j].name, s) == 0){
				p = (char**)((char*)part+paramtab[j].offset);
				free(*p);
				*p = t;
				t = nil;
				break;
			}
		}
		free(s);
		free(t);
	}
}

static void
parsestructure(Part *part, Sx *v)
{
	int i;
	char *s, *t;

	if(isnil(v))
		return;
	if(v->type != SxList){
	bad:
		warn("bad structure: %$", v);
		return;
	}
	if(islist(v->sx[0])){
		/* multipart */
		for(i=0; i<v->nsx && islist(v->sx[i]); i++)
			parsestructure(partcreate(part->msg, part), v->sx[i]);
		free(part->type);
		if(i != v->nsx-1 || !isstring(v->sx[i])){
			warn("bad multipart structure: %$", v);
			part->type = estrdup("multipart/mixed");
			return;
		}
		s = nstring(v->sx[i]);
		strlwr(s);
		part->type = esmprint("multipart/%s", s);
		free(s);
		return;
	}
	/* single part */
	if(!isstring(v->sx[0]) || v->nsx < 2)
		goto bad;
	s = nstring(v->sx[0]);
	t = nstring(v->sx[1]);
	strlwr(s);
	strlwr(t);
	free(part->type);
	part->type = esmprint("%s/%s", s, t);
	if(v->nsx < 7 || !islist(v->sx[2]) || !isstring(v->sx[3])
	|| !isstring(v->sx[4]) || !isstring(v->sx[5]) || !isnumber(v->sx[6]))
		goto bad;
	parseparams(part, v->sx[2]);
	part->idstr = nstring(v->sx[3]);
	part->desc = nstring(v->sx[4]);
	part->encoding = nstring(v->sx[5]);
	part->size = v->sx[6]->number;
	if(strcmp(s, "message") == 0 && strcmp(t, "rfc822") == 0){
		if(v->nsx < 10 || !islist(v->sx[7]) || !islist(v->sx[8]) || !isnumber(v->sx[9]))
			goto bad;
		part->hdr = parseenvelope(v->sx[7]);
		parsestructure(partcreate(part->msg, part), v->sx[8]);
		part->lines = v->sx[9]->number;
	}
	if(strcmp(s, "text") == 0){
		if(v->nsx < 8 || !isnumber(v->sx[7]))
			goto bad;
		part->lines = v->sx[7]->number;
	}
}

static void
xmsgbody(Msg *msg, Sx *k, Sx *v)
{
	USED(k);
	if(v->type != SxList){
		warn("bad body: %$", v);
		return;
	}
	/*
	 * To follow the structure exactly we should
	 * be doing this to partcreate(msg, msg->part[0]),
	 * and we should leave msg->part[0] with type message/rfc822,
	 * but the extra layer is redundant - what else would be in a mailbox?
	 */
	parsestructure(msg->part[0], v);
	if(msg->box->maxseen < msg->imapid)
		msg->box->maxseen = msg->imapid;
	if(msg->imapuid >= msg->box->uidnext)
		msg->box->uidnext = msg->imapuid+1;
}

static void
xmsgbodydata(Msg *msg, Sx *k, Sx *v)
{
	int i;
	char *name, *p;
	Part *part;

	name = k->data;
	name += 5;	/* body[ */
	p = strchr(name, ']');
	if(p)
		*p = 0;

	/* now name is something like 1 or 3.2.MIME - walk down parts from root */
	part = msg->part[0];


	while('1' <= name[0] && name[0] <= '9'){
		i = strtol(name, &p, 10);
		if(*p == '.')
			p++;
		else if(*p != 0){
			warn("bad body name: %$", k);
			return;
		}
		if((part = subpart(part, i-1)) == nil){
			warn("unknown body part: %$", k);
			return;
		}
		name = p;
	}


	if(cistrcmp(name, "") == 0){
		free(part->raw);
		part->raw = nstring(v);
		nocr(part->raw);
	}else if(cistrcmp(name, "HEADER") == 0){
		free(part->rawheader);
		part->rawheader = nstring(v);
		nocr(part->rawheader);
	}else if(cistrcmp(name, "MIME") == 0){
		free(part->mimeheader);
		part->mimeheader = nstring(v);
		nocr(part->mimeheader);
	}else if(cistrcmp(name, "TEXT") == 0){
		free(part->rawbody);
		part->rawbody = nstring(v);
		nocr(part->rawbody);
	}
}

/*
 * Table-driven OK info parser.
 */
static void xokuidvalidity(Imap*, Sx*);
static void xokpermflags(Imap*, Sx*);
static void xokunseen(Imap*, Sx*);
static void xokreadwrite(Imap*, Sx*);
static void xokreadonly(Imap*, Sx*);

struct {
	char *name;
	char fmt;
	void (*fn)(Imap*, Sx*);
} oktab[] = {
	"UIDVALIDITY", 'N',	xokuidvalidity,
	"PERMANENTFLAGS", 'L',	xokpermflags,
	"UNSEEN", 'N',	xokunseen,
	"READ-WRITE", 0,	xokreadwrite,
	"READ-ONLY",	0, xokreadonly
};

static void
xok(Imap *z, Sx *sx)
{
	int i;
	char *name;
	Sx *arg;

	if(sx->nsx >= 4 && sx->sx[2]->type == SxAtom && sx->sx[2]->data[0] == '['){
		if(sx->sx[3]->type == SxAtom && sx->sx[3]->data[0] == ']')
			arg = nil;
		else if(sx->sx[4]->type == SxAtom && sx->sx[4]->data[0] == ']')
			arg = sx->sx[3];
		else{
			warn("cannot parse OK: %$", sx);
			return;
		}
		name = sx->sx[2]->data+1;
		for(i=0; i<nelem(oktab); i++){
			if(cistrcmp(name, oktab[i].name) == 0){
				if(oktab[i].fmt && (arg==nil || arg->type != fmttype(oktab[i].fmt))){
					warn("malformed %s: %$", name, arg);
					continue;
				}
				oktab[i].fn(z, arg);
			}
		}
	}
}

static void
xokuidvalidity(Imap *z, Sx *sx)
{
	int i;
	Box *b;

	if((b=z->box) == nil)
		return;
	if(b->validity != sx->number){
		b->validity = sx->number;
		b->uidnext = 1;
		for(i=0; i<b->nmsg; i++)
			msgfree(b->msg[i]);
		free(b->msg);
		b->msg = nil;
		b->nmsg = 0;
	}
}

static void
xokpermflags(Imap *z, Sx *sx)
{
	USED(z);
	USED(sx);
/*	z->permflags = parseflags(sx); */
}

static void
xokunseen(Imap *z, Sx *sx)
{
	USED(z);
	USED(sx);
/*	z->unseen = sx->number; */
}

static void
xokreadwrite(Imap *z, Sx *sx)
{
	USED(z);
	USED(sx);
/*	z->boxmode = ORDWR; */
}

static void
xokreadonly(Imap *z, Sx *sx)
{
	USED(z);
	USED(sx);
/*	z->boxmode = OREAD; */
}
