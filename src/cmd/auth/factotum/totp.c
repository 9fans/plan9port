/*
 * TOTP - Time-based One-time Password authentication
 *
 * Protocol:
 *
 *	C -> S:	code
 *	S -> C:	ok / bad [msg]
 */

#include "std.h"
#include "dat.h"

static int
base32d(uchar *dest, int ndest, char *src, int nsrc)
{
	char *s, *tab;
	uchar *start;
	int i, u[8];

	if((nsrc%8) != 0)
		return -1;
	if(s = memchr(src, '=', nsrc)){
		if((src+nsrc-s) >= 8)
			return -1;
		nsrc = s-src;
	}
	if(ndest+1 < (5*nsrc+7)/8)
		return -1;
	start = dest;
	tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
	while(nsrc>=8){
		for(i = 0; i < 8; i++){
			s = strchr(tab, src[i]);
			if(s == nil)
				return -1;
			u[i] = s-tab;
		}
		*dest++ = (u[0]<<3) | (0x7 & (u[1]>>2));
		*dest++ = ((0x3 & u[1])<<6) | (u[2]<<1) | (0x1 & (u[3]>>4));
		*dest++ = ((0xf & u[3])<<4) | (0xf & (u[4]>>1));
		*dest++ = ((0x1 & u[4])<<7) | (u[5]<<2) | (0x3 & (u[6]>>3));
		*dest++ = ((0x7 & u[6])<<5) | u[7];
		src  += 8;
		nsrc -= 8;
	}
	if(nsrc > 0){
		if(nsrc == 1 || nsrc == 3 || nsrc == 6)
			return -1;
		for(i=0; i<nsrc; i++){
			s = strchr(tab,(int)src[i]);
			if(s == nil)
				return -1;
			u[i] = s-tab;
		}
		*dest++ = (u[0]<<3) | (0x7 & (u[1]>>2));
		if(nsrc == 2)
			goto out;
		*dest++ = ((0x3 & u[1])<<6) | (u[2]<<1) | (0x1 & (u[3]>>4));
		if(nsrc == 4)
			goto out;
		*dest++ = ((0xf & u[3])<<4) | (0xf & (u[4]>>1));
		if(nsrc == 5)
			goto out;
		*dest++ = ((0x1 & u[4])<<7) | (u[5]<<2) | (0x3 & (u[6]>>3));
	}
out:
	return dest-start;
}

static int
genhotp(uchar *key, int n, uvlong c, int digits)
{
	uchar data[sizeof(uvlong)], digest[SHA1dlen];
	uchar *p, *bp, *ep;
	int i;
	uint32 h, d;

	for(i = 0; i < sizeof data; i++)
		data[sizeof(data)-1-i] = (c >> (8*i)) & 0xff;
	hmac_sha1(data, sizeof data, key, n, digest, nil);
	bp = &digest[digest[sizeof(digest)-1] & 0xf];
	ep = bp+3;
	h = 0;
	for(p = ep; p >= bp; p--)
		h |= *p << (8*(ep-p));
	h &= 0x7fffffff;
	d = 1;
	for(i = 0; i < digits && i < 8; i++)
		d *= 10;
	return h % d;
}

#define Period	30e9

static int
gentotp(uchar *key, int n, vlong t, int digits)
{
	return genhotp(key, n, t/Period, digits);
}

typedef struct Secret Secret;
struct Secret
{
	uchar *data;
	int n;
};

static int
totpcheck(Key *k)
{
	int n;
	char *s;
	uchar key[512];
	Secret *p;

	s = strfindattr(k->privattr, "!secret");
	if(s == nil){
		werrstr("key has no secret");
		return -1;
	}
	n = base32d(key, sizeof key, s, strlen(s));
	if(n < 0){
		werrstr("malformed secret");
		return -1;
	}
	p = malloc(sizeof *p + n);
	if(p == nil)
		return -1;
	p->data = (uchar*)(p+1);
	memmove(p->data, key, n);
	p->n = n;
	k->priv = p;
	return 0;
}

static void
totpclose(Key *k)
{
	free(k->priv);
	k->priv = nil;
}

static int
totpclient(Conv *c)
{
	Key *k;
	Secret *p;
	int code, digits;
	char *res;

	res = nil;
	c->state = "find key";
	k = keyfetch(c, "%A", c->attr);
	if(k == nil)
		goto out;
	p = k->priv;

	c->state = "write response";
	digits = 6;
	code = gentotp(p->data, p->n, nsec(), digits);
	if(convprint(c, "%.*d", digits, code) < 0)
		goto out;
	if(convreadm(c, &res) < 0)
		goto out;
	if(strcmp(res, "ok") == 0){
		keyclose(k);
		free(res);
		return 0;
	}
	if(strncmp(res, "bad ", 4) == 0)
		werrstr("%s", res+4);
	else
		werrstr("bad result: %s", res);

out:
	keyclose(k);
	free(res);
	return -1;
}

static int
totpserver(Conv *c)
{
	Key *k;
	Secret *p;
	char *resp;
	int i, code, digits;
	vlong t;
	static vlong ms[] = { 0, -Period, Period };

	resp = nil;
	c->state = "find key";
	k = keylookup("%A", c->attr);
	if(k == nil)
		goto out;

	p = k->priv;
	c->state = "read response";
	if(convreadm(c, &resp) < 0)
		goto out;

	code = atoi(resp);
	digits = strlen(resp);
	t = nsec();
	for(i = 0; i < nelem(ms); i++)
		if(gentotp(p->data, p->n, t+ms[i], digits) == code){
			c->state = "write status";
			if(convprint(c, "ok") < 0)
				goto out;
			keyclose(k);
			free(resp);
			return 0;
		}
	c->state = "write status";
	convprint(c, "bad authentication failed");

out:
	keyclose(k);
	free(resp);
	return -1;
}

static Role totproles[] = {
	"client",	totpclient,
	"server",	totpserver,
	nil,
};

Proto totp =
{
	"totp",
	totproles,
	"user? dom? !secret?",
	totpcheck,
	totpclose
};
