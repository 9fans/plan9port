#include "common.h"
#include <ctype.h>
#include <plumb.h>
#include <libsec.h>
#include "dat.h"

enum {
	Buffersize = 64*1024
};

typedef struct Inbuf Inbuf;
struct Inbuf
{
	int	fd;
	uchar	*lim;
	uchar	*rptr;
	uchar	*wptr;
	uchar	data[Buffersize+7];
};

static void
addtomessage(Message *m, uchar *p, int n, int done)
{
	int i, len;

	/* add to message (+ 1 in malloc is for a trailing null) */
	if(m->lim - m->end < n){
		if(m->start != nil){
			i = m->end-m->start;
			if(done)
				len = i + n;
			else
				len = (4*(i+n))/3;
			m->start = erealloc(m->start, len + 1);
			m->end = m->start + i;
		} else {
			if(done)
				len = n;
			else
				len = 2*n;
			m->start = emalloc(len + 1);
			m->end = m->start;
		}
		m->lim = m->start + len;
	}

	memmove(m->end, p, n);
	m->end += n;
}

/* */
/*  read in a single message */
/* */
static int
readmessage(Message *m, Inbuf *inb)
{
	int i, n, done;
	uchar *p, *np;
	char sdigest[SHA1dlen*2+1];
	char tmp[64];

	for(done = 0; !done;){
		n = inb->wptr - inb->rptr;
		if(n < 6){
			if(n)
				memmove(inb->data, inb->rptr, n);
			inb->rptr = inb->data;
			inb->wptr = inb->rptr + n;
			i = read(inb->fd, inb->wptr, Buffersize);
			if(i < 0){
				/* if(fd2path(inb->fd, tmp, sizeof tmp) < 0)
					strcpy(tmp, "unknown mailbox");  jpc */
				fprint(2, "error reading '%s': %r\n", tmp);
				return -1;
			}
			if(i == 0){
				if(n != 0)
					addtomessage(m, inb->rptr, n, 1);
				if(m->end == m->start)
					return -1;
				break;
			}
			inb->wptr += i;
		}

		/* look for end of message */
		for(p = inb->rptr; p < inb->wptr; p = np+1){
			/* first part of search for '\nFrom ' */
			np = memchr(p, '\n', inb->wptr - p);
			if(np == nil){
				p = inb->wptr;
				break;
			}

			/*
			 *  if we've found a \n but there's
			 *  not enough room for '\nFrom ', don't do
			 *  the comparison till we've read in more.
			 */
			if(inb->wptr - np < 6){
				p = np;
				break;
			}

			if(strncmp((char*)np, "\nFrom ", 6) == 0){
				done = 1;
				p = np+1;
				break;
			}
		}

		/* add to message (+ 1 in malloc is for a trailing null) */
		n = p - inb->rptr;
		addtomessage(m, inb->rptr, n, done);
		inb->rptr += n;
	}

	/* if it doesn't start with a 'From ', this ain't a mailbox */
	if(strncmp(m->start, "From ", 5) != 0)
		return -1;

	/* dump trailing newline, make sure there's a trailing null */
	/* (helps in body searches) */
	if(*(m->end-1) == '\n')
		m->end--;
	*m->end = 0;
	m->bend = m->rbend = m->end;

	/* digest message */
	sha1((uchar*)m->start, m->end - m->start, m->digest, nil);
	for(i = 0; i < SHA1dlen; i++)
		sprint(sdigest+2*i, "%2.2ux", m->digest[i]);
	m->sdigest = s_copy(sdigest);

	return 0;
}


/* throw out deleted messages.  return number of freshly deleted messages */
int
purgedeleted(Mailbox *mb)
{
	Message *m, *next;
	int newdels;

	/* forget about what's no longer in the mailbox */
	newdels = 0;
	for(m = mb->root->part; m != nil; m = next){
		next = m->next;
		if(m->deleted && m->refs == 0){
			if(m->inmbox)
				newdels++;
			delmessage(mb, m);
		}
	}
	return newdels;
}

