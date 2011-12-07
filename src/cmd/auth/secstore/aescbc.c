/*
encrypt file by writing
	v2hdr,
	16byte initialization vector,
	AES-CBC(key, random | file),
HMAC_SHA1(md5(key), AES-CBC(random | file))
*/
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mp.h>
#include <libsec.h>

extern char* getpassm(char*);

enum{ CHK = 16, BUF = 4096 };

uchar v2hdr[AESbsize+1] = "AES CBC SHA1  2\n";
Biobuf bin;
Biobuf bout;

void
safewrite(uchar *buf, int n)
{
	int i = Bwrite(&bout, buf, n);

	if(i == n)
		return;
	fprint(2, "write error\n");
	exits("write error");
}

void
saferead(uchar *buf, int n)
{
	int i = Bread(&bin, buf, n);

	if(i == n)
		return;
	fprint(2, "read error\n");
	exits("read error");
}

uchar *copy;
int ncopy;

void
safecopy(uchar *buf, int n)
{
	copy = realloc(copy, ncopy+n);
	if(copy == nil) {
		fprint(2, "out of memory\n");
		exits("memory");
	}
	memmove(copy+ncopy, buf, n);
	ncopy += n;
}

int
main(int argc, char **argv)
{
	int encrypt = 0;  /* 0=decrypt, 1=encrypt */
	int n, nkey, pass_stdin = 0;
	char *pass;
	uchar key[AESmaxkey], key2[SHA1dlen];
	uchar buf[BUF+SHA1dlen];    /* assumption: CHK <= SHA1dlen */
	AESstate aes;
	DigestState *dstate;

	ARGBEGIN{
	case 'e':
		encrypt = 1;
		break;
	case 'i':
		pass_stdin = 1;
		break;
	}ARGEND;
	if(argc!=0){
		fprint(2,"usage: %s -d < cipher.aes > clear.txt\n", argv0);
		fprint(2,"   or: %s -e < clear.txt > cipher.aes\n", argv0);
		exits("usage");
	}
	Binit(&bin, 0, OREAD);
	Binit(&bout, 1, OWRITE);

	if(pass_stdin){
		n = readn(3, buf, (sizeof buf)-1);
		if(n < 1)
			exits("usage: echo password |[3=1] auth/aescbc -i ...");
		buf[n] = 0;
		while(buf[n-1] == '\n')
			buf[--n] = 0;
	}else{
		pass = readcons("aescbc password", nil, 1);
		if(pass == nil)
			exits("readcons");
		n = strlen(pass);
		if(n >= BUF)
			exits("key too long");
		strcpy((char*)buf, pass);
		memset(pass, 0, n);
		free(pass);
	}
	if(n <= 0){
		fprint(2,"no key\n");
		exits("key");
	}
	dstate = sha1((uchar*)"aescbc file", 11, nil, nil);
	sha1(buf, n, key2, dstate);
	memcpy(key, key2, 16);
	nkey = 16;
	md5(key, nkey, key2, 0);  /* so even if HMAC_SHA1 is broken, encryption key is protected */

	if(encrypt){
		safewrite(v2hdr, AESbsize);
		genrandom(buf,2*AESbsize); /* CBC is semantically secure if IV is unpredictable. */
		setupAESstate(&aes, key, nkey, buf);  /* use first AESbsize bytes as IV */
		aesCBCencrypt(buf+AESbsize, AESbsize, &aes);  /* use second AESbsize bytes as initial plaintext */
		safewrite(buf, 2*AESbsize);
		dstate = hmac_sha1(buf+AESbsize, AESbsize, key2, MD5dlen, 0, 0);
		for(;;){
			n = Bread(&bin, buf, BUF);
			if(n < 0){
				fprint(2,"read error\n");
				exits("read error");
			}
			aesCBCencrypt(buf, n, &aes);
			safewrite(buf, n);
			dstate = hmac_sha1(buf, n, key2, MD5dlen, 0, dstate);
			if(n < BUF)
				break; /* EOF */
		}
		hmac_sha1(0, 0, key2, MD5dlen, buf, dstate);
		safewrite(buf, SHA1dlen);
	}else{ /* decrypt */
		saferead(buf, AESbsize);
		if(memcmp(buf, v2hdr, AESbsize) != 0){
			fprint(2, "not an aescbc file\n");
			exits("aescbc file");
		}
		saferead(buf, 2*AESbsize);  /* read IV and random initial plaintext */
		setupAESstate(&aes, key, nkey, buf);
		dstate = hmac_sha1(buf+AESbsize, AESbsize, key2, MD5dlen, 0, 0);
		aesCBCdecrypt(buf+AESbsize, AESbsize, &aes);
		saferead(buf, SHA1dlen);
		while((n = Bread(&bin, buf+SHA1dlen, BUF)) > 0){
			dstate = hmac_sha1(buf, n, key2, MD5dlen, 0, dstate);
			aesCBCdecrypt(buf, n, &aes);
			safecopy(buf, n);
			memmove(buf, buf+n, SHA1dlen);  /* these bytes are not yet decrypted */
		}
		hmac_sha1(0, 0, key2, MD5dlen, buf+SHA1dlen, dstate);
		if(memcmp(buf, buf+SHA1dlen, SHA1dlen) != 0){
			fprint(2,"decrypted file failed to authenticate\n");
			exits("decrypted file failed to authenticate");
		}
		safewrite(copy, ncopy);
	}
	exits("");
	return 1;	/* gcc */
}
