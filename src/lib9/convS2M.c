#include	<u.h>
#include	<libc.h>
#include	<fcall.h>

static
uchar*
pstring(uchar *p, char *s)
{
	uint n;

	if(s == nil){
		PBIT16(p, 0);
		p += BIT16SZ;
		return p;
	}

	n = strlen(s);
	PBIT16(p, n);
	p += BIT16SZ;
	memmove(p, s, n);
	p += n;
	return p;
}

static
uchar*
pqid(uchar *p, Qid *q)
{
	PBIT8(p, q->type);
	p += BIT8SZ;
	PBIT32(p, q->vers);
	p += BIT32SZ;
	PBIT64(p, q->path);
	p += BIT64SZ;
	return p;
}

static
uint
stringsz(char *s)
{
	if(s == nil)
		return BIT16SZ;

	return BIT16SZ+strlen(s);
}

uint
sizeS2Mu(Fcall *f, int dotu)
{
	uint n;
	int i;

	n = 0;
	n += BIT32SZ;	/* size */
	n += BIT8SZ;	/* type */
	n += BIT16SZ;	/* tag */

	switch(f->type)
	{
	default:
		return 0;

	case Tversion:
		n += BIT32SZ;
		n += stringsz(f->version);
		break;

	case Tflush:
		n += BIT16SZ;
		break;

	case Tauth:
		n += BIT32SZ;
		n += stringsz(f->uname);
		n += stringsz(f->aname);
		if(dotu)
			n += BIT32SZ;
		break;

	case Tattach:
		n += BIT32SZ;
		n += BIT32SZ;
		n += stringsz(f->uname);
		n += stringsz(f->aname);
		if(dotu)
			n += BIT32SZ;
		break;

	case Twalk:
		n += BIT32SZ;
		n += BIT32SZ;
		n += BIT16SZ;
		for(i=0; i<f->nwname; i++)
			n += stringsz(f->wname[i]);
		break;

	case Topen:
	case Topenfd:
		n += BIT32SZ;
		n += BIT8SZ;
		break;

	case Tcreate:
		n += BIT32SZ;
		n += stringsz(f->name);
		n += BIT32SZ;
		n += BIT8SZ;
		if(dotu)
			n += stringsz(f->extension);
		break;

	case Tread:
		n += BIT32SZ;
		n += BIT64SZ;
		n += BIT32SZ;
		break;

	case Twrite:
		n += BIT32SZ;
		n += BIT64SZ;
		n += BIT32SZ;
		n += f->count;
		break;

	case Tclunk:
	case Tremove:
		n += BIT32SZ;
		break;

	case Tstat:
		n += BIT32SZ;
		break;

	case Twstat:
		n += BIT32SZ;
		n += BIT16SZ;
		n += f->nstat;
		break;
/*
 */

	case Rversion:
		n += BIT32SZ;
		n += stringsz(f->version);
		break;

	case Rerror:
		n += stringsz(f->ename);
		if(dotu)
			n += BIT32SZ;
		break;

	case Rflush:
		break;

	case Rauth:
		n += QIDSZ;
		break;

	case Rattach:
		n += QIDSZ;
		break;

	case Rwalk:
		n += BIT16SZ;
		n += f->nwqid*QIDSZ;
		break;

	case Ropen:
	case Rcreate:
		n += QIDSZ;
		n += BIT32SZ;
		break;

	case Ropenfd:
		n += QIDSZ;
		n += BIT32SZ;
		n += BIT32SZ;
		break;

	case Rread:
		n += BIT32SZ;
		n += f->count;
		break;

	case Rwrite:
		n += BIT32SZ;
		break;

	case Rclunk:
		break;

	case Rremove:
		break;

	case Rstat:
		n += BIT16SZ;
		n += f->nstat;
		break;

	case Rwstat:
		break;
	}
	return n;
}

uint
sizeS2M(Fcall *f)
{
	return sizeS2Mu(f, 0);
}

