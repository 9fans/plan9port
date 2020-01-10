/*
 * Mail file system.
 *
 * Serve the bulk of requests out of memory, so they can
 * be in the main loop (will never see their flushes).
 * Some requests do block and they get handled in
 * separate threads.  They're all okay to give up on
 * early, though, so we just respond saying interrupted
 * and then when they finish, silently discard the request.

TO DO:

	decode subject, etc.
	decode body

	digest
	disposition
	filename

	ctl messages

	fetch mail on demand

 */

#include "a.h"

enum
{
	/* directories */
	Qroot,
	Qbox,
	Qmsg,

	/* control files */
	Qctl,
	Qboxctl,
	Qsearch,

	/* message header - same order as struct Hdr */
	Qdate,
	Qsubject,
	Qfrom,
	Qsender,
	Qreplyto,
	Qto,
	Qcc,
	Qbcc,
	Qinreplyto,
	Qmessageid,

	/* part data - same order as stuct Part */
	Qtype,
	Qidstr,
	Qdesc,
	Qencoding,	/* only here temporarily! */
	Qcharset,
	Qfilename,
	Qraw,
	Qrawheader,
	Qrawbody,
	Qmimeheader,

	/* part numbers - same order as struct Part */
	Qsize,
	Qlines,

	/* other message files */
	Qbody,
	Qheader,
	Qdigest,
	Qdisposition,
	Qflags,
	Qinfo,
	Qrawunix,
	Qunixdate,
	Qunixheader,

	Qfile0 = Qbody,
	Qnfile = Qunixheader+1-Qfile0
};

static char Egreg[] = "gone postal";
static char Enobox[] = "no such mailbox";
static char Enomsg[] = "no such message";
static char Eboxgone[] = "mailbox not available";
static char Emsggone[] = "message not available";
static char Eperm[] = "permission denied";
static char Ebadctl[] = "bad control message";

Channel *fsreqchan;
Srv fs;
Qid rootqid;
ulong t0;

#ifdef PLAN9PORT
void
responderror(Req *r)
{
	char e[ERRMAX];

	rerrstr(e, sizeof e);
	respond(r, e);
}
#endif

int
qtype(Qid q)
{
	return q.path&0x3F;
}

int
qboxid(Qid q)
{
	return (q.path>>40)&0xFFFF;
}

int
qmsgid(Qid q)
{
	return ((q.path>>32)&0xFF000000) | ((q.path>>16)&0xFFFFFF);
}

int
qpartid(Qid q)
{
	return ((q.path>>6)&0x3FF);
}

Qid
qid(int ctl, Box *box, Msg *msg, Part *part)
{
	Qid q;

	q.type = 0;
	if(ctl == Qroot || ctl == Qbox || ctl == Qmsg)
		q.type = QTDIR;
	q.path = (vlong)((msg ? msg->id : 0)&0xFF000000)<<32;
	q.path |= (vlong)((msg ? msg->id : 0)&0xFFFFFF)<<16;
	q.path |= (vlong)((box ? box->id : 0)&0xFFFF)<<40;
	q.path |= ((part ? part->ix : 0)&0x3FF)<<6;
	q.path |= ctl&0x3F;
	q.vers = box ? box->validity : 0;
	return q;
}

int
parseqid(Qid q, Box **box, Msg **msg, Part **part)
{
	*msg = nil;
	*part = nil;

	*box = boxbyid(qboxid(q));
	if(*box){
		*msg = msgbyid(*box, qmsgid(q));
	}
	if(*msg)
		*part = partbyid(*msg, qpartid(q));
	return qtype(q);
}

