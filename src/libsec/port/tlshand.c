#include <u.h>
#include <libc.h>
#include <bio.h>
#include <auth.h>
#include <mp.h>
#include <libsec.h>

/* The main groups of functions are: */
/*		client/server - main handshake protocol definition */
/*		message functions - formating handshake messages */
/*		cipher choices - catalog of digest and encrypt algorithms */
/*		security functions - PKCS#1, sslHMAC, session keygen */
/*		general utility functions - malloc, serialization */
/* The handshake protocol builds on the TLS/SSL3 record layer protocol, */
/* which is implemented in kernel device #a.  See also /lib/rfc/rfc2246. */

enum {
	TLSFinishedLen = 12,
	SSL3FinishedLen = MD5dlen+SHA1dlen,
	MaxKeyData = 104,	/* amount of secret we may need */
	MaxChunk = 1<<14,
	RandomSize = 32,
	SidSize = 32,
	MasterSecretSize = 48,
	AQueue = 0,
	AFlush = 1
};

typedef struct TlsSec TlsSec;

typedef struct Bytes{
	int len;
	uchar data[1];  /* [len] */
} Bytes;

typedef struct Ints{
	int len;
	int data[1];  /* [len] */
} Ints;

typedef struct Algs{
	char *enc;
	char *digest;
	int nsecret;
	int tlsid;
	int ok;
} Algs;

typedef struct Finished{
	uchar verify[SSL3FinishedLen];
	int n;
} Finished;

typedef struct TlsConnection{
	TlsSec *sec;	/* security management goo */
	int hand, ctl;	/* record layer file descriptors */
	int erred;		/* set when tlsError called */
	int (*trace)(char*fmt, ...); /* for debugging */
	int version;	/* protocol we are speaking */
	int verset;		/* version has been set */
	int ver2hi;		/* server got a version 2 hello */
	int isClient;	/* is this the client or server? */
	Bytes *sid;		/* SessionID */
	Bytes *cert;	/* only last - no chain */

	Lock statelk;
	int state;		/* must be set using setstate */

	/* input buffer for handshake messages */
	uchar buf[MaxChunk+2048];
	uchar *rp, *ep;

	uchar crandom[RandomSize];	/* client random */
	uchar srandom[RandomSize];	/* server random */
	int clientVersion;	/* version in ClientHello */
	char *digest;	/* name of digest algorithm to use */
	char *enc;		/* name of encryption algorithm to use */
	int nsecret;	/* amount of secret data to init keys */

	/* for finished messages */
	MD5state	hsmd5;	/* handshake hash */
	SHAstate	hssha1;	/* handshake hash */
	Finished	finished;
} TlsConnection;

typedef struct Msg{
	int tag;
	union {
		struct {
			int version;
			uchar 	random[RandomSize];
			Bytes*	sid;
			Ints*	ciphers;
			Bytes*	compressors;
		} clientHello;
		struct {
			int version;
			uchar 	random[RandomSize];
			Bytes*	sid;
			int cipher;
			int compressor;
		} serverHello;
		struct {
			int ncert;
			Bytes **certs;
		} certificate;
		struct {
			Bytes *types;
			int nca;
			Bytes **cas;
		} certificateRequest;
		struct {
			Bytes *key;
		} clientKeyExchange;
		Finished finished;
	} u;
} Msg;

struct TlsSec{
	char *server;	/* name of remote; nil for server */
	int ok;	/* <0 killed; ==0 in progress; >0 reusable */
	RSApub *rsapub;
	AuthRpc *rpc;	/* factotum for rsa private key */
	uchar sec[MasterSecretSize];	/* master secret */
	uchar crandom[RandomSize];	/* client random */
	uchar srandom[RandomSize];	/* server random */
	int clientVers;		/* version in ClientHello */
	int vers;			/* final version */
	/* byte generation and handshake checksum */
	void (*prf)(uchar*, int, uchar*, int, char*, uchar*, int, uchar*, int);
	void (*setFinished)(TlsSec*, MD5state, SHAstate, uchar*, int);
	int nfin;
};


enum {
	TLSVersion = 0x0301,
	SSL3Version = 0x0300,
	ProtocolVersion = 0x0301,	/* maximum version we speak */
	MinProtoVersion = 0x0300,	/* limits on version we accept */
	MaxProtoVersion	= 0x03ff
};

/* handshake type */
enum {
	HHelloRequest,
	HClientHello,
	HServerHello,
	HSSL2ClientHello = 9,  /* local convention;  see devtls.c */
	HCertificate = 11,
	HServerKeyExchange,
	HCertificateRequest,
	HServerHelloDone,
	HCertificateVerify,
	HClientKeyExchange,
	HFinished = 20,
	HMax
};

/* alerts */
enum {
	ECloseNotify = 0,
	EUnexpectedMessage = 10,
	EBadRecordMac = 20,
	EDecryptionFailed = 21,
	ERecordOverflow = 22,
	EDecompressionFailure = 30,
	EHandshakeFailure = 40,
	ENoCertificate = 41,
	EBadCertificate = 42,
	EUnsupportedCertificate = 43,
	ECertificateRevoked = 44,
	ECertificateExpired = 45,
	ECertificateUnknown = 46,
	EIllegalParameter = 47,
	EUnknownCa = 48,
	EAccessDenied = 49,
	EDecodeError = 50,
	EDecryptError = 51,
	EExportRestriction = 60,
	EProtocolVersion = 70,
	EInsufficientSecurity = 71,
	EInternalError = 80,
	EUserCanceled = 90,
	ENoRenegotiation = 100,
	EMax = 256
};

/* cipher suites */
enum {
	TLS_NULL_WITH_NULL_NULL	 		= 0x0000,
	TLS_RSA_WITH_NULL_MD5 			= 0x0001,
	TLS_RSA_WITH_NULL_SHA 			= 0x0002,
	TLS_RSA_EXPORT_WITH_RC4_40_MD5 		= 0x0003,
	TLS_RSA_WITH_RC4_128_MD5 		= 0x0004,
	TLS_RSA_WITH_RC4_128_SHA 		= 0x0005,
	TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5	= 0X0006,
	TLS_RSA_WITH_IDEA_CBC_SHA 		= 0X0007,
	TLS_RSA_EXPORT_WITH_DES40_CBC_SHA	= 0X0008,
	TLS_RSA_WITH_DES_CBC_SHA		= 0X0009,
	TLS_RSA_WITH_3DES_EDE_CBC_SHA		= 0X000A,
	TLS_DH_DSS_EXPORT_WITH_DES40_CBC_SHA	= 0X000B,
	TLS_DH_DSS_WITH_DES_CBC_SHA		= 0X000C,
	TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA	= 0X000D,
	TLS_DH_RSA_EXPORT_WITH_DES40_CBC_SHA	= 0X000E,
	TLS_DH_RSA_WITH_DES_CBC_SHA		= 0X000F,
	TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA	= 0X0010,
	TLS_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA	= 0X0011,
	TLS_DHE_DSS_WITH_DES_CBC_SHA		= 0X0012,
	TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA	= 0X0013,	/* ZZZ must be implemented for tls1.0 compliance */
	TLS_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA	= 0X0014,
	TLS_DHE_RSA_WITH_DES_CBC_SHA		= 0X0015,
	TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA	= 0X0016,
	TLS_DH_anon_EXPORT_WITH_RC4_40_MD5	= 0x0017,
	TLS_DH_anon_WITH_RC4_128_MD5 		= 0x0018,
	TLS_DH_anon_EXPORT_WITH_DES40_CBC_SHA	= 0X0019,
	TLS_DH_anon_WITH_DES_CBC_SHA		= 0X001A,
	TLS_DH_anon_WITH_3DES_EDE_CBC_SHA	= 0X001B,

	TLS_RSA_WITH_AES_128_CBC_SHA		= 0X002f,	/* aes, aka rijndael with 128 bit blocks */
	TLS_DH_DSS_WITH_AES_128_CBC_SHA		= 0X0030,
	TLS_DH_RSA_WITH_AES_128_CBC_SHA		= 0X0031,
	TLS_DHE_DSS_WITH_AES_128_CBC_SHA	= 0X0032,
	TLS_DHE_RSA_WITH_AES_128_CBC_SHA	= 0X0033,
	TLS_DH_anon_WITH_AES_128_CBC_SHA	= 0X0034,
	TLS_RSA_WITH_AES_256_CBC_SHA		= 0X0035,
	TLS_DH_DSS_WITH_AES_256_CBC_SHA		= 0X0036,
	TLS_DH_RSA_WITH_AES_256_CBC_SHA		= 0X0037,
	TLS_DHE_DSS_WITH_AES_256_CBC_SHA	= 0X0038,
	TLS_DHE_RSA_WITH_AES_256_CBC_SHA	= 0X0039,
	TLS_DH_anon_WITH_AES_256_CBC_SHA	= 0X003A,
	CipherMax
};

/* compression methods */
enum {
	CompressionNull = 0,
	CompressionMax
};

static Algs cipherAlgs[] = {
	{"rc4_128", "md5",	2 * (16 + MD5dlen), TLS_RSA_WITH_RC4_128_MD5},
	{"rc4_128", "sha1",	2 * (16 + SHA1dlen), TLS_RSA_WITH_RC4_128_SHA},
	{"3des_ede_cbc","sha1",2*(4*8+SHA1dlen), TLS_RSA_WITH_3DES_EDE_CBC_SHA},
};

