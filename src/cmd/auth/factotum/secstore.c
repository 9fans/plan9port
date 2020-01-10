/*
 * Various files from /sys/src/cmd/auth/secstore, just enough
 * to download a file at boot time.
 */

#include "std.h"
#include "dat.h"
#include <ip.h>

enum{ CHK = 16};
enum{ MAXFILESIZE = 10*1024*1024 };

enum{/* PW status bits */
	Enabled 	= (1<<0),
	STA 		= (1<<1)	/* extra SecurID step */
};

static char testmess[] = "__secstore\tPAK\nC=%s\nm=0\n";
char *secstore;

int
secdial(void)
{
	char *p;

	p = secstore;
	if(p == nil)	  /* else use the authserver */
		p = getenv("secstore");
	if(p == nil)
		p = getenv("auth");
	if(p == nil)
		p = "secstore";

	return dial(netmkaddr(p, "net", "secstore"), 0, 0, 0);
}


int
havesecstore(void)
{
	int m, n, fd;
	uchar buf[500];

	n = snprint((char*)buf, sizeof buf, testmess, owner);
	hnputs(buf, 0x8000+n-2);

	fd = secdial();
	if(fd < 0){
		if(debug)
			fprint(2, "secdial: %r\n");
		flog("secdial: %r");
		return 0;
	}
	if(write(fd, buf, n) != n || readn(fd, buf, 2) != 2){
		flog("secstore: no count");
		close(fd);
		return 0;
	}
	n = ((buf[0]&0x7f)<<8) + buf[1];
	if(n+1 > sizeof buf){
		flog("secstore: bad count");
		werrstr("implausibly large count %d", n);
		close(fd);
		return 0;
	}
	m = readn(fd, buf, n);
	close(fd);
	if(m != n){
		flog("secstore: unexpected eof");
		if(m >= 0)
			werrstr("short read from secstore");
		return 0;
	}
	buf[n] = 0;
	if(strcmp((char*)buf, "!account expired") == 0){
		flog("secstore: account expired");
		werrstr("account expired");
		return 0;
	}
	if(strcmp((char*)buf, "!account exists") == 0){
		flog("secstore: account exists");
		return 1;
	}
	flog("secstore: %s", buf);
	return 0;
}

/* delimited, authenticated, encrypted connection */
enum{ Maxmsg=4096 };	/* messages > Maxmsg bytes are truncated */
typedef struct SConn SConn;

extern SConn* newSConn(int);	/* arg is open file descriptor */
struct SConn{
	void *chan;
	int secretlen;
	int (*secret)(SConn*, uchar*, int);/*  */
	int (*read)(SConn*, uchar*, int); /* <0 if error;  errmess in buffer */
	int (*write)(SConn*, uchar*, int);
	void (*free)(SConn*);		/* also closes file descriptor */
};
/* secret(s,b,dir) sets secret for digest, encrypt, using the secretlen */
/*		bytes in b to form keys 	for the two directions; */
/*	  set dir=0 in client, dir=1 in server */

/* error convention: write !message in-band */
#define readstr secstore_readstr
static void writerr(SConn*, char*);
static int readstr(SConn*, char*);  /* call with buf of size Maxmsg+1 */
	/* returns -1 upon error, with error message in buf */

typedef struct ConnState {
	uchar secret[SHA1dlen];
	ulong seqno;
	RC4state rc4;
} ConnState;

#undef SS
typedef struct SS {
	int fd;		/* file descriptor for read/write of encrypted data */
	int alg;	/* if nonzero, "alg sha rc4_128" */
	ConnState in, out;
} SS;

static int
SC_secret(SConn *conn, uchar *sigma, int direction)
{
	SS *ss = (SS*)(conn->chan);
	int nsigma = conn->secretlen;

	if(direction != 0){
		hmac_sha1(sigma, nsigma, (uchar*)"one", 3, ss->out.secret, nil);
		hmac_sha1(sigma, nsigma, (uchar*)"two", 3, ss->in.secret, nil);
	}else{
		hmac_sha1(sigma, nsigma, (uchar*)"two", 3, ss->out.secret, nil);
		hmac_sha1(sigma, nsigma, (uchar*)"one", 3, ss->in.secret, nil);
	}
	setupRC4state(&ss->in.rc4, ss->in.secret, 16); /* restrict to 128 bits */
	setupRC4state(&ss->out.rc4, ss->out.secret, 16);
	ss->alg = 1;
	return 0;
}