static struct {
	int type;
	char *name;
} typenames[] = {
	Qbody,			"body",
	Qbcc,			"bcc",
	Qcc,				"cc",
	Qdate,			"date",
	Qfilename,		"filename",
	Qflags,			"flags",
	Qfrom,			"from",
	Qheader,			"header",
	Qinfo,			"info",
	Qinreplyto,		"inreplyto",
	Qlines,			"lines",
	Qmimeheader,	"mimeheader",
	Qmessageid,		"messageid",
	Qraw,			"raw",
	Qrawunix,		"rawunix",
	Qrawbody,		"rawbody",
	Qrawheader,		"rawheader",
	Qreplyto,			"replyto",
	Qsender,			"sender",
	Qsubject,		"subject",
	Qto,				"to",
	Qtype,			"type",
	Qunixdate,		"unixdate",
	Qunixheader,		"unixheader",
	Qidstr,			"idstr",
	Qdesc,			"desc",
	Qencoding,		"encoding",
	Qcharset,		"charset"
};

char*
nameoftype(int t)
{
	int i;

	for(i=0; i<nelem(typenames); i++)
		if(typenames[i].type == t)
			return typenames[i].name;
	return "???";
}

int
typeofname(char *name)
{
	int i;

	for(i=0; i<nelem(typenames); i++)
		if(strcmp(typenames[i].name, name) == 0)
			return typenames[i].type;
	return 0;
}

static void
fsattach(Req *r)
{
	r->fid->qid = rootqid;
	r->ofcall.qid = rootqid;
	respond(r, nil);
}

static int
isnumber(char *s)
{
	int n;

	if(*s < '1' || *s > '9')
		return 0;
	n = strtol(s, &s, 10);
	if(*s != 0)
		return 0;
	return n;
}

static char*
fswalk1(Fid *fid, char *name, void *arg)
{
	int a, type;
	Box *b, *box;
	Msg *msg;
	Part *p, *part;

	USED(arg);

	switch(type = parseqid(fid->qid, &box, &msg, &part)){
	case Qroot:
		if(strcmp(name, "..") == 0)
			return nil;
		if(strcmp(name, "ctl") == 0){
			fid->qid = qid(Qctl, nil, nil, nil);
			return nil;
		}
		if((box = boxbyname(name)) != nil){
			fid->qid = qid(Qbox, box, nil, nil);
			return nil;
		}
		break;

	case Qbox:
		/*
		 * Would be nice if .. could work even if the box is gone,
		 * but we don't know how deep the directory was.
		 */
		if(box == nil)
			return Eboxgone;
		if(strcmp(name, "..") == 0){
			if((box = box->parent) == nil){
				fid->qid = rootqid;
				return nil;
			}
			fid->qid = qid(Qbox, box, nil, nil);
			return nil;
		}
		if(strcmp(name, "ctl") == 0){
			fid->qid = qid(Qboxctl, box, nil, nil);
			return nil;
		}
		if(strcmp(name, "search") == 0){
			fid->qid = qid(Qsearch, box, nil, nil);
			return nil;
		}
		if((b = subbox(box, name)) != nil){
			fid->qid = qid(Qbox, b, nil, nil);
			return nil;
		}
		if((a = isnumber(name)) != 0){
			if((msg = msgbyid(box, a)) == nil){
				return Enomsg;
			}
			fid->qid = qid(Qmsg, box, msg, nil);
			return nil;
		}
		break;

	case Qmsg:
		if(strcmp(name, "..") == 0){
			if(part == msg->part[0]){
				fid->qid = qid(Qbox, box, nil, nil);
				return nil;
			}
			fid->qid = qid(Qmsg, box, msg, part->parent);
			return nil;
		}
		if((type = typeofname(name)) > 0){
			/* XXX - should check that type makes sense (see msggen) */
			fid->qid = qid(type, box, msg, part);
			return nil;
		}
		if((a = isnumber(name)) != 0){
			if((p = subpart(part, a-1)) != nil){
				fid->qid = qid(Qmsg, box, msg, p);
				return nil;
			}
		}
		break;
	}
	return "not found";
}

static void
fswalk(Req *r)
{
	walkandclone(r, fswalk1, nil, nil);
}

