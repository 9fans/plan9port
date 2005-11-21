#include <u.h>
#include <libc.h>
#include <fcall.h>

static uint dumpsome(char*, char*, char*, long);
static void fdirconv(char*, char*, Dir*);
static char *qidtype(char*, uchar);

#define	QIDFMT	"(%.16llux %lud %s)"

int
fcallfmt(Fmt *fmt)
{
	Fcall *f;
	int fid, type, tag, i;
	char buf[512], tmp[200];
	char *p, *e;
	Dir *d;
	Qid *q;

	e = buf+sizeof(buf);
	f = va_arg(fmt->args, Fcall*);
	type = f->type;
	fid = f->fid;
	tag = f->tag;
	switch(type){
	case Tversion:	/* 100 */
		seprint(buf, e, "Tversion tag %ud msize %ud version '%s'", tag, f->msize, f->version);
		break;
	case Rversion:
		seprint(buf, e, "Rversion tag %ud msize %ud version '%s'", tag, f->msize, f->version);
		break;
	case Tauth:	/* 102 */
		seprint(buf, e, "Tauth tag %ud afid %d uname %s aname %s", tag,
			f->afid, f->uname, f->aname);
		break;
	case Rauth:
		seprint(buf, e, "Rauth tag %ud qid " QIDFMT, tag,
			f->aqid.path, f->aqid.vers, qidtype(tmp, f->aqid.type));
		break;
	case Tattach:	/* 104 */
		seprint(buf, e, "Tattach tag %ud fid %d afid %d uname %s aname %s", tag,
			fid, f->afid, f->uname, f->aname);
		break;
	case Rattach:
		seprint(buf, e, "Rattach tag %ud qid " QIDFMT, tag,
			f->qid.path, f->qid.vers, qidtype(tmp, f->qid.type));
		break;
	case Rerror:	/* 107; 106 (Terror) illegal */
		seprint(buf, e, "Rerror tag %ud ename %s", tag, f->ename);
		break;
	case Tflush:	/* 108 */
		seprint(buf, e, "Tflush tag %ud oldtag %ud", tag, f->oldtag);
		break;
	case Rflush:
		seprint(buf, e, "Rflush tag %ud", tag);
		break;
	case Twalk:	/* 110 */
		p = seprint(buf, e, "Twalk tag %ud fid %d newfid %d nwname %d ", tag, fid, f->newfid, f->nwname);
		if(f->nwname <= MAXWELEM)
			for(i=0; i<f->nwname; i++)
				p = seprint(p, e, "%d:%s ", i, f->wname[i]);
		break;
	case Rwalk:
		p = seprint(buf, e, "Rwalk tag %ud nwqid %ud ", tag, f->nwqid);
		if(f->nwqid <= MAXWELEM)
			for(i=0; i<f->nwqid; i++){
				q = &f->wqid[i];
				p = seprint(p, e, "%d:" QIDFMT " ", i,
					q->path, q->vers, qidtype(tmp, q->type));
			}
		break;
	case Topen:	/* 112 */
		seprint(buf, e, "Topen tag %ud fid %ud mode %d", tag, fid, f->mode);
		break;
	case Ropen:
		seprint(buf, e, "Ropen tag %ud qid " QIDFMT " iounit %ud", tag,
			f->qid.path, f->qid.vers, qidtype(tmp, f->qid.type), f->iounit);
		break;
	case Topenfd:	/* 98 */
		seprint(buf, e, "Topenfd tag %ud fid %ud mode %d", tag, fid, f->mode);
		break;
	case Ropenfd:
		seprint(buf, e, "Ropenfd tag %ud qid " QIDFMT " iounit %ud unixfd %d", tag,
			f->qid.path, f->qid.vers, qidtype(tmp, f->qid.type), f->iounit, f->unixfd);
		break;
	case Tcreate:	/* 114 */
		seprint(buf, e, "Tcreate tag %ud fid %ud name %s perm %M mode %d", tag, fid, f->name, (ulong)f->perm, f->mode);
		break;
	case Rcreate:
		seprint(buf, e, "Rcreate tag %ud qid " QIDFMT " iounit %ud ", tag,
			f->qid.path, f->qid.vers, qidtype(tmp, f->qid.type), f->iounit);
		break;
	case Tread:	/* 116 */
		seprint(buf, e, "Tread tag %ud fid %d offset %lld count %ud",
			tag, fid, f->offset, f->count);
		break;
	case Rread:
		p = seprint(buf, e, "Rread tag %ud count %ud ", tag, f->count);
			dumpsome(p, e, f->data, f->count);
		break;
	case Twrite:	/* 118 */
		p = seprint(buf, e, "Twrite tag %ud fid %d offset %lld count %ud ",
			tag, fid, f->offset, f->count);
		dumpsome(p, e, f->data, f->count);
		break;
	case Rwrite:
		seprint(buf, e, "Rwrite tag %ud count %ud", tag, f->count);
		break;
	case Tclunk:	/* 120 */
		seprint(buf, e, "Tclunk tag %ud fid %ud", tag, fid);
		break;
	case Rclunk:
		seprint(buf, e, "Rclunk tag %ud", tag);
		break;
	case Tremove:	/* 122 */
		seprint(buf, e, "Tremove tag %ud fid %ud", tag, fid);
		break;
	case Rremove:
		seprint(buf, e, "Rremove tag %ud", tag);
		break;
	case Tstat:	/* 124 */
		seprint(buf, e, "Tstat tag %ud fid %ud", tag, fid);
		break;
	case Rstat:
		p = seprint(buf, e, "Rstat tag %ud ", tag);
		if(f->stat == nil || f->nstat > sizeof tmp)
			seprint(p, e, " stat(%d bytes)", f->nstat);
		else{
			d = (Dir*)tmp;
			convM2D(f->stat, f->nstat, d, (char*)(d+1));
			seprint(p, e, " stat ");
			fdirconv(p+6, e, d);
		}
		break;
	case Twstat:	/* 126 */
		p = seprint(buf, e, "Twstat tag %ud fid %ud", tag, fid);
		if(f->stat == nil || f->nstat > sizeof tmp)
			seprint(p, e, " stat(%d bytes)", f->nstat);
		else{
			d = (Dir*)tmp;
			convM2D(f->stat, f->nstat, d, (char*)(d+1));
			seprint(p, e, " stat ");
			fdirconv(p+6, e, d);
		}
		break;
	case Rwstat:
		seprint(buf, e, "Rwstat tag %ud", tag);
		break;
	default:
		seprint(buf, e,  "unknown type %d", type);
	}
	return fmtstrcpy(fmt, buf);
}