uint
convS2Mu(Fcall *f, uchar *ap, uint nap, int dotu)
{
	uchar *p;
	uint i, size;

	size = sizeS2Mu(f, dotu);
	if(size == 0)
		return 0;
	if(size > nap)
		return 0;

	p = (uchar*)ap;

	PBIT32(p, size);
	p += BIT32SZ;
	PBIT8(p, f->type);
	p += BIT8SZ;
	PBIT16(p, f->tag);
	p += BIT16SZ;

	switch(f->type)
	{
	default:
		return 0;

	case Tversion:
		PBIT32(p, f->msize);
		p += BIT32SZ;
		p = pstring(p, f->version);
		break;

	case Tflush:
		PBIT16(p, f->oldtag);
		p += BIT16SZ;
		break;

	case Tauth:
		PBIT32(p, f->afid);
		p += BIT32SZ;
		p  = pstring(p, f->uname);
		p  = pstring(p, f->aname);
		if(dotu){
			f->uidnum = NOUID;
			PBIT32(p, f->uidnum);
			p += BIT32SZ;
		}
		break;

	case Tattach:
		PBIT32(p, f->fid);
		p += BIT32SZ;
		PBIT32(p, f->afid);
		p += BIT32SZ;
		p  = pstring(p, f->uname);
		p  = pstring(p, f->aname);
		if(dotu){
			f->uidnum = NOUID;
			PBIT32(p, f->uidnum);
			p += BIT32SZ;
		}
		break;

	case Twalk:
		PBIT32(p, f->fid);
		p += BIT32SZ;
		PBIT32(p, f->newfid);
		p += BIT32SZ;
		PBIT16(p, f->nwname);
		p += BIT16SZ;
		if(f->nwname > MAXWELEM)
			return 0;
		for(i=0; i<f->nwname; i++)
			p = pstring(p, f->wname[i]);
		break;

	case Topen:
	case Topenfd:
		PBIT32(p, f->fid);
		p += BIT32SZ;
		PBIT8(p, f->mode);
		p += BIT8SZ;
		break;

	case Tcreate:
		PBIT32(p, f->fid);
		p += BIT32SZ;
		p = pstring(p, f->name);
		PBIT32(p, f->perm);
		p += BIT32SZ;
		PBIT8(p, f->mode);
		p += BIT8SZ;
		if(dotu)
			p = pstring(p, f->extension);
		break;

	case Tread:
		PBIT32(p, f->fid);
		p += BIT32SZ;
		PBIT64(p, f->offset);
		p += BIT64SZ;
		PBIT32(p, f->count);
		p += BIT32SZ;
		break;

	case Twrite:
		PBIT32(p, f->fid);
		p += BIT32SZ;
		PBIT64(p, f->offset);
		p += BIT64SZ;
		PBIT32(p, f->count);
		p += BIT32SZ;
		memmove(p, f->data, f->count);
		p += f->count;
		break;

	case Tclunk:
	case Tremove:
		PBIT32(p, f->fid);
		p += BIT32SZ;
		break;

	case Tstat:
		PBIT32(p, f->fid);
		p += BIT32SZ;
		break;

	case Twstat:
		PBIT32(p, f->fid);
		p += BIT32SZ;
		PBIT16(p, f->nstat);
		p += BIT16SZ;
		memmove(p, f->stat, f->nstat);
		p += f->nstat;
		break;
/*
 */

	case Rversion:
		PBIT32(p, f->msize);
		p += BIT32SZ;
		p = pstring(p, f->version);
		break;

	case Rerror:
		p = pstring(p, f->ename);
		if(dotu){
			PBIT32(p, f->errornum);
			p += BIT32SZ;
		}
		break;

	case Rflush:
		break;

	case Rauth:
		p = pqid(p, &f->aqid);
		break;

	case Rattach:
		p = pqid(p, &f->qid);
		break;

	case Rwalk:
		PBIT16(p, f->nwqid);
		p += BIT16SZ;
		if(f->nwqid > MAXWELEM)
			return 0;
		for(i=0; i<f->nwqid; i++)
			p = pqid(p, &f->wqid[i]);
		break;

	case Ropen:
	case Rcreate:
	case Ropenfd:
		p = pqid(p, &f->qid);
		PBIT32(p, f->iounit);
		p += BIT32SZ;
		if(f->type == Ropenfd){
			PBIT32(p, f->unixfd);
			p += BIT32SZ;
		}
		break;

	case Rread:
		PBIT32(p, f->count);
		p += BIT32SZ;
		memmove(p, f->data, f->count);
		p += f->count;
		break;

	case Rwrite:
		PBIT32(p, f->count);
		p += BIT32SZ;
		break;

	case Rclunk:
		break;

	case Rremove:
		break;

	case Rstat:
		PBIT16(p, f->nstat);
		p += BIT16SZ;
		memmove(p, f->stat, f->nstat);
		p += f->nstat;
		break;

	case Rwstat:
		break;
	}
	if(size != p-ap)
		return 0;
	return size;
}

uint
convS2M(Fcall *f, uchar *ap, uint nap)
{
	return convS2Mu(f, ap, nap, 0);
}
