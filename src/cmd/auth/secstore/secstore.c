/* network login client */
#include <u.h>
#include <libc.h>
#include <mp.h>
#include <libsec.h>
#include <authsrv.h>
#include "SConn.h"
#include "secstore.h"
enum{ CHK = 16, MAXFILES = 100 };

typedef struct AuthConn{
	SConn *conn;
	char pass[64];
	int passlen;
} AuthConn;

int verbose;
Nvrsafe nvr;
char *SECSTORE_DIR;

void
usage(void)
{
	fprint(2, "usage: secstore [-cin] [-g getfile] [-p putfile] [-r rmfile] [-s tcp!server!5356] [-u user] [-v]\n");
	exits("usage");
}

static int
getfile(SConn *conn, char *gf, uchar **buf, ulong *buflen, uchar *key, int nkey)
{
	int fd = -1;
	int i, n, nr, nw, len;
	char s[Maxmsg+1];
	uchar skey[SHA1dlen], ib[Maxmsg+CHK], *ibr, *ibw, *bufw, *bufe;
	AESstate aes;
	DigestState *sha;

	if(strchr(gf, '/')){
		fprint(2, "simple filenames, not paths like %s\n", gf);
		return -1;
	}
	memset(&aes, 0, sizeof aes);

	snprint(s, Maxmsg, "GET %s\n", gf);
	conn->write(conn, (uchar*)s, strlen(s));

	/* get file size */
	s[0] = '\0';
	bufw = bufe = nil;
	if(readstr(conn, s) < 0){
		fprint(2, "remote: %s\n", s);
		return -1;
	}
	len = atoi(s);
	if(len == -1){
		fprint(2, "remote file %s does not exist\n", gf);
		return -1;
	}else if(len == -3){
		fprint(2, "implausible filesize for %s\n", gf);
		return -1;
	}else if(len < 0){
		fprint(2, "GET refused for %s\n", gf);
		return -1;
	}
	if(buf != nil){
		*buflen = len - AESbsize - CHK;
		*buf = bufw = emalloc(len);
		bufe = bufw + len;
	}

	/* directory listing */
	if(strcmp(gf,".")==0){
		if(buf != nil)
			*buflen = len;
		for(i=0; i < len; i += n){
			if((n = conn->read(conn, (uchar*)s, Maxmsg)) <= 0){
				fprint(2, "empty file chunk\n");
				return -1;
			}
			if(buf == nil)
				write(1, s, n);
			else
				memmove((*buf)+i, s, n);
		}
		return 0;
	}

	/* conn is already encrypted against wiretappers,
		but gf is also encrypted against server breakin. */
	if(buf == nil && (fd =create(gf, OWRITE, 0600)) < 0){
		fprint(2, "can't open %s: %r\n", gf);
		return -1;
	}

	ibr = ibw = ib;
	for(nr=0; nr < len;){
		if((n = conn->read(conn, ibw, Maxmsg)) <= 0){
			fprint(2, "empty file chunk n=%d nr=%d len=%d: %r\n", n, nr, len);
			return -1;
		}
		nr += n;
		ibw += n;
		if(!aes.setup){ /* first time, read 16 byte IV */
			if(n < AESbsize){
				fprint(2, "no IV in file\n");
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
			if(buf == nil){
				nw = write(fd, ibr, n);
				if(nw != n){
					fprint(2, "write error on %s", gf);
					return -1;
				}
			}else{
				assert(bufw+n <= bufe);
				memmove(bufw, ibr, n);
				bufw += n;
			}
			ibr += n;
		}
		memmove(ib, ibr, ibw-ibr);
		ibw = ib + (ibw-ibr);
		ibr = ib;
	}
	if(buf == nil)
		close(fd);
	n = ibw-ibr;
	if((n != CHK) || (memcmp(ib, "XXXXXXXXXXXXXXXX", CHK) != 0)){
			fprint(2,"decrypted file failed to authenticate!\n");
			return -1;
	}
	return 0;
}

/* This sends a file to the secstore disk that can, in an emergency, be */
/* decrypted by the program aescbc.c. */
static int
putfile(SConn *conn, char *pf, uchar *buf, ulong len, uchar *key, int nkey)
{
	int i, n, fd, ivo, bufi, done;
	char s[Maxmsg];
	uchar  skey[SHA1dlen], b[CHK+Maxmsg], IV[AESbsize];
	AESstate aes;
	DigestState *sha;

	/* create initialization vector */
	srand(time(0));  /* doesn't need to be unpredictable */
	for(i=0; i<AESbsize; i++)
		IV[i] = 0xff & rand();
	sha = sha1((uchar*)"aescbc file", 11, nil, nil);
	sha1(key, nkey, skey, sha);
	setupAESstate(&aes, skey, AESbsize, IV);
	memset(skey, 0, sizeof skey);

	snprint(s, Maxmsg, "PUT %s\n", pf);
	conn->write(conn, (uchar*)s, strlen(s));

	if(buf == nil){
		/* get file size */
		if((fd = open(pf, OREAD)) < 0){
			fprint(2, "can't open %s: %r\n", pf);
			return -1;
		}
		len = seek(fd, 0, 2);
		seek(fd, 0, 0);
	} else {
		fd = -1;
	}
	if(len > MAXFILESIZE){
		fprint(2, "implausible filesize %ld for %s\n", len, pf);
		return -1;
	}

	/* send file size */
	snprint(s, Maxmsg, "%ld", len+AESbsize+CHK);
	conn->write(conn, (uchar*)s, strlen(s));

	/* send IV and file+XXXXX in Maxmsg chunks */
	ivo = AESbsize;
	bufi = 0;
	memcpy(b, IV, ivo);
	for(done = 0; !done; ){
		if(buf == nil){
			n = read(fd, b+ivo, Maxmsg-ivo);
			if(n < 0){
				fprint(2, "read error on %s: %r\n", pf);
				return -1;
			}
		}else{
			if((n = len - bufi) > Maxmsg-ivo)
				n = Maxmsg-ivo;
			memcpy(b+ivo, buf+bufi, n);
			bufi += n;
		}
		n += ivo;
		ivo = 0;
		if(n < Maxmsg){ /* EOF on input; append XX... */
			memset(b+n, 'X', CHK);
			n += CHK; /* might push n>Maxmsg */
			done = 1;
		}
		aesCBCencrypt(b, n, &aes);
		if(n > Maxmsg){
			assert(done==1);
			conn->write(conn, b, Maxmsg);
			n -= Maxmsg;
			memmove(b, b+Maxmsg, n);
		}
		conn->write(conn, b, n);
	}

	if(buf == nil)
		close(fd);
	fprint(2, "saved %ld bytes\n", len);

	return 0;
}

static int
removefile(SConn *conn, char *rf)
{
	char buf[Maxmsg];

	if(strchr(rf, '/')){
		fprint(2, "simple filenames, not paths like %s\n", rf);
		return -1;
	}

	snprint(buf, Maxmsg, "RM %s\n", rf);
	conn->write(conn, (uchar*)buf, strlen(buf));

	return 0;
}

static int
cmd(AuthConn *c, char **gf, int *Gflag, char **pf, char **rf)
{
	ulong len;
	int rv = -1;
	uchar *memfile, *memcur, *memnext;

	while(*gf != nil){
		if(verbose)
			fprint(2, "get %s\n", *gf);
		if(getfile(c->conn, *gf, *Gflag ? &memfile : nil, &len, (uchar*)c->pass, c->passlen) < 0)
			goto Out;
		if(*Gflag){
			/* write one line at a time, as required by /mnt/factotum/ctl */
			memcur = memfile;
			while(len>0){
				memnext = (uchar*)strchr((char*)memcur, '\n');
				if(memnext){
					write(1, memcur, memnext-memcur+1);
					len -= memnext-memcur+1;
					memcur = memnext+1;
				}else{
					write(1, memcur, len);
					break;
				}
			}
			free(memfile);
		}
		gf++;
		Gflag++;
	}
	while(*pf != nil){
		if(verbose)
			fprint(2, "put %s\n", *pf);
		if(putfile(c->conn, *pf, nil, 0, (uchar*)c->pass, c->passlen) < 0)
			goto Out;
		pf++;
	}
	while(*rf != nil){
		if(verbose)
			fprint(2, "rm  %s\n", *rf);
		if(removefile(c->conn, *rf) < 0)
			goto Out;
		rf++;
	}

	c->conn->write(c->conn, (uchar*)"BYE", 3);
	rv = 0;

Out:
	c->conn->free(c->conn);
	return rv;
}

static int
chpasswd(AuthConn *c, char *id)
{
	ulong len;
	int rv = -1, newpasslen = 0;
	mpint *H, *Hi;
	uchar *memfile;
	char *newpass, *passck;
	char *list, *cur, *next, *hexHi;
	char *f[8], prompt[128];

	H = mpnew(0);
	Hi = mpnew(0);
	/* changing our password is vulnerable to connection failure */
	for(;;){
		snprint(prompt, sizeof(prompt), "new password for %s: ", id);
		newpass = readcons(prompt, nil, 1);
		if(newpass == nil)
			goto Out;
		if(strlen(newpass) >= 7)
			break;
		else if(strlen(newpass) == 0){
			fprint(2, "!password change aborted\n");
			goto Out;
		}
		print("!password must be at least 7 characters\n");
	}
	newpasslen = strlen(newpass);
	snprint(prompt, sizeof(prompt), "retype password: ");
	passck = readcons(prompt, nil, 1);
	if(passck == nil){
		fprint(2, "readcons failed\n");
		goto Out;
	}
	if(strcmp(passck, newpass) != 0){
		fprint(2, "passwords didn't match\n");
		goto Out;
	}

	c->conn->write(c->conn, (uchar*)"CHPASS", strlen("CHPASS"));
	hexHi = PAK_Hi(id, newpass, H, Hi);
	c->conn->write(c->conn, (uchar*)hexHi, strlen(hexHi));
	free(hexHi);
	mpfree(H);
	mpfree(Hi);

	if(getfile(c->conn, ".", (uchar **)(void*)&list, &len, nil, 0) < 0){
		fprint(2, "directory listing failed.\n");
		goto Out;
	}

	/* Loop over files and reencrypt them; try to keep going after error */
	for(cur=list; (next=strchr(cur, '\n')) != nil; cur=next+1){
		*next = '\0';
		if(tokenize(cur, f, nelem(f))< 1)
			break;
		fprint(2, "reencrypting '%s'\n", f[0]);
		if(getfile(c->conn, f[0], &memfile, &len, (uchar*)c->pass, c->passlen) < 0){
			fprint(2, "getfile of '%s' failed\n", f[0]);
			continue;
		}
		if(putfile(c->conn, f[0], memfile, len, (uchar*)newpass, newpasslen) < 0)
			fprint(2, "putfile of '%s' failed\n", f[0]);
		free(memfile);
	}
	free(list);
	c->conn->write(c->conn, (uchar*)"BYE", 3);
	rv = 0;

Out:
	if(newpass != nil){
		memset(newpass, 0, newpasslen);
		free(newpass);
	}
	c->conn->free(c->conn);
	return rv;
}

static AuthConn*
login(char *id, char *dest, int pass_stdin, int pass_nvram)
{
	AuthConn *c;
	int fd, n, ntry = 0;
	char *S, *PINSTA = nil, *nl, s[Maxmsg+1], *pass;

	if(dest == nil){
		fprint(2, "tried to login with nil dest\n");
		exits("nil dest");
	}
	c = emalloc(sizeof(*c));
	if(pass_nvram){
		/* if(readnvram(&nvr, 0) < 0) */
			exits("readnvram: %r");
		strecpy(c->pass, c->pass+sizeof c->pass, nvr.config);
	}
	if(pass_stdin){
		n = readn(0, s, Maxmsg-2);  /* so len(PINSTA)<Maxmsg-3 */
		if(n < 1)
			exits("no password on standard input");
		s[n] = 0;
		nl = strchr(s, '\n');
		if(nl){
			*nl++ = 0;
			PINSTA = estrdup(nl);
			nl = strchr(PINSTA, '\n');
			if(nl)
				*nl = 0;
		}
		strecpy(c->pass, c->pass+sizeof c->pass, s);
	}
	while(1){
		if(verbose)
			fprint(2, "dialing %s\n", dest);
		if((fd = dial(dest, nil, nil, nil)) < 0){
			fprint(2, "can't dial %s\n", dest);
			free(c);
			return nil;
		}
		if((c->conn = newSConn(fd)) == nil){
			free(c);
			return nil;
		}
		ntry++;
		if(!pass_stdin && !pass_nvram){
			pass = readcons("secstore password", nil, 1);
			if(pass == nil)
				pass = estrdup("");
			if(strlen(pass) >= sizeof c->pass){
				fprint(2, "password too long, skipping secstore login\n");
				exits("password too long");
			}
			strcpy(c->pass, pass);
			memset(pass, 0, strlen(pass));
			free(pass);
		}
		if(c->pass[0]==0){
			fprint(2, "null password, skipping secstore login\n");
			exits("no password");
		}
		if(PAKclient(c->conn, id, c->pass, &S) >= 0)
			break;
		c->conn->free(c->conn);
		if(pass_stdin)
			exits("invalid password on standard input");
		if(pass_nvram)
			exits("invalid password in nvram");
		/* and let user try retyping the password */
		if(ntry==3)
			fprint(2, "Enter an empty password to quit.\n");
	}
	c->passlen = strlen(c->pass);
	fprint(2, "server: %s\n", S);
	free(S);
	if(readstr(c->conn, s) < 0){
		c->conn->free(c->conn);
		free(c);
		return nil;
	}
	if(strcmp(s, "STA") == 0){
		long sn;
		if(pass_stdin){
			if(PINSTA)
				strncpy(s+3, PINSTA, (sizeof s)-3);
			else
				exits("missing PIN+SecureID on standard input");
			free(PINSTA);
		}else{
			pass = readcons("STA PIN+SecureID", nil, 1);
			if(pass == nil)
				pass = estrdup("");
			strncpy(s+3, pass, (sizeof s)-4);
			memset(pass, 0, strlen(pass));
			free(pass);
		}
		sn = strlen(s+3);
		if(verbose)
			fprint(2, "%ld\n", sn);
		c->conn->write(c->conn, (uchar*)s, sn+3);
		readstr(c->conn, s);
	}
	if(strcmp(s, "OK") != 0){
		fprint(2, "%s\n", s);
		c->conn->free(c->conn);
		free(c);
		return nil;
	}
	return c;
}

int
main(int argc, char **argv)
{
	int chpass = 0, pass_stdin = 0, pass_nvram = 0, rc;
	int ngfile = 0, npfile = 0, nrfile = 0, Gflag[MAXFILES+1];
	char *gfile[MAXFILES], *pfile[MAXFILES], *rfile[MAXFILES];
	char *serve, *tcpserve, *user;
	AuthConn *c;

	serve = getenv("secstore");
	if(serve == nil)
		serve = "secstore";
	user = getuser();
	memset(Gflag, 0, sizeof Gflag);
	fmtinstall('B', mpfmt);
	fmtinstall('H', encodefmt);

	ARGBEGIN{
	case 'c':
		chpass = 1;
		break;
	case 'G':
		Gflag[ngfile]++;
		/* fall through */
	case 'g':
		if(ngfile >= MAXFILES)
			exits("too many gfiles");
		gfile[ngfile++] = ARGF();
		if(gfile[ngfile-1] == nil)
			usage();
		break;
	case 'i':
		pass_stdin = 1;
		break;
	case 'n':
		pass_nvram = 1;
		break;
	case 'p':
		if(npfile >= MAXFILES)
			exits("too many pfiles");
		pfile[npfile++] = ARGF();
		if(pfile[npfile-1] == nil)
			usage();
		break;
	case 'r':
		if(nrfile >= MAXFILES)
			exits("too many rfiles");
		rfile[nrfile++] = ARGF();
		if(rfile[nrfile-1] == nil)
			usage();
		break;
	case 's':
		serve = EARGF(usage());
		break;
	case 'u':
		user = EARGF(usage());
		break;
	case 'v':
		verbose++;
		break;
	default:
		usage();
		break;
	}ARGEND;
	gfile[ngfile] = nil;
	pfile[npfile] = nil;
	rfile[nrfile] = nil;

	if(argc!=0 || user==nil)
		usage();

	if(chpass && (ngfile || npfile || nrfile)){
		fprint(2, "Get, put, and remove invalid with password change.\n");
		exits("usage");
	}

	tcpserve = netmkaddr(serve, "tcp", "secstore");
	c = login(user, tcpserve, pass_stdin, pass_nvram);
	if(c == nil){
		fprint(2, "secstore authentication failed\n");
		exits("secstore authentication failed");
	}
	if(chpass)
		rc = chpasswd(c, user);
	else
		rc = cmd(c, gfile, Gflag, pfile, rfile);
	if(rc < 0){
		fprint(2, "secstore cmd failed\n");
		exits("secstore cmd failed");
	}
	exits("");
	return 0;
}