static struct {
	int flag;
	char *name;
} flagtab[] = {
	FlagJunk,			"junk",
	FlagNonJunk,		"notjunk",
	FlagReplied,	"replied",
	FlagFlagged,		"flagged",
/*	FlagDeleted,		"deleted", */
	FlagDraft,		"draft",
	FlagSeen,			"seen"
};

static void
addaddrs(Fmt *fmt, char *prefix, char *addrs)
{
	char **f;
	int i, nf, inquote;
	char *p, *sep;

	if(addrs == nil)
		return;
	addrs = estrdup(addrs);
	nf = 0;
	inquote = 0;
	for(p=addrs; *p; p++){
		if(*p == ' ' && !inquote)
			nf++;
		if(*p == '\'')
			inquote = !inquote;
	}
	nf += 10;
	f = emalloc(nf*sizeof f[0]);
	nf = tokenize(addrs, f, nf);
	fmtprint(fmt, "%s:", prefix);
	sep = " ";
	for(i=0; i+1<nf; i+=2){
		if(f[i][0])
			fmtprint(fmt, "%s%s <%s>", sep, f[i], f[i+1]);
		else
			fmtprint(fmt, "%s%s", sep, f[i+1]);
		sep = ", ";
	}
	fmtprint(fmt, "\n");
	free(addrs);
}

static void
mkbody(Part *p, Qid q)
{
	char *t;
	int len;

	USED(q);
	if(p->msg->part[0] == p)
		t = p->rawbody;
	else
		t = p->raw;
	if(t == nil)
		return;

	len = -1;
	if(p->encoding && cistrcmp(p->encoding, "quoted-printable") == 0)
		t = decode(QuotedPrintable, t, &len);
	else if(p->encoding && cistrcmp(p->encoding, "base64") == 0)
		t = decode(Base64, t, &len);
	else
		t = estrdup(t);

	if(p->charset){
		t = tcs(p->charset, t);
		len = -1;
	}
	p->body = t;
	if(len == -1)
		p->nbody = strlen(t);
	else
		p->nbody = len;
}

static Qid ZQ;