static void
hash(uchar secret[SHA1dlen], uchar *data, int len, int seqno, uchar d[SHA1dlen])
{
	DigestState sha;
	uchar seq[4];

	seq[0] = seqno>>24;
	seq[1] = seqno>>16;
	seq[2] = seqno>>8;
	seq[3] = seqno;
	memset(&sha, 0, sizeof sha);
	sha1(secret, SHA1dlen, nil, &sha);
	sha1(data, len, nil, &sha);
	sha1(seq, 4, d, &sha);
}

static int
verify(uchar secret[SHA1dlen], uchar *data, int len, int seqno, uchar d[SHA1dlen])
{
	DigestState sha;
	uchar seq[4];
	uchar digest[SHA1dlen];

	seq[0] = seqno>>24;
	seq[1] = seqno>>16;
	seq[2] = seqno>>8;
	seq[3] = seqno;
	memset(&sha, 0, sizeof sha);
	sha1(secret, SHA1dlen, nil, &sha);
	sha1(data, len, nil, &sha);
	sha1(seq, 4, digest, &sha);
	return memcmp(d, digest, SHA1dlen);
}

static int
SC_read(SConn *conn, uchar *buf, int n)
{
	SS *ss = (SS*)(conn->chan);
	uchar count[2], digest[SHA1dlen];
	int len, nr;

	if(read(ss->fd, count, 2) != 2 || (count[0]&0x80) == 0){
		werrstr("!SC_read invalid count");
		return -1;
	}
	len = (count[0]&0x7f)<<8 | count[1];	/* SSL-style count; no pad */
	if(ss->alg){
		len -= SHA1dlen;
		if(len <= 0 || readn(ss->fd, digest, SHA1dlen) != SHA1dlen){
			werrstr("!SC_read missing sha1");
			return -1;
		}
		if(len > n || readn(ss->fd, buf, len) != len){
			werrstr("!SC_read missing data");
			return -1;
		}
		rc4(&ss->in.rc4, digest, SHA1dlen);
		rc4(&ss->in.rc4, buf, len);
		if(verify(ss->in.secret, buf, len, ss->in.seqno, digest) != 0){
			werrstr("!SC_read integrity check failed");
			return -1;
		}
	}else{
		if(len <= 0 || len > n){
			werrstr("!SC_read implausible record length");
			return -1;
		}
		if( (nr = readn(ss->fd, buf, len)) != len){
			werrstr("!SC_read expected %d bytes, but got %d", len, nr);
			return -1;
		}
	}
	ss->in.seqno++;
	return len;
}

static int
SC_write(SConn *conn, uchar *buf, int n)
{
	SS *ss = (SS*)(conn->chan);
	uchar count[2], digest[SHA1dlen], enc[Maxmsg+1];
	int len;

	if(n <= 0 || n > Maxmsg+1){
		werrstr("!SC_write invalid n %d", n);
		return -1;
	}
	len = n;
	if(ss->alg)
		len += SHA1dlen;
	count[0] = 0x80 | len>>8;
	count[1] = len;
	if(write(ss->fd, count, 2) != 2){
		werrstr("!SC_write invalid count");
		return -1;
	}
	if(ss->alg){
		hash(ss->out.secret, buf, n, ss->out.seqno, digest);
		rc4(&ss->out.rc4, digest, SHA1dlen);
		memcpy(enc, buf, n);
		rc4(&ss->out.rc4, enc, n);
		if(write(ss->fd, digest, SHA1dlen) != SHA1dlen ||
				write(ss->fd, enc, n) != n){
			werrstr("!SC_write error on send");
			return -1;
		}
	}else{
		if(write(ss->fd, buf, n) != n){
			werrstr("!SC_write error on send");
			return -1;
		}
	}
	ss->out.seqno++;
	return n;
}

static void
SC_free(SConn *conn)
{
	SS *ss = (SS*)(conn->chan);

	close(ss->fd);
	free(ss);
	free(conn);
}