static uchar compressors[] = {
	CompressionNull,
};

static TlsConnection *tlsServer2(int ctl, int hand, uchar *cert, int ncert, int (*trace)(char*fmt, ...), PEMChain *chain);
static TlsConnection *tlsClient2(int ctl, int hand, uchar *csid, int ncsid, int (*trace)(char*fmt, ...));

static void	msgClear(Msg *m);
static char* msgPrint(char *buf, int n, Msg *m);
static int	msgRecv(TlsConnection *c, Msg *m);
static int	msgSend(TlsConnection *c, Msg *m, int act);
static void	tlsError(TlsConnection *c, int err, char *msg, ...);
/* #pragma	varargck argpos	tlsError 3*/
static int setVersion(TlsConnection *c, int version);
static int finishedMatch(TlsConnection *c, Finished *f);
static void tlsConnectionFree(TlsConnection *c);

static int setAlgs(TlsConnection *c, int a);
static int okCipher(Ints *cv);
static int okCompression(Bytes *cv);
static int initCiphers(void);
static Ints* makeciphers(void);

static TlsSec* tlsSecInits(int cvers, uchar *csid, int ncsid, uchar *crandom, uchar *ssid, int *nssid, uchar *srandom);
static int	tlsSecSecrets(TlsSec *sec, int vers, uchar *epm, int nepm, uchar *kd, int nkd);
static TlsSec*	tlsSecInitc(int cvers, uchar *crandom);
static int	tlsSecSecretc(TlsSec *sec, uchar *sid, int nsid, uchar *srandom, uchar *cert, int ncert, int vers, uchar **epm, int *nepm, uchar *kd, int nkd);
static int	tlsSecFinished(TlsSec *sec, MD5state md5, SHAstate sha1, uchar *fin, int nfin, int isclient);
static void	tlsSecOk(TlsSec *sec);
/* static void	tlsSecKill(TlsSec *sec); */
static void	tlsSecClose(TlsSec *sec);
static void	setMasterSecret(TlsSec *sec, Bytes *pm);
static void	serverMasterSecret(TlsSec *sec, uchar *epm, int nepm);
static void	setSecrets(TlsSec *sec, uchar *kd, int nkd);
static int	clientMasterSecret(TlsSec *sec, RSApub *pub, uchar **epm, int *nepm);
static Bytes *pkcs1_encrypt(Bytes* data, RSApub* key, int blocktype);
static Bytes *pkcs1_decrypt(TlsSec *sec, uchar *epm, int nepm);
static void	tlsSetFinished(TlsSec *sec, MD5state hsmd5, SHAstate hssha1, uchar *finished, int isClient);
static void	sslSetFinished(TlsSec *sec, MD5state hsmd5, SHAstate hssha1, uchar *finished, int isClient);
static void	sslPRF(uchar *buf, int nbuf, uchar *key, int nkey, char *label,
			uchar *seed0, int nseed0, uchar *seed1, int nseed1);
static int setVers(TlsSec *sec, int version);

static AuthRpc* factotum_rsa_open(uchar *cert, int certlen);
static mpint* factotum_rsa_decrypt(AuthRpc *rpc, mpint *cipher);
static void factotum_rsa_close(AuthRpc*rpc);

static void* emalloc(int);
static void* erealloc(void*, int);
static void put32(uchar *p, u32int);
static void put24(uchar *p, int);
static void put16(uchar *p, int);
/* static u32int get32(uchar *p); */
static int get24(uchar *p);
static int get16(uchar *p);
static Bytes* newbytes(int len);
static Bytes* makebytes(uchar* buf, int len);
static void freebytes(Bytes* b);
static Ints* newints(int len);
/* static Ints* makeints(int* buf, int len); */
static void freeints(Ints* b);

/*================= client/server ======================== */

/*	push TLS onto fd, returning new (application) file descriptor */
/*		or -1 if error. */
int
tlsServer(int fd, TLSconn *conn)
{
	char buf[8];
	char dname[64];
	int n, data, ctl, hand;
	TlsConnection *tls;

	if(conn == nil)
		return -1;
	ctl = open("#a/tls/clone", ORDWR);
	if(ctl < 0)
		return -1;
	n = read(ctl, buf, sizeof(buf)-1);
	if(n < 0){
		close(ctl);
		return -1;
	}
	buf[n] = 0;
	sprint(conn->dir, "#a/tls/%s", buf);
	sprint(dname, "#a/tls/%s/hand", buf);
	hand = open(dname, ORDWR);
	if(hand < 0){
		close(ctl);
		return -1;
	}
	fprint(ctl, "fd %d 0x%x", fd, ProtocolVersion);
	tls = tlsServer2(ctl, hand, conn->cert, conn->certlen, conn->trace, conn->chain);
	sprint(dname, "#a/tls/%s/data", buf);
	data = open(dname, ORDWR);
	close(fd);
	close(hand);
	close(ctl);
	if(data < 0){
		return -1;
	}
	if(tls == nil){
		close(data);
		return -1;
	}
	if(conn->cert)
		free(conn->cert);
	conn->cert = 0;  /* client certificates are not yet implemented */
	conn->certlen = 0;
	conn->sessionIDlen = tls->sid->len;
	conn->sessionID = emalloc(conn->sessionIDlen);
	memcpy(conn->sessionID, tls->sid->data, conn->sessionIDlen);
	tlsConnectionFree(tls);
	return data;
}

/*	push TLS onto fd, returning new (application) file descriptor */
/*		or -1 if error. */
int
tlsClient(int fd, TLSconn *conn)
{
	char buf[8];
	char dname[64];
	int n, data, ctl, hand;
	TlsConnection *tls;

	if(!conn)
		return -1;
	ctl = open("#a/tls/clone", ORDWR);
	if(ctl < 0)
		return -1;
	n = read(ctl, buf, sizeof(buf)-1);
	if(n < 0){
		close(ctl);
		return -1;
	}
	buf[n] = 0;
	sprint(conn->dir, "#a/tls/%s", buf);
	sprint(dname, "#a/tls/%s/hand", buf);
	hand = open(dname, ORDWR);
	if(hand < 0){
		close(ctl);
		return -1;
	}
	sprint(dname, "#a/tls/%s/data", buf);
	data = open(dname, ORDWR);
	if(data < 0)
		return -1;
	fprint(ctl, "fd %d 0x%x", fd, ProtocolVersion);
	tls = tlsClient2(ctl, hand, conn->sessionID, conn->sessionIDlen, conn->trace);
	close(fd);
	close(hand);
	close(ctl);
	if(tls == nil){
		close(data);
		return -1;
	}
	conn->certlen = tls->cert->len;
	conn->cert = emalloc(conn->certlen);
	memcpy(conn->cert, tls->cert->data, conn->certlen);
	conn->sessionIDlen = tls->sid->len;
	conn->sessionID = emalloc(conn->sessionIDlen);
	memcpy(conn->sessionID, tls->sid->data, conn->sessionIDlen);
	tlsConnectionFree(tls);
	return data;
}

static int
countchain(PEMChain *p)
{
	int i = 0;

	while (p) {
		i++;
		p = p->next;
	}
	return i;
}

