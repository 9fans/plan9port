#include "std.h"
#include "dat.h"

/*
 * RSA authentication.
 * 
 * Client:
 *	start n=xxx ek=xxx
 *	write msg
 *	read decrypt(msg)
 *
 * Sign (PKCS #1 using hash=sha1 or hash=md5)
 *	start n=xxx ek=xxx
 *	write hash(msg)
 *	read signature(hash(msg))
 * 
 * all numbers are hexadecimal biginits parsable with strtomp.
 * must be lower case for attribute matching in start.
 */

static int
rsaclient(Conv *c)
{
	char *chal;
	mpint *m;
	Key *k;

	k = keylookup("%A", c->attr);
	if(k == nil)
		return -1;
	c->state = "read challenge";
	if(convreadm(c, &chal) < 0){
		keyclose(k);
		return -1;
	}
	if(strlen(chal) < 32){
	badchal:
		free(chal);
		convprint(c, "bad challenge");
		keyclose(k);
		return -1;
	}
	m = strtomp(chal, nil, 16, nil);
	if(m == nil)
		goto badchal;
	free(chal);
	m = rsadecrypt(k->priv, m, m);
	convprint(c, "%B", m);
	mpfree(m);
	keyclose(k);
	return 0;
}

static int
xrsasign(Conv *c)
{
	char *hash;
	int dlen, n;
	DigestAlg *hashfn;
	Key *k;
	uchar sig[1024], digest[64];

	k = keylookup("%A", c->attr);
	if(k == nil)
		return -1;
	hash = strfindattr(k->attr, "hash");
	if(hash == nil)
		hash = "sha1";
	if(strcmp(hash, "sha1") == 0){
		hashfn = sha1;
		dlen = SHA1dlen;
	}else if(strcmp(hash, "md5") == 0){
		hashfn = md5;
		dlen = MD5dlen;
	}else{
		werrstr("unknown hash function %s", hash);
		return -1;
	}
	c->state = "read data";
	if((n=convread(c, digest, dlen)) < 0){
		keyclose(k);
		return -1;
	}
	memset(sig, 0xAA, sizeof sig);
	n = rsasign(k->priv, hashfn, digest, dlen, sig, sizeof sig);
	keyclose(k);
	if(n < 0)
		return -1;	
	convwrite(c, sig, n);
	return 0;
}

/*
 * convert to canonical form (lower case) 
 * for use in attribute matches.
 */
static void
strlwr(char *a)
{
	for(; *a; a++){
		if('A' <= *a && *a <= 'Z')
			*a += 'a' - 'A';
	}
}

static RSApriv*
readrsapriv(Key *k)
{
	char *a;
	RSApriv *priv;

	priv = rsaprivalloc();

	if((a=strfindattr(k->attr, "ek"))==nil 
	|| (priv->pub.ek=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	strlwr(a);
	if((a=strfindattr(k->attr, "n"))==nil 
	|| (priv->pub.n=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	strlwr(a);
	if((a=strfindattr(k->privattr, "!p"))==nil 
	|| (priv->p=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	strlwr(a);
	if((a=strfindattr(k->privattr, "!q"))==nil 
	|| (priv->q=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	strlwr(a);
	if((a=strfindattr(k->privattr, "!kp"))==nil 
	|| (priv->kp=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	strlwr(a);
	if((a=strfindattr(k->privattr, "!kq"))==nil 
	|| (priv->kq=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	strlwr(a);
	if((a=strfindattr(k->privattr, "!c2"))==nil 
	|| (priv->c2=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	strlwr(a);
	if((a=strfindattr(k->privattr, "!dk"))==nil 
	|| (priv->dk=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	strlwr(a);
	return priv;

Error:
	rsaprivfree(priv);
	return nil;
}

static int
rsacheck(Key *k)
{
	static int first = 1;
	
	if(first){
		fmtinstall('B', mpfmt);
		first = 0;
	}

	if((k->priv = readrsapriv(k)) == nil){
		werrstr("malformed key data");
		return -1;
	}
	return 0;
}

static void
rsaclose(Key *k)
{
	rsaprivfree(k->priv);
	k->priv = nil;
}

static Role
rsaroles[] = 
{
	"client",	rsaclient,
	"sign",	xrsasign,
	0
};

Proto rsa = {
	"rsa",
	rsaroles,
	nil,
	rsacheck,
	rsaclose
};
