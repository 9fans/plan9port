/*
 * HTTPDIGEST - MD5 challenge/response authentication (RFC 2617)
 *
 * Client protocol:
 *	write challenge: nonce method uri
 *	read response: 2*MD5dlen hex digits
 *
 * Server protocol:
 *	unimplemented
 */
#include "std.h"
#include "dat.h"

static void
digest(char *user, char *realm, char *passwd,
	char *nonce, char *method, char *uri,
	char *dig);

static int
hdclient(Conv *c)
{
	char *realm, *passwd, *user, *f[4], *s, resp[MD5dlen*2+1];
	int ret;
	Key *k;

	ret = -1;
	s = nil;

	c->state = "keylookup";
	k = keyfetch(c, "%A", c->attr);
	if(k == nil)
		goto out;

	user = strfindattr(k->attr, "user");
	realm = strfindattr(k->attr, "realm");
	passwd = strfindattr(k->attr, "!password");

	if(convreadm(c, &s) < 0)
		goto out;
	if(tokenize(s, f, 4) != 3){
		werrstr("bad challenge -- want nonce method uri");
		goto out;
	}

	digest(user, realm, passwd, f[0], f[1], f[2], resp);
	convwrite(c, resp, strlen(resp));
	ret = 0;

out:
	free(s);
	keyclose(k);
	return ret;
}

static void
strtolower(char *s)
{
	while(*s){
		*s = tolower((uchar)*s);
		s++;
	}
}

static void
digest(char *user, char *realm, char *passwd,
	char *nonce, char *method, char *uri,
	char *dig)
{
	uchar b[MD5dlen];
	char ha1[MD5dlen*2+1];
	char ha2[MD5dlen*2+1];
	DigestState *s;

	/*
	 *  H(A1) = MD5(uid + ":" + realm ":" + passwd)
	 */
	s = md5((uchar*)user, strlen(user), nil, nil);
	md5((uchar*)":", 1, nil, s);
	md5((uchar*)realm, strlen(realm), nil, s);
	md5((uchar*)":", 1, nil, s);
	md5((uchar*)passwd, strlen(passwd), b, s);
	enc16(ha1, sizeof(ha1), b, MD5dlen);
	strtolower(ha1);

	/*
	 *  H(A2) = MD5(method + ":" + uri)
	 */
	s = md5((uchar*)method, strlen(method), nil, nil);
	md5((uchar*)":", 1, nil, s);
	md5((uchar*)uri, strlen(uri), b, s);
	enc16(ha2, sizeof(ha2), b, MD5dlen);
	strtolower(ha2);

	/*
	 *  digest = MD5(H(A1) + ":" + nonce + ":" + H(A2))
	 */
	s = md5((uchar*)ha1, MD5dlen*2, nil, nil);
	md5((uchar*)":", 1, nil, s);
	md5((uchar*)nonce, strlen(nonce), nil, s);
	md5((uchar*)":", 1, nil, s);
	md5((uchar*)ha2, MD5dlen*2, b, s);
	enc16(dig, MD5dlen*2+1, b, MD5dlen);
	strtolower(dig);
}

static Role hdroles[] =
{
	"client",	hdclient,
	0
};

Proto httpdigest =
{
	"httpdigest",
	hdroles,
	"user? realm? !password?",
	0,
	0
};
