#include "std.h"
#include "dat.h"

/*
 * DSA signing and verification
 * 
 * Sign:
 *	start p=xxx q=xxx alpha=xxx key=xxx
 *	write msg
 *	read signature(msg)
 *
 * Verify: (not implemented)
 *	start p=xxx q=xxx alpha=xxx key=xxx
 *	write msg
 *	write signature(msg)
 *	read ok or fail
 * 
 * all numbers are hexadecimal bigints parsable with strtomp.
 */
 
static int
xdsasign(Conv *c)
{
	int n;
	mpint *m;
	uchar digest[SHA1dlen];
	DSAsig *sig;
	Key *k;

	k = keylookup("%A", c->attr);
	if(k == nil)
		return -1;

	c->state = "read data";
	if((n=convread(c, digest, SHA1dlen)) < 0){
		keyclose(k);
		return -1;
	}
	m = betomp(digest, SHA1dlen, nil);
	if(m == nil){
		keyclose(k);
		return -1;
	}
	sig = dsasign(k->priv, m);
	keyclose(k);
	mpfree(m);
	if(sig == nil)
		return -1;
	convprint(c, "%B %B", sig->r, sig->s);
	dsasigfree(sig);
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

static DSApriv*
readdsapriv(Key *k)
{
	char *a;
	DSApriv *priv;

	priv = dsaprivalloc();

	if((a=strfindattr(k->attr, "p"))==nil 
	|| (priv->pub.p=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	strlwr(a);
	if((a=strfindattr(k->attr, "q"))==nil 
	|| (priv->pub.q=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	strlwr(a);
	if((a=strfindattr(k->privattr, "alpha"))==nil 
	|| (priv->pub.alpha=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	strlwr(a);
	if((a=strfindattr(k->privattr, "key"))==nil 
	|| (priv->pub.key=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	strlwr(a);
	if((a=strfindattr(k->privattr, "!secret"))==nil 
	|| (priv->secret=strtomp(a, nil, 16, nil))==nil)
		goto Error;
	strlwr(a);
	return priv;

Error:
	dsaprivfree(priv);
	return nil;
}

static int
dsacheck(Key *k)
{
	static int first = 1;
	
	if(first){
		fmtinstall('B', mpfmt);
		first = 0;
	}

	if((k->priv = readdsapriv(k)) == nil){
		werrstr("malformed key data");
		return -1;
	}
	return 0;
}

static void
dsaclose(Key *k)
{
	dsaprivfree(k->priv);
	k->priv = nil;
}

static Role
dsaroles[] = 
{
	"sign",	xdsasign,
	0
};

Proto dsa = {
	"dsa",
	dsaroles,
	nil,
	dsacheck,
	dsaclose
};

