#include	<u.h>
#include	<libc.h>
#include	<fcall.h>

uint
sizeD2Mu(Dir *d, int dotu)
{
	char *sv[5];
	int i, ns, nstr, fixlen;

	sv[0] = d->name;
	sv[1] = d->uid;
	sv[2] = d->gid;
	sv[3] = d->muid;
	
	fixlen = STATFIXLEN;
	nstr = 4;
	if(dotu){
		fixlen = STATFIXLENU;
		sv[4] = d->ext;
		nstr = 5;
	}
	
	ns = 0;
	for(i = 0; i < nstr; i++)
		if(sv[i])
			ns += strlen(sv[i]);

	return fixlen + ns;
}

uint
sizeD2M(Dir *d)
{
	return sizeD2Mu(d, 0);
}

uint
convD2Mu(Dir *d, uchar *buf, uint nbuf, int dotu)
{
	uchar *p, *ebuf;
	char *sv[5];
	int i, ns, nsv[5], ss, nstr, fixlen;

	if(nbuf < BIT16SZ)
		return 0;

	p = buf;
	ebuf = buf + nbuf;

	sv[0] = d->name;
	sv[1] = d->uid;
	sv[2] = d->gid;
	sv[3] = d->muid;

	fixlen = STATFIXLEN;
	nstr = 4;
	if(dotu){
		fixlen = STATFIXLENU;
		sv[4] = d->ext;
		nstr = 5;
	}
	
	ns = 0;
	for(i = 0; i < nstr; i++){
		if(sv[i])
			nsv[i] = strlen(sv[i]);
		else
			nsv[i] = 0;
		ns += nsv[i];
	}

	ss = fixlen + ns;

	/* set size befor erroring, so user can know how much is needed */
	/* note that length excludes count field itself */
	PBIT16(p, ss-BIT16SZ);
	p += BIT16SZ;

	if(ss > nbuf)
		return BIT16SZ;

	PBIT16(p, d->type);
	p += BIT16SZ;
	PBIT32(p, d->dev);
	p += BIT32SZ;
	PBIT8(p, d->qid.type);
	p += BIT8SZ;
	PBIT32(p, d->qid.vers);
	p += BIT32SZ;
	PBIT64(p, d->qid.path);
	p += BIT64SZ;
	PBIT32(p, d->mode);
	p += BIT32SZ;
	PBIT32(p, d->atime);
	p += BIT32SZ;
	PBIT32(p, d->mtime);
	p += BIT32SZ;
	PBIT64(p, d->length);
	p += BIT64SZ;

	for(i = 0; i < nstr; i++){
		ns = nsv[i];
		if(p + ns + BIT16SZ > ebuf)
			return 0;
		PBIT16(p, ns);
		p += BIT16SZ;
		if(ns)
			memmove(p, sv[i], ns);
		p += ns;
	}
	
	if(dotu){
		PBIT32(p, d->uidnum);
		p += BIT32SZ;
		PBIT32(p, d->gidnum);
		p += BIT32SZ;
		PBIT32(p, d->muidnum);
		p += BIT32SZ;
	}

	if(ss != p - buf)
		return 0;

	return p - buf;
}

uint
convD2M(Dir *d, uchar *buf, uint nbuf)
{
	return convD2Mu(d, buf, nbuf, 0);
}
