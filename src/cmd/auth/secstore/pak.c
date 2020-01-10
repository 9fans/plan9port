/* PAK is an encrypted key exchange protocol designed by Philip MacKenzie et al. */
/* It is patented and use outside Plan 9 requires you get a license. */
/* (All other EKE protocols are patented as well, by Lucent or others.) */
#include <u.h>
#include <libc.h>
#include <mp.h>
#include <libsec.h>
#include "SConn.h"
#include "secstore.h"

extern int verbose;

char VERSION[] = "secstore";
static char *feedback[] = {"alpha","bravo","charlie","delta","echo","foxtrot","golf","hotel"};

typedef struct PAKparams{
	mpint *q, *p, *r, *g;
} PAKparams;

static PAKparams *pak;

/* from seed EB7B6E35F7CD37B511D96C67D6688CC4DD440E1E */
static void
initPAKparams(void)
{
	if(pak)
		return;
	pak = (PAKparams*)emalloc(sizeof(*pak));
	pak->q = strtomp("E0F0EF284E10796C5A2A511E94748BA03C795C13", nil, 16, nil);
	pak->p = strtomp("C41CFBE4D4846F67A3DF7DE9921A49D3B42DC33728427AB159CEC8CBB"
		"DB12B5F0C244F1A734AEB9840804EA3C25036AD1B61AFF3ABBC247CD4B384224567A86"
		"3A6F020E7EE9795554BCD08ABAD7321AF27E1E92E3DB1C6E7E94FAAE590AE9C48F96D9"
		"3D178E809401ABE8A534A1EC44359733475A36A70C7B425125062B1142D",
		nil, 16, nil);
	pak->r = strtomp("DF310F4E54A5FEC5D86D3E14863921E834113E060F90052AD332B3241"
		"CEF2497EFA0303D6344F7C819691A0F9C4A773815AF8EAECFB7EC1D98F039F17A32A7E"
		"887D97251A927D093F44A55577F4D70444AEBD06B9B45695EC23962B175F266895C67D"
		"21C4656848614D888A4", nil, 16, nil);
	pak->g = strtomp("2F1C308DC46B9A44B52DF7DACCE1208CCEF72F69C743ADD4D23271734"
		"44ED6E65E074694246E07F9FD4AE26E0FDDD9F54F813C40CB9BCD4338EA6F242AB94CD"
		"410E676C290368A16B1A3594877437E516C53A6EEE5493A038A017E955E218E7819734"
		"E3E2A6E0BAE08B14258F8C03CC1B30E0DDADFCF7CEDF0727684D3D255F1",
		nil, 16, nil);
}

/* H = (sha(ver,C,sha(passphrase)))^r mod p, */
/* a hash function expensive to attack by brute force. */
static void
longhash(char *ver, char *C, uchar *passwd, mpint *H)
{
	uchar *Cp;
	int i, n, nver, nC;
	uchar buf[140], key[1];

	nver = strlen(ver);
	nC = strlen(C);
	n = nver + nC + SHA1dlen;
	Cp = (uchar*)emalloc(n);
	memmove(Cp, ver, nver);
	memmove(Cp+nver, C, nC);
	memmove(Cp+nver+nC, passwd, SHA1dlen);
	for(i = 0; i < 7; i++){
		key[0] = 'A'+i;
		hmac_sha1(Cp, n, key, sizeof key, buf+i*SHA1dlen, nil);
	}
	memset(Cp, 0, n);
	free(Cp);
	betomp(buf, sizeof buf, H);
	mpmod(H, pak->p, H);
	mpexp(H, pak->r, pak->p, H);
}

/* Hi = H^-1 mod p */
char *
PAK_Hi(char *C, char *passphrase, mpint *H, mpint *Hi)
{
	uchar passhash[SHA1dlen];

	sha1((uchar *)passphrase, strlen(passphrase), passhash, nil);
	initPAKparams();
	longhash(VERSION, C, passhash, H);
	mpinvert(H, pak->p, Hi);
	return mptoa(Hi, 64, nil, 0);
}