static int
filedata(int type, Box *box, Msg *msg, Part *part, char **pp, int *len, int *freeme, int force, Qid q)
{
	int i, inquote, n, t;
	char *from, *s;
	static char buf[256];
	Fmt fmt;

	*pp = nil;
	*freeme = 0;
	if(len)
		*len = -1;

	if(msg == nil || part == nil){
		werrstr(Emsggone);
		return -1;
	}
	switch(type){
	case Qdate:
	case Qsubject:
	case Qfrom:
	case Qsender:
	case Qreplyto:
	case Qto:
	case Qcc:
	case Qbcc:
	case Qinreplyto:
	case Qmessageid:
		if(part->hdr == nil){
			werrstr(Emsggone);
			return -1;
		}
		*pp = ((char**)&part->hdr->date)[type-Qdate];
		return 0;

	case Qunixdate:
		strcpy(buf, ctime(msg->date));
		*pp = buf;
		return 0;

	case Qunixheader:
		if(part->hdr == nil){
			werrstr(Emsggone);
			return -1;
		}
		from = part->hdr->from;
		if(from == nil)
			from = "???";
		else{
			inquote = 0;
			for(; *from; from++){
				if(*from == '\'')
					inquote = !inquote;
				if(!inquote && *from == ' '){
					from++;
					break;
				}
			}
			if(*from == 0)
				from = part->hdr->from;
		}
		n = snprint(buf, sizeof buf, "From %s %s", from, ctime(msg->date));
		if(n+1 < sizeof buf){
			*pp = buf;
			return 0;
		}
		fmtstrinit(&fmt);
		fmtprint(&fmt, "From %s %s", from, ctime(msg->date));
		s = fmtstrflush(&fmt);
		if(s){
			*pp = s;
			*freeme = 1;
		}else
			*pp = buf;
		return 0;

	case Qtype:
	case Qidstr:
	case Qdesc:
	case Qencoding:
	case Qcharset:
	case Qfilename:
	case Qraw:
	case Qrawheader:
	case Qrawbody:
	case Qmimeheader:
		*pp = ((char**)&part->type)[type-Qtype];
		if(*pp == nil && force){
			switch(type){
			case Qraw:
				imapfetchraw(imap, part);
				break;
			case Qrawheader:
				imapfetchrawheader(imap, part);
				break;
			case Qrawbody:
				imapfetchrawbody(imap, part);
				break;
			case Qmimeheader:
				imapfetchrawmime(imap, part);
				break;
			default:
				return 0;
			}
			/*
			 * We ran fetchsomething, which might have changed
			 * the mailbox contents.  Msg might even be gone.
			 */
			t = parseqid(q, &box, &msg, &part);
			if(t != type || msg == nil || part == nil)
				return 0;
			*pp = ((char**)&part->type)[type-Qtype];
		}
		return 0;

	case Qbody:
		if(part->body){
			*pp = part->body;
			if(len)
				*len = part->nbody;
			return 0;
		}
		if(!force)
			return 0;
		if(part->rawbody == nil){
			if(part->msg->part[0] == part)
				imapfetchrawbody(imap, part);
			else
				imapfetchraw(imap, part);
			t = parseqid(q, &box, &msg, &part);
			if(t != type || msg == nil || part == nil)
				return 0;
		}
		mkbody(part, q);
		*pp = part->body;
		if(len)
			*len = part->nbody;
		return 0;

	case Qsize:
	case Qlines:
		n = ((uint*)&part->size)[type-Qsize];
		snprint(buf, sizeof buf, "%d", n);
		*pp = buf;
		return 0;

	case Qflags:
		s = buf;
		*s = 0;
		for(i=0; i<nelem(flagtab); i++){
			if(msg->flags&flagtab[i].flag){
				if(s > buf)
					*s++ = ' ';
				strcpy(s, flagtab[i].name);
				s += strlen(s);
			}
		}
		*pp = buf;
		return 0;

	case Qinfo:
		fmtstrinit(&fmt);
		if(part == msg->part[0]){
			if(msg->date)
				fmtprint(&fmt, "unixdate %ud %s", msg->date, ctime(msg->date));
			if(msg->flags){
				filedata(Qflags, box, msg, part, pp, nil, freeme, 0, ZQ);
				fmtprint(&fmt, "flags %s\n", buf);
			}
		}
		if(part->hdr){
			if(part->hdr->digest)
				fmtprint(&fmt, "digest %s\n", part->hdr->digest);
			if(part->hdr->from)
				fmtprint(&fmt, "from %s\n", part->hdr->from);
			if(part->hdr->to)
				fmtprint(&fmt, "to %s\n", part->hdr->to);
			if(part->hdr->cc)
				fmtprint(&fmt, "cc %s\n", part->hdr->cc);
			if(part->hdr->replyto)
				fmtprint(&fmt, "replyto %s\n", part->hdr->replyto);
			if(part->hdr->bcc)
				fmtprint(&fmt, "bcc %s\n", part->hdr->bcc);
			if(part->hdr->inreplyto)
				fmtprint(&fmt, "inreplyto %s\n", part->hdr->inreplyto);
			if(part->hdr->date)
				fmtprint(&fmt, "date %s\n", part->hdr->date);
			if(part->hdr->sender)
				fmtprint(&fmt, "sender %s\n", part->hdr->sender);
			if(part->hdr->messageid)
				fmtprint(&fmt, "messageid %s\n", part->hdr->messageid);
			if(part->hdr->subject)
				fmtprint(&fmt, "subject %s\n", part->hdr->subject);
		}
		if(part->type)
			fmtprint(&fmt, "type %s\n", part->type);
		if(part->lines)
			fmtprint(&fmt, "lines %d\n", part->lines);
		if(part->filename)
			fmtprint(&fmt, "filename %s\n", part->filename);
		s = fmtstrflush(&fmt);
		if(s == nil)
			s = estrdup("");
		*freeme = 1;
		*pp = s;
		return 0;

	case Qheader:
		if(part->hdr == nil)
			return 0;
		fmtstrinit(&fmt);
		if(part == msg->part[0])
			fmtprint(&fmt, "Date: %s", ctime(msg->date));
		else
			fmtprint(&fmt, "Date: %s\n", part->hdr->date);
		addaddrs(&fmt, "To", part->hdr->to);
		addaddrs(&fmt, "From", part->hdr->from);
		if(part->hdr->from==nil
		|| (part->hdr->sender && strcmp(part->hdr->sender, part->hdr->from) != 0))
			addaddrs(&fmt, "Sender", part->hdr->sender);
		if(part->hdr->from==nil
		|| (part->hdr->replyto && strcmp(part->hdr->replyto, part->hdr->from) != 0))
			addaddrs(&fmt, "Reply-To", part->hdr->replyto);
		fmtprint(&fmt, "Subject: %s\n", part->hdr->subject);
		s = fmtstrflush(&fmt);
		if(s == nil)
			s = estrdup("");
		*freeme = 1;
		*pp = s;
		return 0;

	default:
		werrstr(Egreg);
		return -1;
	}
}