SConn*
newSConn(int fd)
{
	SS *ss;
	SConn *conn;

	if(fd < 0)
		return nil;
	ss = (SS*)emalloc(sizeof(*ss));
	conn = (SConn*)emalloc(sizeof(*conn));
	ss->fd  = fd;
	ss->alg = 0;
	conn->chan = (void*)ss;
	conn->secretlen = SHA1dlen;
	conn->free = SC_free;
	conn->secret = SC_secret;
	conn->read = SC_read;
	conn->write = SC_write;
	return conn;
}

static void
writerr(SConn *conn, char *s)
{
	char buf[Maxmsg];

	snprint(buf, Maxmsg, "!%s", s);
	conn->write(conn, (uchar*)buf, strlen(buf));
}

static int
readstr(SConn *conn, char *s)
{
	int n;

	n = conn->read(conn, (uchar*)s, Maxmsg);
	if(n >= 0){
		s[n] = 0;
		if(s[0] == '!'){
			memmove(s, s+1, n);
			n = -1;
		}
	}else{
		strcpy(s, "read error");
	}
	return n;
}

static int
getfile(SConn *conn, uchar *key, int nkey)
{
	char *buf;
	int nbuf, n, nr, len;
	char s[Maxmsg+1], *gf, *p, *q;
	uchar skey[SHA1dlen], ib[Maxmsg+CHK], *ibr, *ibw;
	AESstate aes;
	DigestState *sha;

	gf = "factotum";
	memset(&aes, 0, sizeof aes);

	snprint(s, Maxmsg, "GET %s\n", gf);
	conn->write(conn, (uchar*)s, strlen(s));

	/* get file size */
	s[0] = '\0';
	if(readstr(conn, s) < 0){
		werrstr("secstore: %r");
		return -1;
	}
	if((len = atoi(s)) < 0){
		werrstr("secstore: remote file %s does not exist", gf);
		return -1;
	}else if(len > MAXFILESIZE){/*assert */
		werrstr("secstore: implausible file size %d for %s", len, gf);
		return -1;
	}

	ibr = ibw = ib;
	buf = nil;
	nbuf = 0;
	for(nr=0; nr < len;){
		if((n = conn->read(conn, ibw, Maxmsg)) <= 0){
			werrstr("secstore: empty file chunk n=%d nr=%d len=%d: %r", n, nr, len);
			return -1;
		}
		nr += n;
		ibw += n;
		if(!aes.setup){ /* first time, read 16 byte IV */
			if(n < 16){
				werrstr("secstore: no IV in file");
				return -1;
			}
			sha = sha1((uchar*)"aescbc file", 11, nil, nil);
			sha1(key, nkey, skey, sha);
			setupAESstate(&aes, skey, AESbsize, ibr);
			memset(skey, 0, sizeof skey);
			ibr += AESbsize;
			n -= AESbsize;
		}
		aesCBCdecrypt(ibw-n, n, &aes);
		n = ibw-ibr-CHK;
		if(n > 0){
			buf = erealloc(buf, nbuf+n+1);
			memmove(buf+nbuf, ibr, n);
			nbuf += n;
			ibr += n;
		}
		memmove(ib, ibr, ibw-ibr);
		ibw = ib + (ibw-ibr);
		ibr = ib;
	}
	n = ibw-ibr;
	if((n != CHK) || (memcmp(ib, "XXXXXXXXXXXXXXXX", CHK) != 0)){
		werrstr("secstore: decrypted file failed to authenticate!");
		free(buf);
		return -1;
	}
	if(nbuf == 0){
		werrstr("secstore got empty file");
		return -1;
	}
	buf[nbuf] = '\0';
	p = buf;
	n = 0;
	while(p){
		if(q = strchr(p, '\n'))
			*q++ = '\0';
		n++;
		if(ctlwrite(p) < 0){
			flog("secstore %s:%d: %r", gf, n);
			fprint(2, "secstore(%s) line %d: %r\n", gf, n);
		}
		p = q;
	}
	free(buf);
	return 0;
}

static char VERSION[] = "secstore";

typedef struct PAKparams{
	mpint *q, *p, *r, *g;
} PAKparams;

static PAKparams *pak;