static TlsConnection *
tlsServer2(int ctl, int hand, uchar *cert, int ncert, int (*trace)(char*fmt, ...), PEMChain *chp)
{
	TlsConnection *c;
	Msg m;
	Bytes *csid;
	uchar sid[SidSize], kd[MaxKeyData];
	char *secrets;
	int cipher, compressor, nsid, rv, numcerts, i;

	if(trace)
		trace("tlsServer2\n");
	if(!initCiphers())
		return nil;
	c = emalloc(sizeof(TlsConnection));
	c->ctl = ctl;
	c->hand = hand;
	c->trace = trace;
	c->version = ProtocolVersion;

	memset(&m, 0, sizeof(m));
	if(!msgRecv(c, &m)){
		if(trace)
			trace("initial msgRecv failed\n");
		goto Err;
	}
	if(m.tag != HClientHello) {
		tlsError(c, EUnexpectedMessage, "expected a client hello");
		goto Err;
	}
	c->clientVersion = m.u.clientHello.version;
	if(trace)
		trace("ClientHello version %x\n", c->clientVersion);
	if(setVersion(c, m.u.clientHello.version) < 0) {
		tlsError(c, EIllegalParameter, "incompatible version");
		goto Err;
	}

	memmove(c->crandom, m.u.clientHello.random, RandomSize);
	cipher = okCipher(m.u.clientHello.ciphers);
	if(cipher < 0) {
		/* reply with EInsufficientSecurity if we know that's the case */
		if(cipher == -2)
			tlsError(c, EInsufficientSecurity, "cipher suites too weak");
		else
			tlsError(c, EHandshakeFailure, "no matching cipher suite");
		goto Err;
	}
	if(!setAlgs(c, cipher)){
		tlsError(c, EHandshakeFailure, "no matching cipher suite");
		goto Err;
	}
	compressor = okCompression(m.u.clientHello.compressors);
	if(compressor < 0) {
		tlsError(c, EHandshakeFailure, "no matching compressor");
		goto Err;
	}

	csid = m.u.clientHello.sid;
	if(trace)
		trace("  cipher %d, compressor %d, csidlen %d\n", cipher, compressor, csid->len);
	c->sec = tlsSecInits(c->clientVersion, csid->data, csid->len, c->crandom, sid, &nsid, c->srandom);
	if(c->sec == nil){
		tlsError(c, EHandshakeFailure, "can't initialize security: %r");
		goto Err;
	}
	c->sec->rpc = factotum_rsa_open(cert, ncert);
	if(c->sec->rpc == nil){
		tlsError(c, EHandshakeFailure, "factotum_rsa_open: %r");
		goto Err;
	}
	c->sec->rsapub = X509toRSApub(cert, ncert, nil, 0);
	msgClear(&m);

	m.tag = HServerHello;
	m.u.serverHello.version = c->version;
	memmove(m.u.serverHello.random, c->srandom, RandomSize);
	m.u.serverHello.cipher = cipher;
	m.u.serverHello.compressor = compressor;
	c->sid = makebytes(sid, nsid);
	m.u.serverHello.sid = makebytes(c->sid->data, c->sid->len);
	if(!msgSend(c, &m, AQueue))
		goto Err;
	msgClear(&m);

	m.tag = HCertificate;
	numcerts = countchain(chp);
	m.u.certificate.ncert = 1 + numcerts;
	m.u.certificate.certs = emalloc(m.u.certificate.ncert * sizeof(Bytes));
	m.u.certificate.certs[0] = makebytes(cert, ncert);
	for (i = 0; i < numcerts && chp; i++, chp = chp->next)
		m.u.certificate.certs[i+1] = makebytes(chp->pem, chp->pemlen);
	if(!msgSend(c, &m, AQueue))
		goto Err;
	msgClear(&m);

	m.tag = HServerHelloDone;
	if(!msgSend(c, &m, AFlush))
		goto Err;
	msgClear(&m);

	if(!msgRecv(c, &m))
		goto Err;
	if(m.tag != HClientKeyExchange) {
		tlsError(c, EUnexpectedMessage, "expected a client key exchange");
		goto Err;
	}
	if(tlsSecSecrets(c->sec, c->version, m.u.clientKeyExchange.key->data, m.u.clientKeyExchange.key->len, kd, c->nsecret) < 0){
		tlsError(c, EHandshakeFailure, "couldn't set secrets: %r");
		goto Err;
	}
	if(trace)
		trace("tls secrets\n");
	secrets = (char*)emalloc(2*c->nsecret);
	enc64(secrets, 2*c->nsecret, kd, c->nsecret);
	rv = fprint(c->ctl, "secret %s %s 0 %s", c->digest, c->enc, secrets);
	memset(secrets, 0, 2*c->nsecret);
	free(secrets);
	memset(kd, 0, c->nsecret);
	if(rv < 0){
		tlsError(c, EHandshakeFailure, "can't set keys: %r");
		goto Err;
	}
	msgClear(&m);

	/* no CertificateVerify; skip to Finished */
	if(tlsSecFinished(c->sec, c->hsmd5, c->hssha1, c->finished.verify, c->finished.n, 1) < 0){
		tlsError(c, EInternalError, "can't set finished: %r");
		goto Err;
	}
	if(!msgRecv(c, &m))
		goto Err;
	if(m.tag != HFinished) {
		tlsError(c, EUnexpectedMessage, "expected a finished");
		goto Err;
	}
	if(!finishedMatch(c, &m.u.finished)) {
		tlsError(c, EHandshakeFailure, "finished verification failed");
		goto Err;
	}
	msgClear(&m);

	/* change cipher spec */
	if(fprint(c->ctl, "changecipher") < 0){
		tlsError(c, EInternalError, "can't enable cipher: %r");
		goto Err;
	}

	if(tlsSecFinished(c->sec, c->hsmd5, c->hssha1, c->finished.verify, c->finished.n, 0) < 0){
		tlsError(c, EInternalError, "can't set finished: %r");
		goto Err;
	}
	m.tag = HFinished;
	m.u.finished = c->finished;
	if(!msgSend(c, &m, AFlush))
		goto Err;
	msgClear(&m);
	if(trace)
		trace("tls finished\n");

	if(fprint(c->ctl, "opened") < 0)
		goto Err;
	tlsSecOk(c->sec);
	return c;

Err:
	msgClear(&m);
	tlsConnectionFree(c);
	return 0;
}

static TlsConnection *
tlsClient2(int ctl, int hand, uchar *csid, int ncsid, int (*trace)(char*fmt, ...))
{
	TlsConnection *c;
	Msg m;
	uchar kd[MaxKeyData], *epm;
	char *secrets;
	int creq, nepm, rv;

	if(!initCiphers())
		return nil;
	epm = nil;
	c = emalloc(sizeof(TlsConnection));
	c->version = ProtocolVersion;
	c->ctl = ctl;
	c->hand = hand;
	c->trace = trace;
	c->isClient = 1;
	c->clientVersion = c->version;

	c->sec = tlsSecInitc(c->clientVersion, c->crandom);
	if(c->sec == nil)
		goto Err;

	/* client hello */
	memset(&m, 0, sizeof(m));
	m.tag = HClientHello;
	m.u.clientHello.version = c->clientVersion;
	memmove(m.u.clientHello.random, c->crandom, RandomSize);
	m.u.clientHello.sid = makebytes(csid, ncsid);
	m.u.clientHello.ciphers = makeciphers();
	m.u.clientHello.compressors = makebytes(compressors,sizeof(compressors));
	if(!msgSend(c, &m, AFlush))
		goto Err;
	msgClear(&m);

	/* server hello */
	if(!msgRecv(c, &m))
		goto Err;
	if(m.tag != HServerHello) {
		tlsError(c, EUnexpectedMessage, "expected a server hello");
		goto Err;
	}
	if(setVersion(c, m.u.serverHello.version) < 0) {
		tlsError(c, EIllegalParameter, "incompatible version %r");
		goto Err;
	}
	memmove(c->srandom, m.u.serverHello.random, RandomSize);
	c->sid = makebytes(m.u.serverHello.sid->data, m.u.serverHello.sid->len);
	if(c->sid->len != 0 && c->sid->len != SidSize) {
		tlsError(c, EIllegalParameter, "invalid server session identifier");
		goto Err;
	}
	if(!setAlgs(c, m.u.serverHello.cipher)) {
		tlsError(c, EIllegalParameter, "invalid cipher suite");
		goto Err;
	}
	if(m.u.serverHello.compressor != CompressionNull) {
		tlsError(c, EIllegalParameter, "invalid compression");
		goto Err;
	}
	msgClear(&m);

	/* certificate */
	if(!msgRecv(c, &m) || m.tag != HCertificate) {
		tlsError(c, EUnexpectedMessage, "expected a certificate");
		goto Err;
	}
	if(m.u.certificate.ncert < 1) {
		tlsError(c, EIllegalParameter, "runt certificate");
		goto Err;
	}
	c->cert = makebytes(m.u.certificate.certs[0]->data, m.u.certificate.certs[0]->len);
	msgClear(&m);

	/* server key exchange (optional) */
	if(!msgRecv(c, &m))
		goto Err;
	if(m.tag == HServerKeyExchange) {
		tlsError(c, EUnexpectedMessage, "got an server key exchange");
		goto Err;
		/* If implementing this later, watch out for rollback attack */
		/* described in Wagner Schneier 1996, section 4.4. */
	}

	/* certificate request (optional) */
	creq = 0;
	if(m.tag == HCertificateRequest) {
		creq = 1;
		msgClear(&m);
		if(!msgRecv(c, &m))
			goto Err;
	}

	if(m.tag != HServerHelloDone) {
		tlsError(c, EUnexpectedMessage, "expected a server hello done");
		goto Err;
	}
	msgClear(&m);

	if(tlsSecSecretc(c->sec, c->sid->data, c->sid->len, c->srandom,
			c->cert->data, c->cert->len, c->version, &epm, &nepm,
			kd, c->nsecret) < 0){
		tlsError(c, EBadCertificate, "invalid x509/rsa certificate");
		goto Err;
	}
	secrets = (char*)emalloc(2*c->nsecret);
	enc64(secrets, 2*c->nsecret, kd, c->nsecret);
	rv = fprint(c->ctl, "secret %s %s 1 %s", c->digest, c->enc, secrets);
	memset(secrets, 0, 2*c->nsecret);
	free(secrets);
	memset(kd, 0, c->nsecret);
	if(rv < 0){
		tlsError(c, EHandshakeFailure, "can't set keys: %r");
		goto Err;
	}

	if(creq) {
		/* send a zero length certificate */
		m.tag = HCertificate;
		if(!msgSend(c, &m, AFlush))
			goto Err;
		msgClear(&m);
	}

	/* client key exchange */
	m.tag = HClientKeyExchange;
	m.u.clientKeyExchange.key = makebytes(epm, nepm);
	free(epm);
	epm = nil;
	if(m.u.clientKeyExchange.key == nil) {
		tlsError(c, EHandshakeFailure, "can't set secret: %r");
		goto Err;
	}
	if(!msgSend(c, &m, AFlush))
		goto Err;
	msgClear(&m);

	/* change cipher spec */
	if(fprint(c->ctl, "changecipher") < 0){
		tlsError(c, EInternalError, "can't enable cipher: %r");
		goto Err;
	}

	/* Cipherchange must occur immediately before Finished to avoid */
	/* potential hole;  see section 4.3 of Wagner Schneier 1996. */
	if(tlsSecFinished(c->sec, c->hsmd5, c->hssha1, c->finished.verify, c->finished.n, 1) < 0){
		tlsError(c, EInternalError, "can't set finished 1: %r");
		goto Err;
	}
	m.tag = HFinished;
	m.u.finished = c->finished;

	if(!msgSend(c, &m, AFlush)) {
		fprint(2, "tlsClient nepm=%d\n", nepm);
		tlsError(c, EInternalError, "can't flush after client Finished: %r");
		goto Err;
	}
	msgClear(&m);

	if(tlsSecFinished(c->sec, c->hsmd5, c->hssha1, c->finished.verify, c->finished.n, 0) < 0){
		fprint(2, "tlsClient nepm=%d\n", nepm);
		tlsError(c, EInternalError, "can't set finished 0: %r");
		goto Err;
	}
	if(!msgRecv(c, &m)) {
		fprint(2, "tlsClient nepm=%d\n", nepm);
		tlsError(c, EInternalError, "can't read server Finished: %r");
		goto Err;
	}
	if(m.tag != HFinished) {
		fprint(2, "tlsClient nepm=%d\n", nepm);
		tlsError(c, EUnexpectedMessage, "expected a Finished msg from server");
		goto Err;
	}

	if(!finishedMatch(c, &m.u.finished)) {
		tlsError(c, EHandshakeFailure, "finished verification failed");
		goto Err;
	}
	msgClear(&m);

	if(fprint(c->ctl, "opened") < 0){
		if(trace)
			trace("unable to do final open: %r\n");
		goto Err;
	}
	tlsSecOk(c->sec);
	return c;

Err:
	free(epm);
	msgClear(&m);
	tlsConnectionFree(c);
	return 0;
}


