#include "a.h"

enum
{
	BoxSubChunk = 16,
	BoxChunk = 64,
	MsgChunk = 256,
	PartChunk = 4,
	PartSubChunk = 4
};

Box **boxes;
uint nboxes;
Box *rootbox;
int boxid;

Box*
boxbyname(char *name)
{
	int i;

	/* LATER: replace with hash table */
	for(i=0; i<nboxes; i++)
		if(boxes[i] && strcmp(boxes[i]->name, name) == 0)
			return boxes[i];
	return nil;
}

Box*
subbox(Box *b, char *elem)
{
	int i;

	for(i=0; i<b->nsub; i++)
		if(b->sub[i] && strcmp(b->sub[i]->elem, elem) == 0)
			return b->sub[i];
	return nil;
}

Box*
boxbyid(uint id)
{
	int i;

	/* LATER: replace with binary search */
	for(i=0; i<nboxes; i++)
		if(boxes[i] && boxes[i]->id == id)
			return boxes[i];
	return nil;
}

Box*
boxcreate(char *name)
{
	char *p;
	Box *b, *bb;

	if((b = boxbyname(name)) != nil)
		return b;

	b = emalloc(sizeof *b);
	b->id = ++boxid;
	b->time = time(0);
	b->name = estrdup(name);
	b->uidnext = 1;
	p = strrchr(b->name, '/');
	if(p){
		*p = 0;
		bb = boxcreate(b->name);
		*p = '/';
		b->elem = p+1;
	}else{
		bb = rootbox;
		b->elem = b->name;
	}
	if(nboxes%BoxChunk == 0)
		boxes = erealloc(boxes, (nboxes+BoxChunk)*sizeof boxes[0]);
	boxes[nboxes++] = b;
	if(bb->nsub%BoxSubChunk == 0)
		bb->sub = erealloc(bb->sub, (bb->nsub+BoxSubChunk)*sizeof bb->sub[0]);
	bb->sub[bb->nsub++] = b;
	b->parent = bb;
	return b;
}

void
boxfree(Box *b)
{
	int i;

	if(b == nil)
		return;
	for(i=0; i<b->nmsg; i++)
		msgfree(b->msg[i]);
	free(b->msg);
	free(b);
}

Part*
partcreate(Msg *m, Part *pp)
{
	Part *p;

	if(m->npart%PartChunk == 0)
		m->part = erealloc(m->part, (m->npart+PartChunk)*sizeof m->part[0]);
	p = emalloc(sizeof *p);
	p->msg = m;
	p->ix = m->npart;
	m->part[m->npart++] = p;
	if(pp){
		if(pp->nsub%PartSubChunk == 0)
			pp->sub = erealloc(pp->sub, (pp->nsub+PartSubChunk)*sizeof pp->sub[0]);
		p->pix = pp->nsub;
		p->parent = pp;
		pp->sub[pp->nsub++] = p;
	}
	return p;
}

void
partfree(Part *p)
{
	int i;

	if(p == nil)
		return;
	for(i=0; i<p->nsub; i++)
		partfree(p->sub[i]);
	free(p->sub);
	hdrfree(p->hdr);
	free(p->type);
	free(p->idstr);
	free(p->desc);
	free(p->encoding);
	free(p->charset);
	free(p->raw);
	free(p->rawheader);
	free(p->rawbody);
	free(p->mimeheader);
	free(p->body);
	free(p);
}

void
msgfree(Msg *m)
{
	int i;

	if(m == nil)
		return;
	for(i=0; i<m->npart; i++)
		free(m->part[i]);
	free(m->part);
	free(m);
}

void
msgplumb(Msg *m, int delete)
{
	static int fd = -1;
	Plumbmsg p;
	Plumbattr a[10];
	char buf[256], date[40];
	int ai;

	if(m == nil || m->npart < 1 || m->part[0]->hdr == nil)
		return;
	if(m->box && strcmp(m->box->name, "mbox") != 0)
		return;

	p.src = "mailfs";
	p.dst = "seemail";
	p.wdir = "/";
	p.type = "text";

	ai = 0;
	a[ai].name = "filetype";
	a[ai].value = "mail";

	a[++ai].name = "mailtype";
	a[ai].value = delete?"delete":"new";
	a[ai-1].next = &a[ai];

	if(m->part[0]->hdr->from){
		a[++ai].name = "sender";
		a[ai].value = m->part[0]->hdr->from;
		a[ai-1].next = &a[ai];
	}

	if(m->part[0]->hdr->subject){
		a[++ai].name = "subject";
		a[ai].value = m->part[0]->hdr->subject;
		a[ai-1].next = &a[ai];
	}

	if(m->part[0]->hdr->digest){
		a[++ai].name = "digest";
		a[ai].value = m->part[0]->hdr->digest;
		a[ai-1].next = &a[ai];
	}

	strcpy(date, ctime(m->date));
	date[strlen(date)-1] = 0;	/* newline */
	a[++ai].name = "date";
	a[ai].value = date;
	a[ai-1].next = &a[ai];

	a[ai].next = nil;

	p.attr = a;
#ifdef PLAN9PORT
	snprint(buf, sizeof buf, "Mail/%s/%ud", m->box->name, m->id);
#else
	snprint(buf, sizeof buf, "/mail/fs/%s/%ud", m->box->name, m->id);
#endif
	p.ndata = strlen(buf);
	p.data = buf;

	if(fd < 0)
		fd = plumbopen("send", OWRITE);
	if(fd < 0)
		return;

	plumbsend(fd, &p);
}


Msg*
msgcreate(Box *box)
{
	Msg *m;

	m = emalloc(sizeof *m);
	m->box = box;
	partcreate(m, nil);
	m->part[0]->type = estrdup("message/rfc822");
	if(box->nmsg%MsgChunk == 0)
		box->msg = erealloc(box->msg, (box->nmsg+MsgChunk)*sizeof box->msg[0]);
	m->ix = box->nmsg++;
	box->msg[m->ix] = m;
	m->id = ++box->msgid;
	return m;
}

Msg*
msgbyimapuid(Box *box, uint uid, int docreate)
{
	int i;
	Msg *msg;

	if(box == nil)
		return nil;
	/* LATER: binary search or something */
	for(i=0; i<box->nmsg; i++)
		if(box->msg[i]->imapuid == uid)
			return box->msg[i];
	if(!docreate)
		return nil;
	msg = msgcreate(box);
	msg->imapuid = uid;
	return msg;
}

Msg*
msgbyid(Box *box, uint id)
{
	int i;

	if(box == nil)
		return nil;
	/* LATER: binary search or something */
	for(i=0; i<box->nmsg; i++)
		if(box->msg[i]->id == id)
			return box->msg[i];
	return nil;
}

Part*
partbyid(Msg *m, uint id)
{
	if(m == nil)
		return nil;
	if(id >= m->npart)
		return nil;
	return m->part[id];
}

Part*
subpart(Part *p, uint a)
{
	if(p == nil || a >= p->nsub)
		return nil;
	return p->sub[a];
}

void
hdrfree(Hdr *h)
{
	if(h == nil)
		return;
	free(h->date);
	free(h->subject);
	free(h->from);
	free(h->sender);
	free(h->replyto);
	free(h->to);
	free(h->cc);
	free(h->bcc);
	free(h->inreplyto);
	free(h->messageid);
	free(h->digest);
	free(h);
}

void
boxinit(void)
{
	rootbox = emalloc(sizeof *rootbox);
	rootbox->name = estrdup("");
	rootbox->time = time(0);
}
