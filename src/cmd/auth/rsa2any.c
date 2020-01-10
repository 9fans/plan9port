#include <u.h>
#include <libc.h>
#include <bio.h>
#include <auth.h>
#include <mp.h>
#include <libsec.h>
#include "rsa2any.h"

RSApriv*
getkey(int argc, char **argv, int needprivate, Attr **pa)
{
	char *file, *s, *p;
	int sz;
	RSApriv *key;
	Biobuf *b;
	int regen;
	Attr *a;

	if(argc == 0)
		file = "/dev/stdin";
	else
		file = argv[0];

	key = mallocz(sizeof(RSApriv), 1);
	if(key == nil)
		return nil;

	if((b = Bopen(file, OREAD)) == nil){
		werrstr("open %s: %r", file);
		return nil;
	}
	s = Brdstr(b, '\n', 1);
	if(s == nil){
		werrstr("read %s: %r", file);
		return nil;
	}
	if(strncmp(s, "key ", 4) != 0){
		werrstr("bad key format");
		return nil;
	}

	regen = 0;
	a = _parseattr(s+4);
	if(a == nil){
		werrstr("empty key");
		return nil;
	}
	if((p = _strfindattr(a, "proto")) == nil){
		werrstr("no proto");
		return nil;
	}
	if(strcmp(p, "rsa") != 0){
		werrstr("proto not rsa");
		return nil;
	}
	if((p = _strfindattr(a, "ek")) == nil){
		werrstr("no ek");
		return nil;
	}
	if((key->pub.ek = strtomp(p, &p, 16, nil)) == nil || *p != 0){
		werrstr("bad ek");
		return nil;
	}
	if((p = _strfindattr(a, "n")) == nil){
		werrstr("no n");
		return nil;
	}
	if((key->pub.n = strtomp(p, &p, 16, nil)) == nil || *p != 0){
		werrstr("bad n");
		return nil;
	}
	if((p = _strfindattr(a, "size")) == nil)
		fprint(2, "warning: missing size; will add\n");
	else if((sz = strtol(p, &p, 10)) == 0 || *p != 0)
		fprint(2, "warning: bad size; will correct\n");
	else if(sz != mpsignif(key->pub.n))
		fprint(2, "warning: wrong size (got %d, expected %d); will correct\n",
			sz, mpsignif(key->pub.n));
	if(!needprivate)
		goto call;
	if((p = _strfindattr(a, "!dk")) == nil){
		werrstr("no !dk");
		return nil;
	}
	if((key->dk = strtomp(p, &p, 16, nil)) == nil || *p != 0){
		werrstr("bad !dk");
		return nil;
	}
	if((p = _strfindattr(a, "!p")) == nil){
		werrstr("no !p");
		return nil;
	}
	if((key->p = strtomp(p, &p, 16, nil)) == nil || *p != 0){
		werrstr("bad !p");
		return nil;
	}
	if((p = _strfindattr(a, "!q")) == nil){
		werrstr("no !q");
		return nil;
	}
	if((key->q = strtomp(p, &p, 16, nil)) == nil || *p != 0){
		werrstr("bad !q");
		return nil;
	}
	if((p = _strfindattr(a, "!kp")) == nil){
		fprint(2, "warning: no !kp\n");
		regen = 1;
		goto regen;
	}
	if((key->kp = strtomp(p, &p, 16, nil)) == nil || *p != 0){
		fprint(2, "warning: bad !kp\n");
		regen = 1;
		goto regen;
	}
	if((p = _strfindattr(a, "!kq")) == nil){
		fprint(2, "warning: no !kq\n");
		regen = 1;
		goto regen;
	}
	if((key->kq = strtomp(p, &p, 16, nil)) == nil || *p != 0){
		fprint(2, "warning: bad !kq\n");
		regen = 1;
		goto regen;
	}
	if((p = _strfindattr(a, "!c2")) == nil){
		fprint(2, "warning: no !c2\n");
		regen = 1;
		goto regen;
	}
	if((key->c2 = strtomp(p, &p, 16, nil)) == nil || *p != 0){
		fprint(2, "warning: bad !c2\n");
		regen = 1;
		goto regen;
	}
regen:
	if(regen){
		RSApriv *k2;

		k2 = rsafill(key->pub.n, key->pub.ek, key->dk, key->p, key->q);
		if(k2 == nil){
			werrstr("regenerating chinese-remainder parts failed: %r");
			return nil;
		}
		key = k2;
	}
call:
	a = _delattr(a, "ek");
	a = _delattr(a, "n");
	a = _delattr(a, "size");
	a = _delattr(a, "!dk");
	a = _delattr(a, "!p");
	a = _delattr(a, "!q");
	a = _delattr(a, "!c2");
	a = _delattr(a, "!kp");
	a = _delattr(a, "!kq");
	if(pa)
		*pa = a;
	return key;
}