/*================= message functions ======================== */

static uchar sendbuf[9000], *sendp;

static int
msgSend(TlsConnection *c, Msg *m, int act)
{
	uchar *p; /* sendp = start of new message;  p = write pointer */
	int nn, n, i;

	if(sendp == nil)
		sendp = sendbuf;
	p = sendp;
	if(c->trace)
		c->trace("send %s", msgPrint((char*)p, (sizeof sendbuf) - (p-sendbuf), m));

	p[0] = m->tag;	/* header - fill in size later */
	p += 4;

	switch(m->tag) {
	default:
		tlsError(c, EInternalError, "can't encode a %d", m->tag);
		goto Err;
	case HClientHello:
		/* version */
		put16(p, m->u.clientHello.version);
		p += 2;

		/* random */
		memmove(p, m->u.clientHello.random, RandomSize);
		p += RandomSize;

		/* sid */
		n = m->u.clientHello.sid->len;
		assert(n < 256);
		p[0] = n;
		memmove(p+1, m->u.clientHello.sid->data, n);
		p += n+1;

		n = m->u.clientHello.ciphers->len;
		assert(n > 0 && n < 200);
		put16(p, n*2);
		p += 2;
		for(i=0; i<n; i++) {
			put16(p, m->u.clientHello.ciphers->data[i]);
			p += 2;
		}

		n = m->u.clientHello.compressors->len;
		assert(n > 0);
		p[0] = n;
		memmove(p+1, m->u.clientHello.compressors->data, n);
		p += n+1;
		break;
	case HServerHello:
		put16(p, m->u.serverHello.version);
		p += 2;

		/* random */
		memmove(p, m->u.serverHello.random, RandomSize);
		p += RandomSize;

		/* sid */
		n = m->u.serverHello.sid->len;
		assert(n < 256);
		p[0] = n;
		memmove(p+1, m->u.serverHello.sid->data, n);
		p += n+1;

		put16(p, m->u.serverHello.cipher);
		p += 2;
		p[0] = m->u.serverHello.compressor;
		p += 1;
		break;
	case HServerHelloDone:
		break;
	case HCertificate:
		nn = 0;
		for(i = 0; i < m->u.certificate.ncert; i++)
			nn += 3 + m->u.certificate.certs[i]->len;
		if(p + 3 + nn - sendbuf > sizeof(sendbuf)) {
			tlsError(c, EInternalError, "output buffer too small for certificate");
			goto Err;
		}
		put24(p, nn);
		p += 3;
		for(i = 0; i < m->u.certificate.ncert; i++){
			put24(p, m->u.certificate.certs[i]->len);
			p += 3;
			memmove(p, m->u.certificate.certs[i]->data, m->u.certificate.certs[i]->len);
			p += m->u.certificate.certs[i]->len;
		}
		break;
	case HClientKeyExchange:
		n = m->u.clientKeyExchange.key->len;
		if(c->version != SSL3Version){
			put16(p, n);
			p += 2;
		}
		memmove(p, m->u.clientKeyExchange.key->data, n);
		p += n;
		break;
	case HFinished:
		memmove(p, m->u.finished.verify, m->u.finished.n);
		p += m->u.finished.n;
		break;
	}

	/* go back and fill in size */
	n = p-sendp;
	assert(p <= sendbuf+sizeof(sendbuf));
	put24(sendp+1, n-4);

	/* remember hash of Handshake messages */
	if(m->tag != HHelloRequest) {
		md5(sendp, n, 0, &c->hsmd5);
		sha1(sendp, n, 0, &c->hssha1);
	}

	sendp = p;
	if(act == AFlush){
		sendp = sendbuf;
		if(write(c->hand, sendbuf, p-sendbuf) < 0){
			fprint(2, "write error: %r\n");
			goto Err;
		}
	}
	msgClear(m);
	return 1;
Err:
	msgClear(m);
	return 0;
}

static uchar*
tlsReadN(TlsConnection *c, int n)
{
	uchar *p;
	int nn, nr;

	nn = c->ep - c->rp;
	if(nn < n){
		if(c->rp != c->buf){
			memmove(c->buf, c->rp, nn);
			c->rp = c->buf;
			c->ep = &c->buf[nn];
		}
		for(; nn < n; nn += nr) {
			nr = read(c->hand, &c->rp[nn], n - nn);
			if(nr <= 0)
				return nil;
			c->ep += nr;
		}
	}
	p = c->rp;
	c->rp += n;
	return p;
}