int
filldir(Dir *d, int type, Box *box, Msg *msg, Part *part)
{
	int freeme, len;
	char *s;

	memset(d, 0, sizeof *d);
	if(box){
		d->atime = box->time;
		d->mtime = box->time;
	}else{
		d->atime = t0;
		d->mtime = t0;
	}
	d->uid = estrdup9p("upas");
	d->gid = estrdup9p("upas");
	d->muid = estrdup9p("upas");
	d->qid = qid(type, box, msg, part);

	switch(type){
	case Qroot:
	case Qbox:
	case Qmsg:
		d->mode = 0555|DMDIR;
		if(box && !(box->flags&FlagNoInferiors))
			d->mode = 0775|DMDIR;
		break;
	case Qctl:
	case Qboxctl:
		d->mode = 0222;
		break;
	case Qsearch:
		d->mode = 0666;
		break;

	case Qflags:
		d->mode = 0666;
		goto msgfile;
	default:
		d->mode = 0444;
	msgfile:
		if(filedata(type, box, msg, part, &s, &len, &freeme, 0, ZQ) >= 0){
			if(s){
				if(len == -1)
					d->length = strlen(s);
				else
					d->length = len;
				if(freeme)
					free(s);
			}
		}else if(type == Qraw && msg && part == msg->part[0])
			d->length = msg->size;
		break;
	}

	switch(type){
	case Qroot:
		d->name = estrdup9p("/");
		break;
	case Qbox:
		if(box == nil){
			werrstr(Enobox);
			return -1;
		}
		d->name = estrdup9p(box->elem);
		break;
	case Qmsg:
		if(msg == nil){
			werrstr(Enomsg);
			return -1;
		}
		if(part == nil || part == msg->part[0])
			d->name = esmprint("%d", msg->id);
		else
			d->name = esmprint("%d", part->pix+1);
		break;
	case Qctl:
	case Qboxctl:
		d->name = estrdup9p("ctl");
		break;
	case Qsearch:
		d->name = estrdup9p("search");
		break;
	default:
		d->name = estrdup9p(nameoftype(type));
		break;
	}
	return 0;
}

static void
fsstat(Req *r)
{
	int type;
	Box *box;
	Msg *msg;
	Part *part;

	type = parseqid(r->fid->qid, &box, &msg, &part);
	if(filldir(&r->d, type, box, msg, part) < 0)
		responderror(r);
	else
		respond(r, nil);
}

int
rootgen(int i, Dir *d, void *aux)
{
	USED(aux);

	if(i == 0)
		return filldir(d, Qctl, nil, nil, nil);
	i--;
	if(i < rootbox->nsub)
		return filldir(d, Qbox, rootbox->sub[i], nil, nil);
	return -1;
}