DSApriv*
getdsakey(int argc, char **argv, int needprivate, Attr **pa)
{
	char *file, *s, *p;
	DSApriv *key;
	Biobuf *b;
	Attr *a;

	if(argc == 0)
		file = "/dev/stdin";
	else
		file = argv[0];

	key = mallocz(sizeof(RSApriv), 1);
	if(key == nil)
		return nil;

	if((b = Bopen(file, OREAD)) == nil){
		werrstr("open %s: %r", file);
		return nil;
	}
	s = Brdstr(b, '\n', 1);
	if(s == nil){
		werrstr("read %s: %r", file);
		return nil;
	}
	if(strncmp(s, "key ", 4) != 0){
		werrstr("bad key format");
		return nil;
	}

	a = _parseattr(s+4);
	if(a == nil){
		werrstr("empty key");
		return nil;
	}
	if((p = _strfindattr(a, "proto")) == nil){
		werrstr("no proto");
		return nil;
	}
	if(strcmp(p, "dsa") != 0){
		werrstr("proto not dsa");
		return nil;
	}
	if((p = _strfindattr(a, "p")) == nil){
		werrstr("no p");
		return nil;
	}
	if((key->pub.p = strtomp(p, &p, 16, nil)) == nil || *p != 0){
		werrstr("bad p");
		return nil;
	}
	if((p = _strfindattr(a, "q")) == nil){
		werrstr("no q");
		return nil;
	}
	if((key->pub.q = strtomp(p, &p, 16, nil)) == nil || *p != 0){
		werrstr("bad q");
		return nil;
	}
	if((p = _strfindattr(a, "alpha")) == nil){
		werrstr("no alpha");
		return nil;
	}
	if((key->pub.alpha = strtomp(p, &p, 16, nil)) == nil || *p != 0){
		werrstr("bad alpha");
		return nil;
	}
	if((p = _strfindattr(a, "key")) == nil){
		werrstr("no key=");
		return nil;
	}
	if((key->pub.key = strtomp(p, &p, 16, nil)) == nil || *p != 0){
		werrstr("bad key=");
		return nil;
	}
	if(!needprivate)
		goto call;
	if((p = _strfindattr(a, "!secret")) == nil){
		werrstr("no !secret");
		return nil;
	}
	if((key->secret = strtomp(p, &p, 16, nil)) == nil || *p != 0){
		werrstr("bad !secret");
		return nil;
	}
call:
	a = _delattr(a, "p");
	a = _delattr(a, "q");
	a = _delattr(a, "alpha");
	a = _delattr(a, "key");
	a = _delattr(a, "!secret");
	if(pa)
		*pa = a;
	return key;
}

uchar*
put4(uchar *p, uint n)
{
	p[0] = (n>>24)&0xFF;
	p[1] = (n>>16)&0xFF;
	p[2] = (n>>8)&0xFF;
	p[3] = n&0xFF;
	return p+4;
}

uchar*
putn(uchar *p, void *v, uint n)
{
	memmove(p, v, n);
	p += n;
	return p;
}

uchar*
putstr(uchar *p, char *s)
{
	p = put4(p, strlen(s));
	p = putn(p, s, strlen(s));
	return p;
}

uchar*
putmp2(uchar *p, mpint *b)
{
	int bits, n;

	if(mpcmp(b, mpzero) == 0)
		return put4(p, 0);
	bits = mpsignif(b);
	n = (bits+7)/8;
	if(bits%8 == 0){
		p = put4(p, n+1);
		*p++ = 0;
	}else
		p = put4(p, n);
	mptobe(b, p, n, nil);
	p += n;
	return p;
}