static int
msgRecv(TlsConnection *c, Msg *m)
{
	uchar *p;
	int type, n, nn, i, nsid, nrandom, nciph;

	for(;;) {
		p = tlsReadN(c, 4);
		if(p == nil)
			return 0;
		type = p[0];
		n = get24(p+1);

		if(type != HHelloRequest)
			break;
		if(n != 0) {
			tlsError(c, EDecodeError, "invalid hello request during handshake");
			return 0;
		}
	}

	if(n > sizeof(c->buf)) {
		tlsError(c, EDecodeError, "handshake message too long %d %d", n, sizeof(c->buf));
		return 0;
	}

	if(type == HSSL2ClientHello){
		/* Cope with an SSL3 ClientHello expressed in SSL2 record format.
			This is sent by some clients that we must interoperate
			with, such as Java's JSSE and Microsoft's Internet Explorer. */
		p = tlsReadN(c, n);
		if(p == nil)
			return 0;
		md5(p, n, 0, &c->hsmd5);
		sha1(p, n, 0, &c->hssha1);
		m->tag = HClientHello;
		if(n < 22)
			goto Short;
		m->u.clientHello.version = get16(p+1);
		p += 3;
		n -= 3;
		nn = get16(p); /* cipher_spec_len */
		nsid = get16(p + 2);
		nrandom = get16(p + 4);
		p += 6;
		n -= 6;
		if(nsid != 0 	/* no sid's, since shouldn't restart using ssl2 header */
				|| nrandom < 16 || nn % 3)
			goto Err;
		if(c->trace && (n - nrandom != nn))
			c->trace("n-nrandom!=nn: n=%d nrandom=%d nn=%d\n", n, nrandom, nn);
		/* ignore ssl2 ciphers and look for {0x00, ssl3 cipher} */
		nciph = 0;
		for(i = 0; i < nn; i += 3)
			if(p[i] == 0)
				nciph++;
		m->u.clientHello.ciphers = newints(nciph);
		nciph = 0;
		for(i = 0; i < nn; i += 3)
			if(p[i] == 0)
				m->u.clientHello.ciphers->data[nciph++] = get16(&p[i + 1]);
		p += nn;
		m->u.clientHello.sid = makebytes(nil, 0);
		if(nrandom > RandomSize)
			nrandom = RandomSize;
		memset(m->u.clientHello.random, 0, RandomSize - nrandom);
		memmove(&m->u.clientHello.random[RandomSize - nrandom], p, nrandom);
		m->u.clientHello.compressors = newbytes(1);
		m->u.clientHello.compressors->data[0] = CompressionNull;
		goto Ok;
	}

	md5(p, 4, 0, &c->hsmd5);
	sha1(p, 4, 0, &c->hssha1);

	p = tlsReadN(c, n);
	if(p == nil)
		return 0;

	md5(p, n, 0, &c->hsmd5);
	sha1(p, n, 0, &c->hssha1);

	m->tag = type;

	switch(type) {
	default:
		tlsError(c, EUnexpectedMessage, "can't decode a %d", type);
		goto Err;
	case HClientHello:
		if(n < 2)
			goto Short;
		m->u.clientHello.version = get16(p);
		p += 2;
		n -= 2;

		if(n < RandomSize)
			goto Short;
		memmove(m->u.clientHello.random, p, RandomSize);
		p += RandomSize;
		n -= RandomSize;
		if(n < 1 || n < p[0]+1)
			goto Short;
		m->u.clientHello.sid = makebytes(p+1, p[0]);
		p += m->u.clientHello.sid->len+1;
		n -= m->u.clientHello.sid->len+1;

		if(n < 2)
			goto Short;
		nn = get16(p);
		p += 2;
		n -= 2;

		if((nn & 1) || n < nn || nn < 2)
			goto Short;
		m->u.clientHello.ciphers = newints(nn >> 1);
		for(i = 0; i < nn; i += 2)
			m->u.clientHello.ciphers->data[i >> 1] = get16(&p[i]);
		p += nn;
		n -= nn;

		if(n < 1 || n < p[0]+1 || p[0] == 0)
			goto Short;
		nn = p[0];
		m->u.clientHello.compressors = newbytes(nn);
		memmove(m->u.clientHello.compressors->data, p+1, nn);
		n -= nn + 1;
		break;
	case HServerHello:
		if(n < 2)
			goto Short;
		m->u.serverHello.version = get16(p);
		p += 2;
		n -= 2;

		if(n < RandomSize)
			goto Short;
		memmove(m->u.serverHello.random, p, RandomSize);
		p += RandomSize;
		n -= RandomSize;

		if(n < 1 || n < p[0]+1)
			goto Short;
		m->u.serverHello.sid = makebytes(p+1, p[0]);
		p += m->u.serverHello.sid->len+1;
		n -= m->u.serverHello.sid->len+1;

		if(n < 3)
			goto Short;
		m->u.serverHello.cipher = get16(p);
		m->u.serverHello.compressor = p[2];
		n -= 3;
		break;
	case HCertificate:
		if(n < 3)
			goto Short;
		nn = get24(p);
		p += 3;
		n -= 3;
		if(n != nn)
			goto Short;
		/* certs */
		i = 0;
		while(n > 0) {
			if(n < 3)
				goto Short;
			nn = get24(p);
			p += 3;
			n -= 3;
			if(nn > n)
				goto Short;
			m->u.certificate.ncert = i+1;
			m->u.certificate.certs = erealloc(m->u.certificate.certs, (i+1)*sizeof(Bytes));
			m->u.certificate.certs[i] = makebytes(p, nn);
			p += nn;
			n -= nn;
			i++;
		}
		break;
	case HCertificateRequest:
		if(n < 2)
			goto Short;
		nn = get16(p);
		p += 2;
		n -= 2;
		if(nn < 1 || nn > n)
			goto Short;
		m->u.certificateRequest.types = makebytes(p, nn);
		nn = get24(p);
		p += 3;
		n -= 3;
		if(nn == 0 || n != nn)
			goto Short;
		/* cas */
		i = 0;
		while(n > 0) {
			if(n < 2)
				goto Short;
			nn = get16(p);
			p += 2;
			n -= 2;
			if(nn < 1 || nn > n)
				goto Short;
			m->u.certificateRequest.nca = i+1;
			m->u.certificateRequest.cas = erealloc(m->u.certificateRequest.cas, (i+1)*sizeof(Bytes));
			m->u.certificateRequest.cas[i] = makebytes(p, nn);
			p += nn;
			n -= nn;
			i++;
		}
		break;
	case HServerHelloDone:
		break;
	case HClientKeyExchange:
		/*
		 * this message depends upon the encryption selected
		 * assume rsa.
		 */
		if(c->version == SSL3Version)
			nn = n;
		else{
			if(n < 2)
				goto Short;
			nn = get16(p);
			p += 2;
			n -= 2;
		}
		if(n < nn)
			goto Short;
		m->u.clientKeyExchange.key = makebytes(p, nn);
		n -= nn;
		break;
	case HFinished:
		m->u.finished.n = c->finished.n;
		if(n < m->u.finished.n)
			goto Short;
		memmove(m->u.finished.verify, p, m->u.finished.n);
		n -= m->u.finished.n;
		break;
	}

	if(type != HClientHello && n != 0)
		goto Short;
Ok:
	if(c->trace){
		char buf[8000];
		c->trace("recv %s", msgPrint(buf, sizeof buf, m));
	}
	return 1;
Short:
	tlsError(c, EDecodeError, "handshake message has invalid length");
Err:
	msgClear(m);
	return 0;
}

static void
msgClear(Msg *m)
{
	int i;

	switch(m->tag) {
	default:
		sysfatal("msgClear: unknown message type: %d\n", m->tag);
	case HHelloRequest:
		break;
	case HClientHello:
		freebytes(m->u.clientHello.sid);
		freeints(m->u.clientHello.ciphers);
		freebytes(m->u.clientHello.compressors);
		break;
	case HServerHello:
		freebytes(m->u.clientHello.sid);
		break;
	case HCertificate:
		for(i=0; i<m->u.certificate.ncert; i++)
			freebytes(m->u.certificate.certs[i]);
		free(m->u.certificate.certs);
		break;
	case HCertificateRequest:
		freebytes(m->u.certificateRequest.types);
		for(i=0; i<m->u.certificateRequest.nca; i++)
			freebytes(m->u.certificateRequest.cas[i]);
		free(m->u.certificateRequest.cas);
		break;
	case HServerHelloDone:
		break;
	case HClientKeyExchange:
		freebytes(m->u.clientKeyExchange.key);
		break;
	case HFinished:
		break;
	}
	memset(m, 0, sizeof(Msg));
}

static char *
bytesPrint(char *bs, char *be, char *s0, Bytes *b, char *s1)
{
	int i;

	if(s0)
		bs = seprint(bs, be, "%s", s0);
	bs = seprint(bs, be, "[");
	if(b == nil)
		bs = seprint(bs, be, "nil");
	else
		for(i=0; i<b->len; i++)
			bs = seprint(bs, be, "%.2x ", b->data[i]);
	bs = seprint(bs, be, "]");
	if(s1)
		bs = seprint(bs, be, "%s", s1);
	return bs;
}

static char *
intsPrint(char *bs, char *be, char *s0, Ints *b, char *s1)
{
	int i;

	if(s0)
		bs = seprint(bs, be, "%s", s0);
	bs = seprint(bs, be, "[");
	if(b == nil)
		bs = seprint(bs, be, "nil");
	else
		for(i=0; i<b->len; i++)
			bs = seprint(bs, be, "%x ", b->data[i]);
	bs = seprint(bs, be, "]");
	if(s1)
		bs = seprint(bs, be, "%s", s1);
	return bs;
}

static char*
msgPrint(char *buf, int n, Msg *m)
{
	int i;
	char *bs = buf, *be = buf+n;

	switch(m->tag) {
	default:
		bs = seprint(bs, be, "unknown %d\n", m->tag);
		break;
	case HClientHello:
		bs = seprint(bs, be, "ClientHello\n");
		bs = seprint(bs, be, "\tversion: %.4x\n", m->u.clientHello.version);
		bs = seprint(bs, be, "\trandom: ");
		for(i=0; i<RandomSize; i++)
			bs = seprint(bs, be, "%.2x", m->u.clientHello.random[i]);
		bs = seprint(bs, be, "\n");
		bs = bytesPrint(bs, be, "\tsid: ", m->u.clientHello.sid, "\n");
		bs = intsPrint(bs, be, "\tciphers: ", m->u.clientHello.ciphers, "\n");
		bs = bytesPrint(bs, be, "\tcompressors: ", m->u.clientHello.compressors, "\n");
		break;
	case HServerHello:
		bs = seprint(bs, be, "ServerHello\n");
		bs = seprint(bs, be, "\tversion: %.4x\n", m->u.serverHello.version);
		bs = seprint(bs, be, "\trandom: ");
		for(i=0; i<RandomSize; i++)
			bs = seprint(bs, be, "%.2x", m->u.serverHello.random[i]);
		bs = seprint(bs, be, "\n");
		bs = bytesPrint(bs, be, "\tsid: ", m->u.serverHello.sid, "\n");
		bs = seprint(bs, be, "\tcipher: %.4x\n", m->u.serverHello.cipher);
		bs = seprint(bs, be, "\tcompressor: %.2x\n", m->u.serverHello.compressor);
		break;
	case HCertificate:
		bs = seprint(bs, be, "Certificate\n");
		for(i=0; i<m->u.certificate.ncert; i++)
			bs = bytesPrint(bs, be, "\t", m->u.certificate.certs[i], "\n");
		break;
	case HCertificateRequest:
		bs = seprint(bs, be, "CertificateRequest\n");
		bs = bytesPrint(bs, be, "\ttypes: ", m->u.certificateRequest.types, "\n");
		bs = seprint(bs, be, "\tcertificateauthorities\n");
		for(i=0; i<m->u.certificateRequest.nca; i++)
			bs = bytesPrint(bs, be, "\t\t", m->u.certificateRequest.cas[i], "\n");
		break;
	case HServerHelloDone:
		bs = seprint(bs, be, "ServerHelloDone\n");
		break;
	case HClientKeyExchange:
		bs = seprint(bs, be, "HClientKeyExchange\n");
		bs = bytesPrint(bs, be, "\tkey: ", m->u.clientKeyExchange.key, "\n");
		break;
	case HFinished:
		bs = seprint(bs, be, "HFinished\n");
		for(i=0; i<m->u.finished.n; i++)
			bs = seprint(bs, be, "%.2x", m->u.finished.verify[i]);
		bs = seprint(bs, be, "\n");
		break;
	}
	USED(bs);
	return buf;
}