int
boxgen(int i, Dir *d, void *aux)
{
	Box *box;

	box = aux;
	if(i == 0)
		return filldir(d, Qboxctl, box, nil, nil);
	i--;
	if(i == 0)
		return filldir(d, Qsearch, box, nil, nil);
	i--;
	if(i < box->nsub)
		return filldir(d, Qbox, box->sub[i], nil, nil);
	i -= box->nsub;
	if(i < box->nmsg)
		return filldir(d, Qmsg, box, box->msg[i], nil);
	return -1;
}

static int msgdir[] = {
	Qtype,
	Qbody, Qbcc, Qcc, Qdate, Qflags, Qfrom, Qheader, Qinfo,
	Qinreplyto, Qlines, Qmimeheader, Qmessageid,
	Qraw, Qrawunix, Qrawbody, Qrawheader,
	Qreplyto, Qsender, Qsubject, Qto,
	Qunixdate, Qunixheader
};
static int mimemsgdir[] = {
	Qtype,
	Qbody, Qbcc, Qcc, Qdate, Qfrom, Qheader, Qinfo,
	Qinreplyto, Qlines, Qmimeheader, Qmessageid,
	Qraw, Qrawunix, Qrawbody, Qrawheader,
	Qreplyto, Qsender, Qsubject, Qto
};
static int mimedir[] = {
	Qtype,
	Qbody,
	Qfilename,
	Qcharset,
	Qmimeheader,
	Qraw
};

int
msggen(int i, Dir *d, void *aux)
{
	Box *box;
	Msg *msg;
	Part *part;

	part = aux;
	msg = part->msg;
	box = msg->box;
	if(part->ix == 0){
		if(i < nelem(msgdir))
			return filldir(d, msgdir[i], box, msg, part);
		i -= nelem(msgdir);
	}else if(part->type && strcmp(part->type, "message/rfc822") == 0){
		if(i < nelem(mimemsgdir))
			return filldir(d, mimemsgdir[i], box, msg, part);
		i -= nelem(mimemsgdir);
	}else{
		if(i < nelem(mimedir))
			return filldir(d, mimedir[i], box, msg, part);
		i -= nelem(mimedir);
	}
	if(i < part->nsub)
		return filldir(d, Qmsg, box, msg, part->sub[i]);
	return -1;
}

enum
{
	CMhangup
};
static Cmdtab ctltab[] =
{
	CMhangup, "hangup", 2
};

enum
{
	CMdelete,
	CMrefresh,
	CMreplied,
	CMread,
	CMsave,
	CMjunk,
	CMnonjunk
};
static Cmdtab boxctltab[] =
{
	CMdelete,	"delete",	0,
	CMrefresh,	"refresh", 1,
	CMreplied,	"replied", 0,
	CMread,		"read", 0,
	CMsave,		"save", 0,
	CMjunk,		"junk", 0,
	CMnonjunk,	"nonjunk", 0
};

static void
fsread(Req *r)
{
	char *s;
	int type, len, freeme;
	Box *box;
	Msg *msg;
	Part *part;

	switch(type = parseqid(r->fid->qid, &box, &msg, &part)){
	case Qroot:
		dirread9p(r, rootgen, nil);
		respond(r, nil);
		return;

	case Qbox:
		if(box == nil){
			respond(r, Eboxgone);
			return;
		}
		if(box->nmsg == 0)
			imapcheckbox(imap, box);
		parseqid(r->fid->qid, &box, &msg, &part);
		if(box == nil){
			respond(r, Eboxgone);
			return;
		}
		dirread9p(r, boxgen, box);
		respond(r, nil);
		return;

	case Qmsg:
		if(msg == nil || part == nil){
			respond(r, Emsggone);
			return;
		}
		dirread9p(r, msggen, part);
		respond(r, nil);
		return;

	case Qctl:
	case Qboxctl:
		respond(r, Egreg);
		return;

	case Qsearch:
		readstr(r, r->fid->aux);
		respond(r, nil);
		return;

	default:
		if(filedata(type, box, msg, part, &s, &len, &freeme, 1, r->fid->qid) < 0){
			responderror(r);
			return;
		}
		if(s && len == -1)
			len = strlen(s);
		readbuf(r, s, len);
		if(freeme)
			free(s);
		respond(r, nil);
		return;
	}
}