static char*
qidtype(char *s, uchar t)
{
	char *p;

	p = s;
	if(t & QTDIR)
		*p++ = 'd';
	if(t & QTAPPEND)
		*p++ = 'a';
	if(t & QTEXCL)
		*p++ = 'l';
	if(t & QTAUTH)
		*p++ = 'A';
	*p = '\0';
	return s;
}

int
dirfmt(Fmt *fmt)
{
	char buf[160];

	fdirconv(buf, buf+sizeof buf, va_arg(fmt->args, Dir*));
	return fmtstrcpy(fmt, buf);
}

static void
fdirconv(char *buf, char *e, Dir *d)
{
	char tmp[16];

	seprint(buf, e, "'%s' '%s' '%s' '%s' "
		"q " QIDFMT " m %#luo "
		"at %ld mt %ld l %lld "
		"t %d d %d",
			d->name, d->uid, d->gid, d->muid,
			d->qid.path, d->qid.vers, qidtype(tmp, d->qid.type), d->mode,
			d->atime, d->mtime, d->length,
			d->type, d->dev);
}

/*
 * dump out count (or DUMPL, if count is bigger) bytes from
 * buf to ans, as a string if they are all printable,
 * else as a series of hex bytes
 */
#define DUMPL 64

static uint
dumpsome(char *ans, char *e, char *buf, long count)
{
	int i, printable;
	char *p;

	if(buf == nil){
		seprint(ans, e, "<no data>");
		return strlen(ans);
	}
	printable = 1;
	if(count > DUMPL)
		count = DUMPL;
	for(i=0; i<count && printable; i++)
		if((buf[i]!=0 && buf[i]<32) || (uchar)buf[i]>127)
			printable = 0;
	p = ans;
	*p++ = '\'';
	if(printable){
		if(2*count > e-p-2)
			count = (e-p-2)/2;
		for(i=0; i<count; i++){
			if(buf[i] == 0){
				*p++ = '\\';
				*p++ = '0';
			}else if(buf[i] == '\t'){
				*p++ = '\\';
				*p++ = 't';
			}else if(buf[i] == '\n'){
				*p++ = '\\';
				*p++ = 'n';
			}else
				*p++ = buf[i];
		}
	}else{
		if(2*count > e-p-2)
			count = (e-p-2)/2;
		for(i=0; i<count; i++){
			if(i>0 && i%4==0)
				*p++ = ' ';
			sprint(p, "%2.2ux", (uchar)buf[i]);
			p += 2;
		}
	}
	*p++ = '\'';
	*p = 0;
	assert(p < e);
	return p - ans;
}