static void
tlsError(TlsConnection *c, int err, char *fmt, ...)
{
	char msg[512];
	va_list arg;

	va_start(arg, fmt);
	vseprint(msg, msg+sizeof(msg), fmt, arg);
	va_end(arg);
	if(c->trace)
		c->trace("tlsError: %s\n", msg);
	else if(c->erred)
		fprint(2, "double error: %r, %s", msg);
	else
		werrstr("tls: local %s", msg);
	c->erred = 1;
	fprint(c->ctl, "alert %d", err);
}

/* commit to specific version number */
static int
setVersion(TlsConnection *c, int version)
{
	if(c->verset || version > MaxProtoVersion || version < MinProtoVersion)
		return -1;
	if(version > c->version)
		version = c->version;
	if(version == SSL3Version) {
		c->version = version;
		c->finished.n = SSL3FinishedLen;
	}else if(version == TLSVersion){
		c->version = version;
		c->finished.n = TLSFinishedLen;
	}else
		return -1;
	c->verset = 1;
	return fprint(c->ctl, "version 0x%x", version);
}

/* confirm that received Finished message matches the expected value */
static int
finishedMatch(TlsConnection *c, Finished *f)
{
	return memcmp(f->verify, c->finished.verify, f->n) == 0;
}

/* free memory associated with TlsConnection struct */
/*		(but don't close the TLS channel itself) */
static void
tlsConnectionFree(TlsConnection *c)
{
	tlsSecClose(c->sec);
	freebytes(c->sid);
	freebytes(c->cert);
	memset(c, 0, sizeof(*c));
	free(c);
}


/*================= cipher choices ======================== */

static int weakCipher[CipherMax] =
{
	1,	/* TLS_NULL_WITH_NULL_NULL */
	1,	/* TLS_RSA_WITH_NULL_MD5 */
	1,	/* TLS_RSA_WITH_NULL_SHA */
	1,	/* TLS_RSA_EXPORT_WITH_RC4_40_MD5 */
	0,	/* TLS_RSA_WITH_RC4_128_MD5 */
	0,	/* TLS_RSA_WITH_RC4_128_SHA */
	1,	/* TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5 */
	0,	/* TLS_RSA_WITH_IDEA_CBC_SHA */
	1,	/* TLS_RSA_EXPORT_WITH_DES40_CBC_SHA */
	0,	/* TLS_RSA_WITH_DES_CBC_SHA */
	0,	/* TLS_RSA_WITH_3DES_EDE_CBC_SHA */
	1,	/* TLS_DH_DSS_EXPORT_WITH_DES40_CBC_SHA */
	0,	/* TLS_DH_DSS_WITH_DES_CBC_SHA */
	0,	/* TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA */
	1,	/* TLS_DH_RSA_EXPORT_WITH_DES40_CBC_SHA */
	0,	/* TLS_DH_RSA_WITH_DES_CBC_SHA */
	0,	/* TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA */
	1,	/* TLS_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA */
	0,	/* TLS_DHE_DSS_WITH_DES_CBC_SHA */
	0,	/* TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA */
	1,	/* TLS_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA */
	0,	/* TLS_DHE_RSA_WITH_DES_CBC_SHA */
	0,	/* TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA */
	1,	/* TLS_DH_anon_EXPORT_WITH_RC4_40_MD5 */
	1,	/* TLS_DH_anon_WITH_RC4_128_MD5 */
	1,	/* TLS_DH_anon_EXPORT_WITH_DES40_CBC_SHA */
	1,	/* TLS_DH_anon_WITH_DES_CBC_SHA */
	1,	/* TLS_DH_anon_WITH_3DES_EDE_CBC_SHA */
};

static int
setAlgs(TlsConnection *c, int a)
{
	int i;

	for(i = 0; i < nelem(cipherAlgs); i++){
		if(cipherAlgs[i].tlsid == a){
			c->enc = cipherAlgs[i].enc;
			c->digest = cipherAlgs[i].digest;
			c->nsecret = cipherAlgs[i].nsecret;
			if(c->nsecret > MaxKeyData)
				return 0;
			return 1;
		}
	}
	return 0;
}

static int
okCipher(Ints *cv)
{
	int weak, i, j, c;

	weak = 1;
	for(i = 0; i < cv->len; i++) {
		c = cv->data[i];
		if(c >= CipherMax)
			weak = 0;
		else
			weak &= weakCipher[c];
		for(j = 0; j < nelem(cipherAlgs); j++)
			if(cipherAlgs[j].ok && cipherAlgs[j].tlsid == c)
				return c;
	}
	if(weak)
		return -2;
	return -1;
}

static int
okCompression(Bytes *cv)
{
	int i, j, c;

	for(i = 0; i < cv->len; i++) {
		c = cv->data[i];
		for(j = 0; j < nelem(compressors); j++) {
			if(compressors[j] == c)
				return c;
		}
	}
	return -1;
}

static Lock	ciphLock;
static int	nciphers;

static int
initCiphers(void)
{
	enum {MaxAlgF = 1024, MaxAlgs = 10};
	char s[MaxAlgF], *flds[MaxAlgs];
	int i, j, n, ok;

	lock(&ciphLock);
	if(nciphers){
		unlock(&ciphLock);
		return nciphers;
	}
	j = open("#a/tls/encalgs", OREAD);
	if(j < 0){
		werrstr("can't open #a/tls/encalgs: %r");
		return 0;
	}
	n = read(j, s, MaxAlgF-1);
	close(j);
	if(n <= 0){
		werrstr("nothing in #a/tls/encalgs: %r");
		return 0;
	}
	s[n] = 0;
	n = getfields(s, flds, MaxAlgs, 1, " \t\r\n");
	for(i = 0; i < nelem(cipherAlgs); i++){
		ok = 0;
		for(j = 0; j < n; j++){
			if(strcmp(cipherAlgs[i].enc, flds[j]) == 0){
				ok = 1;
				break;
			}
		}
		cipherAlgs[i].ok = ok;
	}

	j = open("#a/tls/hashalgs", OREAD);
	if(j < 0){
		werrstr("can't open #a/tls/hashalgs: %r");
		return 0;
	}
	n = read(j, s, MaxAlgF-1);
	close(j);
	if(n <= 0){
		werrstr("nothing in #a/tls/hashalgs: %r");
		return 0;
	}
	s[n] = 0;
	n = getfields(s, flds, MaxAlgs, 1, " \t\r\n");
	for(i = 0; i < nelem(cipherAlgs); i++){
		ok = 0;
		for(j = 0; j < n; j++){
			if(strcmp(cipherAlgs[i].digest, flds[j]) == 0){
				ok = 1;
				break;
			}
		}
		cipherAlgs[i].ok &= ok;
		if(cipherAlgs[i].ok)
			nciphers++;
	}
	unlock(&ciphLock);
	return nciphers;
}

static Ints*
makeciphers(void)
{
	Ints *is;
	int i, j;

	is = newints(nciphers);
	j = 0;
	for(i = 0; i < nelem(cipherAlgs); i++){
		if(cipherAlgs[i].ok)
			is->data[j++] = cipherAlgs[i].tlsid;
	}
	return is;
}



/*================= security functions ======================== */

/* given X.509 certificate, set up connection to factotum */
/*	for using corresponding private key */
static AuthRpc*
factotum_rsa_open(uchar *cert, int certlen)
{
	char *s;
	mpint *pub = nil;
	RSApub *rsapub;
	AuthRpc *rpc;

	if((rpc = auth_allocrpc()) == nil){
		return nil;
	}
	s = "proto=rsa service=tls role=client";
	if(auth_rpc(rpc, "start", s, strlen(s)) != ARok){
		factotum_rsa_close(rpc);
		return nil;
	}

	/* roll factotum keyring around to match certificate */
	rsapub = X509toRSApub(cert, certlen, nil, 0);
	while(1){
		if(auth_rpc(rpc, "read", nil, 0) != ARok){
			factotum_rsa_close(rpc);
			rpc = nil;
			goto done;
		}
		pub = strtomp(rpc->arg, nil, 16, nil);
		assert(pub != nil);
		if(mpcmp(pub,rsapub->n) == 0)
			break;
	}
done:
	mpfree(pub);
	rsapubfree(rsapub);
	return rpc;
}

static mpint*
factotum_rsa_decrypt(AuthRpc *rpc, mpint *cipher)
{
	char *p;
	int rv;

	if((p = mptoa(cipher, 16, nil, 0)) == nil)
		return nil;
	rv = auth_rpc(rpc, "write", p, strlen(p));
	free(p);
	if(rv != ARok || auth_rpc(rpc, "read", nil, 0) != ARok)
		return nil;
	mpfree(cipher);
	return strtomp(rpc->arg, nil, 16, nil);
}

static void
factotum_rsa_close(AuthRpc*rpc)
{
	if(!rpc)
		return;
	close(rpc->afd);
	auth_freerpc(rpc);
}