/* another, faster, hash function for each party to */
/* confirm that the other has the right secrets. */
static void
shorthash(char *mess, char *C, char *S, char *m, char *mu, char *sigma, char *Hi, uchar *digest)
{
	SHA1state *state;

	state = sha1((uchar*)mess, strlen(mess), 0, 0);
	state = sha1((uchar*)C, strlen(C), 0, state);
	state = sha1((uchar*)S, strlen(S), 0, state);
	state = sha1((uchar*)m, strlen(m), 0, state);
	state = sha1((uchar*)mu, strlen(mu), 0, state);
	state = sha1((uchar*)sigma, strlen(sigma), 0, state);
	state = sha1((uchar*)Hi, strlen(Hi), 0, state);
	state = sha1((uchar*)mess, strlen(mess), 0, state);
	state = sha1((uchar*)C, strlen(C), 0, state);
	state = sha1((uchar*)S, strlen(S), 0, state);
	state = sha1((uchar*)m, strlen(m), 0, state);
	state = sha1((uchar*)mu, strlen(mu), 0, state);
	state = sha1((uchar*)sigma, strlen(sigma), 0, state);
	sha1((uchar*)Hi, strlen(Hi), digest, state);
}

/* On input, conn provides an open channel to the server; */
/*	C is the name this client calls itself; */
/*	pass is the user's passphrase */
/* On output, session secret has been set in conn */
/*	(unless return code is negative, which means failure). */
/*    If pS is not nil, it is set to the (alloc'd) name the server calls itself. */
int
PAKclient(SConn *conn, char *C, char *pass, char **pS)
{
	char *mess, *mess2, *eol, *S, *hexmu, *ks, *hexm, *hexsigma = nil, *hexHi;
	char kc[2*SHA1dlen+1];
	uchar digest[SHA1dlen];
	int rc = -1, n;
	mpint *x, *m = mpnew(0), *mu = mpnew(0), *sigma = mpnew(0);
	mpint *H = mpnew(0), *Hi = mpnew(0);

	hexHi = PAK_Hi(C, pass, H, Hi);
	if(verbose)
		fprint(2,"%s\n", feedback[H->p[0]&0x7]);  /* provide a clue to catch typos */

	/* random 1<=x<=q-1; send C, m=g**x H */
	x = mprand(240, genrandom, nil);
	mpmod(x, pak->q, x);
	if(mpcmp(x, mpzero) == 0)
		mpassign(mpone, x);
	mpexp(pak->g, x, pak->p, m);
	mpmul(m, H, m);
	mpmod(m, pak->p, m);
	hexm = mptoa(m, 64, nil, 0);
	mess = (char*)emalloc(2*Maxmsg+2);
	mess2 = mess+Maxmsg+1;
	snprint(mess, Maxmsg, "%s\tPAK\nC=%s\nm=%s\n", VERSION, C, hexm);
	conn->write(conn, (uchar*)mess, strlen(mess));

	/* recv g**y, S, check hash1(g**xy) */
	if(readstr(conn, mess) < 0){
		fprint(2, "error: %s\n", mess);
		writerr(conn, "couldn't read g**y");
		goto done;
	}
	eol = strchr(mess, '\n');
	if(strncmp("mu=", mess, 3) != 0 || !eol || strncmp("\nk=", eol, 3) != 0){
		writerr(conn, "verifier syntax error");
		goto done;
	}
	hexmu = mess+3;
	*eol = 0;
	ks = eol+3;
	eol = strchr(ks, '\n');
	if(!eol || strncmp("\nS=", eol, 3) != 0){
		writerr(conn, "verifier syntax error for secstore 1.0");
		goto done;
	}
	*eol = 0;
	S = eol+3;
	eol = strchr(S, '\n');
	if(!eol){
		writerr(conn, "verifier syntax error for secstore 1.0");
		goto done;
	}
	*eol = 0;
	if(pS)
		*pS = estrdup(S);
	strtomp(hexmu, nil, 64, mu);
	mpexp(mu, x, pak->p, sigma);
	hexsigma = mptoa(sigma, 64, nil, 0);
	shorthash("server", C, S, hexm, hexmu, hexsigma, hexHi, digest);
	enc64(kc, sizeof kc, digest, SHA1dlen);
	if(strcmp(ks, kc) != 0){
		writerr(conn, "verifier didn't match");
		goto done;
	}

	/* send hash2(g**xy) */
	shorthash("client", C, S, hexm, hexmu, hexsigma, hexHi, digest);
	enc64(kc, sizeof kc, digest, SHA1dlen);
	snprint(mess2, Maxmsg, "k'=%s\n", kc);
	conn->write(conn, (uchar*)mess2, strlen(mess2));

	/* set session key */
	shorthash("session", C, S, hexm, hexmu, hexsigma, hexHi, digest);
	memset(hexsigma, 0, strlen(hexsigma));
	n = conn->secret(conn, digest, 0);
	memset(digest, 0, SHA1dlen);
	if(n < 0){
		writerr(conn, "can't set secret");
		goto done;
	}

	rc = 0;
done:
	mpfree(x);
	mpfree(sigma);
	mpfree(mu);
	mpfree(m);
	mpfree(Hi);
	mpfree(H);
	free(hexsigma);
	free(hexHi);
	free(hexm);
	free(mess);
	return rc;
}