/* This group was generated by the seed EB7B6E35F7CD37B511D96C67D6688CC4DD440E1E. */
static void
initPAKparams(void)
{
	if(pak)
		return;
	pak = (PAKparams*)emalloc(sizeof(*pak));
	pak->q = strtomp("E0F0EF284E10796C5A2A511E94748BA03C795C13", nil, 16, nil);
	pak->p = strtomp("C41CFBE4D4846F67A3DF7DE9921A49D3B42DC33728427AB159CEC8CBBD"
		"B12B5F0C244F1A734AEB9840804EA3C25036AD1B61AFF3ABBC247CD4B384224567A86"
		"3A6F020E7EE9795554BCD08ABAD7321AF27E1E92E3DB1C6E7E94FAAE590AE9C48F96D9"
		"3D178E809401ABE8A534A1EC44359733475A36A70C7B425125062B1142D", nil, 16, nil);
	pak->r = strtomp("DF310F4E54A5FEC5D86D3E14863921E834113E060F90052AD332B3241CEF"
		"2497EFA0303D6344F7C819691A0F9C4A773815AF8EAECFB7EC1D98F039F17A32A7E887"
		"D97251A927D093F44A55577F4D70444AEBD06B9B45695EC23962B175F266895C67D21"
		"C4656848614D888A4", nil, 16, nil);
	pak->g = strtomp("2F1C308DC46B9A44B52DF7DACCE1208CCEF72F69C743ADD4D2327173444"
		"ED6E65E074694246E07F9FD4AE26E0FDDD9F54F813C40CB9BCD4338EA6F242AB94CD41"
		"0E676C290368A16B1A3594877437E516C53A6EEE5493A038A017E955E218E7819734E3E"
		"2A6E0BAE08B14258F8C03CC1B30E0DDADFCF7CEDF0727684D3D255F1", nil, 16, nil);
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
static char *
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
static int
PAKclient(SConn *conn, char *C, char *pass, char **pS)
{
	char *mess, *mess2, *eol, *S, *hexmu, *ks, *hexm, *hexsigma = nil, *hexHi;
	char kc[2*SHA1dlen+1];
	uchar digest[SHA1dlen];
	int rc = -1, n;
	mpint *x, *m = mpnew(0), *mu = mpnew(0), *sigma = mpnew(0);
	mpint *H = mpnew(0), *Hi = mpnew(0);

	hexHi = PAK_Hi(C, pass, H, Hi);

	/* random 1<=x<=q-1; send C, m=g**x H */
	x = mprand(164, genrandom, nil);
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
	if(n < 0){/*assert */
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

int
secstorefetch(void)
{
	int rv = -1, fd;
	char s[Maxmsg+1];
	SConn *conn;
	char *pass, *sta;

	sta = nil;
	conn = nil;
	pass = readcons("secstore password", nil, 1);
	if(pass==nil || strlen(pass)==0){
		werrstr("cancel");
		goto Out;
	}
	if((fd = secdial()) < 0)
		goto Out;
	if((conn = newSConn(fd)) == nil)
		goto Out;
	if(PAKclient(conn, owner, pass, nil) < 0){
		werrstr("password mistyped?");
		goto Out;
	}
	if(readstr(conn, s) < 0)
		goto Out;
	if(strcmp(s, "STA") == 0){
		sta = readcons("STA PIN+SecureID", nil, 1);
		if(sta==nil || strlen(sta)==0){
			werrstr("cancel");
			goto Out;
		}
		if(strlen(sta) >= sizeof s - 3){
			werrstr("STA response too long");
			goto Out;
		}
		strcpy(s+3, sta);
		conn->write(conn, (uchar*)s, strlen(s));
		readstr(conn, s);
	}
	if(strcmp(s, "OK") !=0){
		werrstr("%s", s);
		goto Out;
	}
	if(getfile(conn, (uchar*)pass, strlen(pass)) < 0)
		goto Out;
	conn->write(conn, (uchar*)"BYE", 3);
	rv = 0;

Out:
	if(rv < 0)
		flog("secstorefetch: %r");
	if(conn)
		conn->free(conn);
	if(pass)
		free(pass);
	if(sta)
		free(sta);
	return rv;
}