static void
tlsPmd5(uchar *buf, int nbuf, uchar *key, int nkey, uchar *label, int nlabel, uchar *seed0, int nseed0, uchar *seed1, int nseed1)
{
	uchar ai[MD5dlen], tmp[MD5dlen];
	int i, n;
	MD5state *s;

	/* generate a1 */
	s = hmac_md5(label, nlabel, key, nkey, nil, nil);
	s = hmac_md5(seed0, nseed0, key, nkey, nil, s);
	hmac_md5(seed1, nseed1, key, nkey, ai, s);

	while(nbuf > 0) {
		s = hmac_md5(ai, MD5dlen, key, nkey, nil, nil);
		s = hmac_md5(label, nlabel, key, nkey, nil, s);
		s = hmac_md5(seed0, nseed0, key, nkey, nil, s);
		hmac_md5(seed1, nseed1, key, nkey, tmp, s);
		n = MD5dlen;
		if(n > nbuf)
			n = nbuf;
		for(i = 0; i < n; i++)
			buf[i] ^= tmp[i];
		buf += n;
		nbuf -= n;
		hmac_md5(ai, MD5dlen, key, nkey, tmp, nil);
		memmove(ai, tmp, MD5dlen);
	}
}

static void
tlsPsha1(uchar *buf, int nbuf, uchar *key, int nkey, uchar *label, int nlabel, uchar *seed0, int nseed0, uchar *seed1, int nseed1)
{
	uchar ai[SHA1dlen], tmp[SHA1dlen];
	int i, n;
	SHAstate *s;

	/* generate a1 */
	s = hmac_sha1(label, nlabel, key, nkey, nil, nil);
	s = hmac_sha1(seed0, nseed0, key, nkey, nil, s);
	hmac_sha1(seed1, nseed1, key, nkey, ai, s);

	while(nbuf > 0) {
		s = hmac_sha1(ai, SHA1dlen, key, nkey, nil, nil);
		s = hmac_sha1(label, nlabel, key, nkey, nil, s);
		s = hmac_sha1(seed0, nseed0, key, nkey, nil, s);
		hmac_sha1(seed1, nseed1, key, nkey, tmp, s);
		n = SHA1dlen;
		if(n > nbuf)
			n = nbuf;
		for(i = 0; i < n; i++)
			buf[i] ^= tmp[i];
		buf += n;
		nbuf -= n;
		hmac_sha1(ai, SHA1dlen, key, nkey, tmp, nil);
		memmove(ai, tmp, SHA1dlen);
	}
}

/* fill buf with md5(args)^sha1(args) */
static void
tlsPRF(uchar *buf, int nbuf, uchar *key, int nkey, char *label, uchar *seed0, int nseed0, uchar *seed1, int nseed1)
{
	int i;
	int nlabel = strlen(label);
	int n = (nkey + 1) >> 1;

	for(i = 0; i < nbuf; i++)
		buf[i] = 0;
	tlsPmd5(buf, nbuf, key, n, (uchar*)label, nlabel, seed0, nseed0, seed1, nseed1);
	tlsPsha1(buf, nbuf, key+nkey-n, n, (uchar*)label, nlabel, seed0, nseed0, seed1, nseed1);
}

/*
 * for setting server session id's
 */
static Lock	sidLock;
static long	maxSid = 1;

/* the keys are verified to have the same public components
 * and to function correctly with pkcs 1 encryption and decryption. */
static TlsSec*
tlsSecInits(int cvers, uchar *csid, int ncsid, uchar *crandom, uchar *ssid, int *nssid, uchar *srandom)
{
	TlsSec *sec = emalloc(sizeof(*sec));

	USED(csid); USED(ncsid);  /* ignore csid for now */

	memmove(sec->crandom, crandom, RandomSize);
	sec->clientVers = cvers;

	put32(sec->srandom, time(0));
	genrandom(sec->srandom+4, RandomSize-4);
	memmove(srandom, sec->srandom, RandomSize);

	/*
	 * make up a unique sid: use our pid, and and incrementing id
	 * can signal no sid by setting nssid to 0.
	 */
	memset(ssid, 0, SidSize);
	put32(ssid, getpid());
	lock(&sidLock);
	put32(ssid+4, maxSid++);
	unlock(&sidLock);
	*nssid = SidSize;
	return sec;
}

static int
tlsSecSecrets(TlsSec *sec, int vers, uchar *epm, int nepm, uchar *kd, int nkd)
{
	if(epm != nil){
		if(setVers(sec, vers) < 0)
			goto Err;
		serverMasterSecret(sec, epm, nepm);
	}else if(sec->vers != vers){
		werrstr("mismatched session versions");
		goto Err;
	}
	setSecrets(sec, kd, nkd);
	return 0;
Err:
	sec->ok = -1;
	return -1;
}

static TlsSec*
tlsSecInitc(int cvers, uchar *crandom)
{
	TlsSec *sec = emalloc(sizeof(*sec));
	sec->clientVers = cvers;
	put32(sec->crandom, time(0));
	genrandom(sec->crandom+4, RandomSize-4);
	memmove(crandom, sec->crandom, RandomSize);
	return sec;
}

static int
tlsSecSecretc(TlsSec *sec, uchar *sid, int nsid, uchar *srandom, uchar *cert, int ncert, int vers, uchar **epm, int *nepm, uchar *kd, int nkd)
{
	RSApub *pub;

	pub = nil;

	USED(sid);
	USED(nsid);

	memmove(sec->srandom, srandom, RandomSize);

	if(setVers(sec, vers) < 0)
		goto Err;

	pub = X509toRSApub(cert, ncert, nil, 0);
	if(pub == nil){
		werrstr("invalid x509/rsa certificate");
		goto Err;
	}
	if(clientMasterSecret(sec, pub, epm, nepm) < 0)
		goto Err;
	rsapubfree(pub);
	setSecrets(sec, kd, nkd);
	return 0;

Err:
	if(pub != nil)
		rsapubfree(pub);
	sec->ok = -1;
	return -1;
}

static int
tlsSecFinished(TlsSec *sec, MD5state md5, SHAstate sha1, uchar *fin, int nfin, int isclient)
{
	if(sec->nfin != nfin){
		sec->ok = -1;
		werrstr("invalid finished exchange");
		return -1;
	}
	md5.malloced = 0;
	sha1.malloced = 0;
	(*sec->setFinished)(sec, md5, sha1, fin, isclient);
	return 1;
}

static void
tlsSecOk(TlsSec *sec)
{
	if(sec->ok == 0)
		sec->ok = 1;
}

/*
static void
tlsSecKill(TlsSec *sec)
{
	if(!sec)
		return;
	factotum_rsa_close(sec->rpc);
	sec->ok = -1;
}
*/

static void
tlsSecClose(TlsSec *sec)
{
	if(!sec)
		return;
	factotum_rsa_close(sec->rpc);
	free(sec->server);
	free(sec);
}

static int
setVers(TlsSec *sec, int v)
{
	if(v == SSL3Version){
		sec->setFinished = sslSetFinished;
		sec->nfin = SSL3FinishedLen;
		sec->prf = sslPRF;
	}else if(v == TLSVersion){
		sec->setFinished = tlsSetFinished;
		sec->nfin = TLSFinishedLen;
		sec->prf = tlsPRF;
	}else{
		werrstr("invalid version");
		return -1;
	}
	sec->vers = v;
	return 0;
}

/*
 * generate secret keys from the master secret.
 *
 * different crypto selections will require different amounts
 * of key expansion and use of key expansion data,
 * but it's all generated using the same function.
 */
static void
setSecrets(TlsSec *sec, uchar *kd, int nkd)
{
	(*sec->prf)(kd, nkd, sec->sec, MasterSecretSize, "key expansion",
			sec->srandom, RandomSize, sec->crandom, RandomSize);
}

/*
 * set the master secret from the pre-master secret.
 */
static void
setMasterSecret(TlsSec *sec, Bytes *pm)
{
	(*sec->prf)(sec->sec, MasterSecretSize, pm->data, MasterSecretSize, "master secret",
			sec->crandom, RandomSize, sec->srandom, RandomSize);
}

static void
serverMasterSecret(TlsSec *sec, uchar *epm, int nepm)
{
	Bytes *pm;

	pm = pkcs1_decrypt(sec, epm, nepm);

	/* if the client messed up, just continue as if everything is ok, */
	/* to prevent attacks to check for correctly formatted messages. */
	/* Hence the fprint(2,) can't be replaced by tlsError(), which sends an Alert msg to the client. */
	if(sec->ok < 0 || pm == nil || get16(pm->data) != sec->clientVers){
		fprint(2, "serverMasterSecret failed ok=%d pm=%p pmvers=%x cvers=%x nepm=%d\n",
			sec->ok, pm, pm ? get16(pm->data) : -1, sec->clientVers, nepm);
		sec->ok = -1;
		if(pm != nil)
			freebytes(pm);
		pm = newbytes(MasterSecretSize);
		genrandom(pm->data, MasterSecretSize);
	}
	setMasterSecret(sec, pm);
	memset(pm->data, 0, pm->len);
	freebytes(pm);
}

static int
clientMasterSecret(TlsSec *sec, RSApub *pub, uchar **epm, int *nepm)
{
	Bytes *pm, *key;

	pm = newbytes(MasterSecretSize);
	put16(pm->data, sec->clientVers);
	genrandom(pm->data+2, MasterSecretSize - 2);

	setMasterSecret(sec, pm);

	key = pkcs1_encrypt(pm, pub, 2);
	memset(pm->data, 0, pm->len);
	freebytes(pm);
	if(key == nil){
		werrstr("tls pkcs1_encrypt failed");
		return -1;
	}

	*nepm = key->len;
	*epm = malloc(*nepm);
	if(*epm == nil){
		freebytes(key);
		werrstr("out of memory");
		return -1;
	}
	memmove(*epm, key->data, *nepm);

	freebytes(key);

	return 1;
}