/* */
/*  read in the mailbox and parse into messages. */
/* */
static char*
_readmbox(Mailbox *mb, int doplumb, Mlock *lk)
{
	int fd;
	String *tmp;
	Dir *d;
	static char err[128];
	Message *m, **l;
	Inbuf *inb;
	char *x;

	l = &mb->root->part;

	/*
	 *  open the mailbox.  If it doesn't exist, try the temporary one.
	 */
retry:
	fd = open(mb->path, OREAD);
	if(fd < 0){
		errstr(err, sizeof(err));
		if(strstr(err, "exist") != 0){
			tmp = s_copy(mb->path);
			s_append(tmp, ".tmp");
			if(sysrename(s_to_c(tmp), mb->path) == 0){
				s_free(tmp);
				goto retry;
			}
			s_free(tmp);
		}
		return err;
	}

	/*
	 *  a new qid.path means reread the mailbox, while
	 *  a new qid.vers means read any new messages
	 */
	d = dirfstat(fd);
	if(d == nil){
		close(fd);
		errstr(err, sizeof(err));
		return err;
	}
	if(mb->d != nil){
		if(d->qid.path == mb->d->qid.path && d->qid.vers == mb->d->qid.vers){
			close(fd);
			free(d);
			return nil;
		}
		if(d->qid.path == mb->d->qid.path){
			while(*l != nil)
				l = &(*l)->next;
			seek(fd, mb->d->length, 0);
		}
		free(mb->d);
	}
	mb->d = d;
	mb->vers++;
	henter(PATH(0, Qtop), mb->name,
		(Qid){PATH(mb->id, Qmbox), mb->vers, QTDIR}, nil, mb);

	inb = emalloc(sizeof(Inbuf));
	inb->rptr = inb->wptr = inb->data;
	inb->fd = fd;

	/*  read new messages */
	snprint(err, sizeof err, "reading '%s'", mb->path);
	logmsg(err, nil);
	for(;;){
		if(lk != nil)
			syslockrefresh(lk);
		m = newmessage(mb->root);
		m->mallocd = 1;
		m->inmbox = 1;
		if(readmessage(m, inb) < 0){
			delmessage(mb, m);
			mb->root->subname--;
			break;
		}

		/* merge mailbox versions */
		while(*l != nil){
			if(memcmp((*l)->digest, m->digest, SHA1dlen) == 0){
				/* matches mail we already read, discard */
				logmsg("duplicate", *l);
				delmessage(mb, m);
				mb->root->subname--;
				m = nil;
				l = &(*l)->next;
				break;
			} else {
				/* old mail no longer in box, mark deleted */
				logmsg("disappeared", *l);
				if(doplumb)
					mailplumb(mb, *l, 1);
				(*l)->inmbox = 0;
				(*l)->deleted = 1;
				l = &(*l)->next;
			}
		}
		if(m == nil)
			continue;

		x = strchr(m->start, '\n');
		if(x == nil)
			m->header = m->end;
		else
			m->header = x + 1;
		m->mheader = m->mhend = m->header;
		parseunix(m);
		parse(m, 0, mb, 0);
		logmsg("new", m);

		/* chain in */
		*l = m;
		l = &m->next;
		if(doplumb)
			mailplumb(mb, m, 0);

	}
	logmsg("mbox read", nil);

	/* whatever is left has been removed from the mbox, mark deleted */
	while(*l != nil){
		if(doplumb)
			mailplumb(mb, *l, 1);
		(*l)->inmbox = 0;
		(*l)->deleted = 1;
		l = &(*l)->next;
	}

	close(fd);
	free(inb);
	return nil;
}

static void
_writembox(Mailbox *mb, Mlock *lk)
{
	Dir *d;
	Message *m;
	String *tmp;
	int mode, errs;
	Biobuf *b;

	tmp = s_copy(mb->path);
	s_append(tmp, ".tmp");

	/*
	 * preserve old files permissions, if possible
	 */
	d = dirstat(mb->path);
	if(d != nil){
		mode = d->mode&0777;
		free(d);
	} else
		mode = MBOXMODE;

	sysremove(s_to_c(tmp));
	b = sysopen(s_to_c(tmp), "alc", mode);
	if(b == 0){
		fprint(2, "can't write temporary mailbox %s: %r\n", s_to_c(tmp));
		return;
	}

	logmsg("writing new mbox", nil);
	errs = 0;
	for(m = mb->root->part; m != nil; m = m->next){
		if(lk != nil)
			syslockrefresh(lk);
		if(m->deleted)
			continue;
		logmsg("writing", m);
		if(Bwrite(b, m->start, m->end - m->start) < 0)
			errs = 1;
		if(Bwrite(b, "\n", 1) < 0)
			errs = 1;
	}
	logmsg("wrote new mbox", nil);

	if(sysclose(b) < 0)
		errs = 1;

	if(errs){
		fprint(2, "error writing temporary mail file\n");
		s_free(tmp);
		return;
	}

	sysremove(mb->path);
	if(sysrename(s_to_c(tmp), mb->path) < 0)
		fprint(2, "%s: can't rename %s to %s: %r\n", argv0,
			s_to_c(tmp), mb->path);
	s_free(tmp);
	if(mb->d != nil)
		free(mb->d);
	mb->d = dirstat(mb->path);
}

char*
plan9syncmbox(Mailbox *mb, int doplumb)
{
	Mlock *lk;
	char *rv;

	lk = nil;
	if(mb->dolock){
		lk = syslock(mb->path);
		if(lk == nil)
			return "can't lock mailbox";
	}

	rv = _readmbox(mb, doplumb, lk);		/* interpolate */
	if(purgedeleted(mb) > 0)
		_writembox(mb, lk);

	if(lk != nil)
		sysunlock(lk);

	return rv;
}

/* */
/*  look to see if we can open this mail box */
/* */
char*
plan9mbox(Mailbox *mb, char *path)
{
	static char err[64];
	String *tmp;

	if(access(path, AEXIST) < 0){
		errstr(err, sizeof(err));
		tmp = s_copy(path);
		s_append(tmp, ".tmp");
		if(access(s_to_c(tmp), AEXIST) < 0){
			s_free(tmp);
			return err;
		}
		s_free(tmp);
	}

	mb->sync = plan9syncmbox;
	return nil;
}
