#include <lib9.h>

int
encodefmt(Fmt *f)
{
	char *out;
	char *buf, *p;
	int len;
	int ilen;
	int rv;
	uchar *b;
	char obuf[64];	/* rsc optimization */

	b = va_arg(f->args, uchar*);
	if(b == 0)
		return fmtstrcpy(f, "<nil>");

	ilen = f->prec;
	f->prec = 0;

	if(!(f->flags&FmtPrec) || ilen < 0)
		goto error;

	f->flags &= ~FmtPrec;

	switch(f->r){
	case '<':
		len = (8*ilen+4)/5 + 3;
		break;
	case '[':
		len = (8*ilen+5)/6 + 4;
		break;
	case 'H':
		len = 2*ilen + 1;
		break;
	default:
		goto error;
	}

	if(len > sizeof(obuf)){
		buf = malloc(len);
		if(buf == nil)
			goto error;
	} else
		buf = obuf;

	/* convert */
	out = buf;
	switch(f->r){
	case '<':
		rv = enc32(out, len, b, ilen);
		break;
	case '[':
		rv = enc64(out, len, b, ilen);
		break;
	case 'H':
		rv = enc16(out, len, b, ilen);
		if(rv >= 0 && (f->flags & FmtLong))
			for(p = buf; *p; p++)
				*p = tolower((uchar)*p);
		break;
	default:
		rv = -1;
		break;
	}
	if(rv < 0)
		goto error;

	fmtstrcpy(f, buf);
	if(buf != obuf)
		free(buf);
	return 0;

error:
	return fmtstrcpy(f, "<encodefmt>");
}