static void
sslSetFinished(TlsSec *sec, MD5state hsmd5, SHAstate hssha1, uchar *finished, int isClient)
{
	DigestState *s;
	uchar h0[MD5dlen], h1[SHA1dlen], pad[48];
	char *label;

	if(isClient)
		label = "CLNT";
	else
		label = "SRVR";

	md5((uchar*)label, 4, nil, &hsmd5);
	md5(sec->sec, MasterSecretSize, nil, &hsmd5);
	memset(pad, 0x36, 48);
	md5(pad, 48, nil, &hsmd5);
	md5(nil, 0, h0, &hsmd5);
	memset(pad, 0x5C, 48);
	s = md5(sec->sec, MasterSecretSize, nil, nil);
	s = md5(pad, 48, nil, s);
	md5(h0, MD5dlen, finished, s);

	sha1((uchar*)label, 4, nil, &hssha1);
	sha1(sec->sec, MasterSecretSize, nil, &hssha1);
	memset(pad, 0x36, 40);
	sha1(pad, 40, nil, &hssha1);
	sha1(nil, 0, h1, &hssha1);
	memset(pad, 0x5C, 40);
	s = sha1(sec->sec, MasterSecretSize, nil, nil);
	s = sha1(pad, 40, nil, s);
	sha1(h1, SHA1dlen, finished + MD5dlen, s);
}

/* fill "finished" arg with md5(args)^sha1(args) */
static void
tlsSetFinished(TlsSec *sec, MD5state hsmd5, SHAstate hssha1, uchar *finished, int isClient)
{
	uchar h0[MD5dlen], h1[SHA1dlen];
	char *label;

	/* get current hash value, but allow further messages to be hashed in */
	md5(nil, 0, h0, &hsmd5);
	sha1(nil, 0, h1, &hssha1);

	if(isClient)
		label = "client finished";
	else
		label = "server finished";
	tlsPRF(finished, TLSFinishedLen, sec->sec, MasterSecretSize, label, h0, MD5dlen, h1, SHA1dlen);
}

static void
sslPRF(uchar *buf, int nbuf, uchar *key, int nkey, char *label, uchar *seed0, int nseed0, uchar *seed1, int nseed1)
{
	DigestState *s;
	uchar sha1dig[SHA1dlen], md5dig[MD5dlen], tmp[26];
	int i, n, len;

	USED(label);
	len = 1;
	while(nbuf > 0){
		if(len > 26)
			return;
		for(i = 0; i < len; i++)
			tmp[i] = 'A' - 1 + len;
		s = sha1(tmp, len, nil, nil);
		s = sha1(key, nkey, nil, s);
		s = sha1(seed0, nseed0, nil, s);
		sha1(seed1, nseed1, sha1dig, s);
		s = md5(key, nkey, nil, nil);
		md5(sha1dig, SHA1dlen, md5dig, s);
		n = MD5dlen;
		if(n > nbuf)
			n = nbuf;
		memmove(buf, md5dig, n);
		buf += n;
		nbuf -= n;
		len++;
	}
}

static mpint*
bytestomp(Bytes* bytes)
{
	mpint* ans;

	ans = betomp(bytes->data, bytes->len, nil);
	return ans;
}

/*
 * Convert mpint* to Bytes, putting high order byte first.
 */
static Bytes*
mptobytes(mpint* big)
{
	int n, m;
	uchar *a;
	Bytes* ans;

	n = (mpsignif(big)+7)/8;
	m = mptobe(big, nil, n, &a);
	ans = makebytes(a, m);
	return ans;
}

/* Do RSA computation on block according to key, and pad */
/* result on left with zeros to make it modlen long. */
static Bytes*
rsacomp(Bytes* block, RSApub* key, int modlen)
{
	mpint *x, *y;
	Bytes *a, *ybytes;
	int ylen;

	x = bytestomp(block);
	y = rsaencrypt(key, x, nil);
	mpfree(x);
	ybytes = mptobytes(y);
	ylen = ybytes->len;

	if(ylen < modlen) {
		a = newbytes(modlen);
		memset(a->data, 0, modlen-ylen);
		memmove(a->data+modlen-ylen, ybytes->data, ylen);
		freebytes(ybytes);
		ybytes = a;
	}
	else if(ylen > modlen) {
		/* assume it has leading zeros (mod should make it so) */
		a = newbytes(modlen);
		memmove(a->data, ybytes->data, modlen);
		freebytes(ybytes);
		ybytes = a;
	}
	mpfree(y);
	return ybytes;
}

/* encrypt data according to PKCS#1, /lib/rfc/rfc2437 9.1.2.1 */
static Bytes*
pkcs1_encrypt(Bytes* data, RSApub* key, int blocktype)
{
	Bytes *pad, *eb, *ans;
	int i, dlen, padlen, modlen;

	modlen = (mpsignif(key->n)+7)/8;
	dlen = data->len;
	if(modlen < 12 || dlen > modlen - 11)
		return nil;
	padlen = modlen - 3 - dlen;
	pad = newbytes(padlen);
	genrandom(pad->data, padlen);
	for(i = 0; i < padlen; i++) {
		if(blocktype == 0)
			pad->data[i] = 0;
		else if(blocktype == 1)
			pad->data[i] = 255;
		else if(pad->data[i] == 0)
			pad->data[i] = 1;
	}
	eb = newbytes(modlen);
	eb->data[0] = 0;
	eb->data[1] = blocktype;
	memmove(eb->data+2, pad->data, padlen);
	eb->data[padlen+2] = 0;
	memmove(eb->data+padlen+3, data->data, dlen);
	ans = rsacomp(eb, key, modlen);
	freebytes(eb);
	freebytes(pad);
	return ans;
}

/* decrypt data according to PKCS#1, with given key. */
/* expect a block type of 2. */
static Bytes*
pkcs1_decrypt(TlsSec *sec, uchar *epm, int nepm)
{
	Bytes *eb, *ans = nil;
	int i, modlen;
	mpint *x, *y;

	modlen = (mpsignif(sec->rsapub->n)+7)/8;
	if(nepm != modlen)
		return nil;
	x = betomp(epm, nepm, nil);
	y = factotum_rsa_decrypt(sec->rpc, x);
	if(y == nil)
		return nil;
	eb = mptobytes(y);
	if(eb->len < modlen){ /* pad on left with zeros */
		ans = newbytes(modlen);
		memset(ans->data, 0, modlen-eb->len);
		memmove(ans->data+modlen-eb->len, eb->data, eb->len);
		freebytes(eb);
		eb = ans;
	}
	if(eb->data[0] == 0 && eb->data[1] == 2) {
		for(i = 2; i < modlen; i++)
			if(eb->data[i] == 0)
				break;
		if(i < modlen - 1)
			ans = makebytes(eb->data+i+1, modlen-(i+1));
	}
	freebytes(eb);
	return ans;
}


/*================= general utility functions ======================== */

static void *
emalloc(int n)
{
	void *p;
	if(n==0)
		n=1;
	p = malloc(n);
	if(p == nil){
		exits("out of memory");
	}
	memset(p, 0, n);
	return p;
}

static void *
erealloc(void *ReallocP, int ReallocN)
{
	if(ReallocN == 0)
		ReallocN = 1;
	if(!ReallocP)
		ReallocP = emalloc(ReallocN);
	else if(!(ReallocP = realloc(ReallocP, ReallocN))){
		exits("out of memory");
	}
	return(ReallocP);
}

static void
put32(uchar *p, u32int x)
{
	p[0] = x>>24;
	p[1] = x>>16;
	p[2] = x>>8;
	p[3] = x;
}

static void
put24(uchar *p, int x)
{
	p[0] = x>>16;
	p[1] = x>>8;
	p[2] = x;
}

static void
put16(uchar *p, int x)
{
	p[0] = x>>8;
	p[1] = x;
}

/*
static u32int
get32(uchar *p)
{
	return (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
}
*/

static int
get24(uchar *p)
{
	return (p[0]<<16)|(p[1]<<8)|p[2];
}

static int
get16(uchar *p)
{
	return (p[0]<<8)|p[1];
}

/* ANSI offsetof() */
#define OFFSET(x, s) ((intptr)(&(((s*)0)->x)))

/*
 * malloc and return a new Bytes structure capable of
 * holding len bytes. (len >= 0)
 * Used to use crypt_malloc, which aborts if malloc fails.
 */
static Bytes*
newbytes(int len)
{
	Bytes* ans;

	ans = (Bytes*)malloc(OFFSET(data[0], Bytes) + len);
	ans->len = len;
	return ans;
}

/*
 * newbytes(len), with data initialized from buf
 */
static Bytes*
makebytes(uchar* buf, int len)
{
	Bytes* ans;

	ans = newbytes(len);
	memmove(ans->data, buf, len);
	return ans;
}

static void
freebytes(Bytes* b)
{
	if(b != nil)
		free(b);
}

/* len is number of ints */
static Ints*
newints(int len)
{
	Ints* ans;

	ans = (Ints*)malloc(OFFSET(data[0], Ints) + len*sizeof(int));
	ans->len = len;
	return ans;
}

/*
static Ints*
makeints(int* buf, int len)
{
	Ints* ans;

	ans = newints(len);
	if(len > 0)
		memmove(ans->data, buf, len*sizeof(int));
	return ans;
}
*/

static void
freeints(Ints* b)
{
	if(b != nil)
		free(b);
}