int
mkmsglist(Box *box, char **f, int nf, Msg ***mm)
{
	int i, nm;
	Msg **m;

	m = emalloc(nf*sizeof m[0]);
	nm = 0;
	for(i=0; i<nf; i++)
		if((m[nm] = msgbyid(box, atoi(f[i]))) != nil)
			nm++;
	*mm = m;
	return nm;
}

static void
fswrite(Req *r)
{
	int i, j, c, type, flag, unflag, flagset, f;
	Box *box;
	Msg *msg;
	Part *part;
	Cmdbuf *cb;
	Cmdtab *ct;
	Msg **m;
	int nm;
	Fmt fmt;

	r->ofcall.count = r->ifcall.count;
	switch(type = parseqid(r->fid->qid, &box, &msg, &part)){
	default:
		respond(r, Egreg);
		break;

	case Qctl:
		cb = parsecmd(r->ifcall.data, r->ifcall.count);
		if((ct = lookupcmd(cb, ctltab, nelem(ctltab))) == nil){
			respondcmderror(r, cb, "unknown message");
			free(cb);
			return;
		}
		r->ofcall.count = r->ifcall.count;
		switch(ct->index){
		case CMhangup:
			imaphangup(imap, atoi(cb->f[1]));
			respond(r, nil);
			break;
		default:
			respond(r, Egreg);
			break;
		}
		free(cb);
		return;

	case Qboxctl:
		cb = parsecmd(r->ifcall.data, r->ifcall.count);
		if((ct = lookupcmd(cb, boxctltab, nelem(boxctltab))) == nil){
			respondcmderror(r, cb, "bad message");
			free(cb);
			return;
		}
		r->ofcall.count = r->ifcall.count;
		switch(ct->index){
		case CMsave:
			if(cb->nf <= 2){
				respondcmderror(r, cb, Ebadctl);
				break;
			}
			nm = mkmsglist(box, cb->f+2, cb->nf-2, &m);
			if(nm != cb->nf-2){
			/*	free(m); */
				respond(r, Enomsg);
				break;
			}
			if(nm > 0 && imapcopylist(imap, cb->f[1], m, nm) < 0)
				responderror(r);
			else
				respond(r, nil);
			free(m);
			break;

		case CMjunk:
			flag = FlagJunk;
			goto flagit;
		case CMnonjunk:
			flag = FlagNonJunk;
			goto flagit;
		case CMreplied:
			flag = FlagReplied;
			goto flagit;
		case CMread:
			flag = FlagSeen;
		flagit:
			if(cb->nf <= 1){
				respondcmderror(r, cb, Ebadctl);
				break;
			}
			nm = mkmsglist(box, cb->f+1, cb->nf-1, &m);
			if(nm != cb->nf-1){
				free(m);
				respond(r, Enomsg);
				break;
			}
			if(nm > 0 && imapflaglist(imap, +1, flag, m, nm) < 0)
				responderror(r);
			else
				respond(r, nil);
			free(m);
			break;

		case CMrefresh:
			imapcheckbox(imap, box);
			respond(r, nil);
			break;

		case CMdelete:
			if(cb->nf <= 1){
				respondcmderror(r, cb, Ebadctl);
				break;
			}
			nm = mkmsglist(box, cb->f+1, cb->nf-1, &m);
			if(nm > 0 && imapremovelist(imap, m, nm) < 0)
				responderror(r);
			else
				respond(r, nil);
			free(m);
			break;

		default:
			respond(r, Egreg);
			break;
		}
		free(cb);
		return;

	case Qflags:
		if(msg == nil){
			respond(r, Enomsg);
			return;
		}
		cb = parsecmd(r->ifcall.data, r->ifcall.count);
		flag = 0;
		unflag = 0;
		flagset = 0;
		for(i=0; i<cb->nf; i++){
			f = 0;
			c = cb->f[i][0];
			if(c == '+' || c == '-')
				cb->f[i]++;
			for(j=0; j<nelem(flagtab); j++){
				if(strcmp(flagtab[j].name, cb->f[i]) == 0){
					f = flagtab[j].flag;
					break;
				}
			}
			if(f == 0){
				respondcmderror(r, cb, "unknown flag %s", cb->f[i]);
				free(cb);
				return;
			}
			if(c == '+')
				flag |= f;
			else if(c == '-')
				unflag |= f;
			else
				flagset |= f;
		}
		free(cb);
		if((flagset!=0)+(unflag!=0)+(flag!=0) != 1){
			respondcmderror(r, cb, Ebadctl);
			return;
		}
		if(flag)
			i = 1;
		else if(unflag){
			i = -1;
			flag = unflag;
		}else{
			i = 0;
			flag = flagset;
		}
		if(imapflaglist(imap, i, flag, &msg, 1) < 0)
			responderror(r);
		else
			respond(r, nil);
		return;

	case Qsearch:
		if(box == nil){
			respond(r, Eboxgone);
			return;
		}
		fmtstrinit(&fmt);
		nm = imapsearchbox(imap, box, r->ifcall.data, &m);
		for(i=0; i<nm; i++){
			if(i>0)
				fmtrune(&fmt, ' ');
			fmtprint(&fmt, "%d", m[i]->id);
		}
		free(r->fid->aux);
		r->fid->aux = fmtstrflush(&fmt);
		respond(r, nil);
		return;
	}
}