/* On input, */
/*	mess contains first message; */
/*	name is name this server should call itself. */
/* On output, session secret has been set in conn; */
/*	if pw!=nil, then *pw points to PW struct for authenticated user. */
/*	returns -1 if error */
int
PAKserver(SConn *conn, char *S, char *mess, PW **pwp)
{
	int rc = -1, n;
	char mess2[Maxmsg+1], *eol;
	char *C, ks[41], *kc, *hexm, *hexmu = nil, *hexsigma = nil, *hexHi = nil;
	uchar digest[SHA1dlen];
	mpint *H = mpnew(0), *Hi = mpnew(0);
	mpint *y = nil, *m = mpnew(0), *mu = mpnew(0), *sigma = mpnew(0);
	PW *pw = nil;

	/* secstore version and algorithm */
	snprint(mess2,Maxmsg,"%s\tPAK\n", VERSION);
	n = strlen(mess2);
	if(strncmp(mess,mess2,n) != 0){
		writerr(conn, "protocol should start with ver alg");
		return -1;
	}
	mess += n;
	initPAKparams();

	/* parse first message into C, m */
	eol = strchr(mess, '\n');
	if(strncmp("C=", mess, 2) != 0 || !eol){
		fprint(2,"mess[1]=%s\n", mess);
		writerr(conn, "PAK version mismatch");
		goto done;
	}
	C = mess+2;
	*eol = 0;
	hexm = eol+3;
	eol = strchr(hexm, '\n');
	if(strncmp("m=", hexm-2, 2) != 0 || !eol){
		writerr(conn, "PAK version mismatch");
		goto done;
	}
	*eol = 0;
	strtomp(hexm, nil, 64, m);
	mpmod(m, pak->p, m);

	/* lookup client */
	if((pw = getPW(C,0)) == nil) {
		snprint(mess2, sizeof mess2, "%r");
		writerr(conn, mess2);
		goto done;
	}
	if(mpcmp(m, mpzero) == 0) {
		writerr(conn, "account exists");
		freePW(pw);
		pw = nil;
		goto done;
	}
	hexHi = mptoa(pw->Hi, 64, nil, 0);

	/* random y, mu=g**y, sigma=g**xy */
	y = mprand(240, genrandom, nil);
	mpmod(y, pak->q, y);
	if(mpcmp(y, mpzero) == 0){
		mpassign(mpone, y);
	}
	mpexp(pak->g, y, pak->p, mu);
	mpmul(m, pw->Hi, m);
	mpmod(m, pak->p, m);
	mpexp(m, y, pak->p, sigma);

	/* send g**y, hash1(g**xy) */
	hexmu = mptoa(mu, 64, nil, 0);
	hexsigma = mptoa(sigma, 64, nil, 0);
	shorthash("server", C, S, hexm, hexmu, hexsigma, hexHi, digest);
	enc64(ks, sizeof ks, digest, SHA1dlen);
	snprint(mess2, sizeof mess2, "mu=%s\nk=%s\nS=%s\n", hexmu, ks, S);
	conn->write(conn, (uchar*)mess2, strlen(mess2));

	/* recv hash2(g**xy) */
	if(readstr(conn, mess2) < 0){
		writerr(conn, "couldn't read verifier");
		goto done;
	}
	eol = strchr(mess2, '\n');
	if(strncmp("k'=", mess2, 3) != 0 || !eol){
		writerr(conn, "verifier syntax error");
		goto done;
	}
	kc = mess2+3;
	*eol = 0;
	shorthash("client", C, S, hexm, hexmu, hexsigma, hexHi, digest);
	enc64(ks, sizeof ks, digest, SHA1dlen);
	if(strcmp(ks, kc) != 0) {
		rc = -2;
		goto done;
	}

	/* set session key */
	shorthash("session", C, S, hexm, hexmu, hexsigma, hexHi, digest);
	n = conn->secret(conn, digest, 1);
	if(n < 0){
		writerr(conn, "can't set secret");
		goto done;
	}

	rc = 0;
done:
	if(rc<0 && pw){
		pw->failed++;
		putPW(pw);
	}
	if(rc==0 && pw && pw->failed>0){
		pw->failed = 0;
		putPW(pw);
	}
	if(pwp)
		*pwp = pw;
	else
		freePW(pw);
	free(hexsigma);
	free(hexHi);
	free(hexmu);
	mpfree(y);
	mpfree(sigma);
	mpfree(mu);
	mpfree(m);
	mpfree(Hi);
	mpfree(H);
	return rc;
}