static void
fsopen(Req *r)
{
	switch(qtype(r->fid->qid)){
	case Qctl:
	case Qboxctl:
		if((r->ifcall.mode&~OTRUNC) != OWRITE){
			respond(r, Eperm);
			return;
		}
		respond(r, nil);
		return;

	case Qflags:
	case Qsearch:
		if((r->ifcall.mode&~OTRUNC) > ORDWR){
			respond(r, Eperm);
			return;
		}
		respond(r, nil);
		return;

	default:
		if(r->ifcall.mode != OREAD){
			respond(r, Eperm);
			return;
		}
		respond(r, nil);
		return;
	}
}

static void
fsflush(Req *r)
{
	/*
	 * We only handle reads and writes outside the main loop,
	 * so we must be flushing one of those.  In both cases it's
	 * okay to just ignore the results of the request, whenever
	 * they're ready.
	 */
	incref(&r->oldreq->ref);
	respond(r->oldreq, "interrupted");
	respond(r, nil);
}

static void
fsthread(void *v)
{
	Req *r;

	r = v;
	switch(r->ifcall.type){
	case Tread:
		fsread(r);
		break;
	case Twrite:
		fswrite(r);
		break;
	}
}

static void
fsrecv(void *v)
{
	Req *r;

	USED(v);
	while((r = recvp(fsreqchan)) != nil){
		switch(r->ifcall.type){
		case Tattach:
			fsattach(r);
			break;
		case Tflush:
			fsflush(r);
			break;
		case Topen:
			fsopen(r);
			break;
		case Twalk:
			fswalk(r);
			break;
		case Tstat:
			fsstat(r);
			break;
		default:
			threadcreate(fsthread, r, STACK);
			break;
		}
	}
}

static void
fssend(Req *r)
{
	sendp(fsreqchan, r);
}

static void
fsdestroyfid(Fid *f)
{
	free(f->aux);
}

void
fsinit0(void)	/* bad planning - clash with lib9pclient */
{
	t0 = time(0);

	fs.attach = fssend;
	fs.flush = fssend;
	fs.open = fssend;
	fs.walk = fssend;
	fs.read = fssend;
	fs.write = fssend;
	fs.stat = fssend;
	fs.destroyfid = fsdestroyfid;

	rootqid = qid(Qroot, nil, nil, nil);

	fsreqchan = chancreate(sizeof(void*), 0);
	mailthread(fsrecv, nil);
}
