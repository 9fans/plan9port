/* encrypt file by writing
	v2hdr,
	16byte initialization vector,
	AES-CBC(key, random | file),
    HMAC_SHA1(md5(key), AES-CBC(random | file))

With CBC, if the first plaintext block is 0, the first ciphertext block is
E(IV).  Using the overflow technique adopted for compatibility with cryptolib
makes the last cipertext block decryptable.  Hence the random prefix to file.
*/
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mp.h>
#include <libsec.h>

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

int
main(int argc, char **argv)
{
	int encrypt = 0;  /* 0=decrypt, 1=encrypt */
	int n, nkey;
	char *hex, *msg = nil;
	uchar key[AESmaxkey], key2[MD5dlen];
	uchar buf[BUF+SHA1dlen];    /* assumption: CHK <= SHA1dlen */
	AESstate aes;
	DigestState *dstate;

	if(argc!=2 || argv[1][0]!='-'){
		fprint(2,"usage: HEX=key %s -d < cipher.aes > clear.txt\n", argv[0]);
		fprint(2,"   or: HEX=key %s -e < clear.txt > cipher.aes\n", argv[0]);
		exits("usage");
	}
	if(argv[1][1] == 'e')
		encrypt = 1;
	Binit(&bin, 0, OREAD);
	Binit(&bout, 1, OWRITE);

	if((hex = getenv("HEX")) == nil)
		hex = getpass("enter key: ");
	nkey = 0;
	if(hex != nil)
		nkey = strlen(hex);
	if(nkey == 0 || (nkey&1) || nkey>2*AESmaxkey){
		fprint(2,"key should be 32 hex digits\n");
		exits("key");
	}
	nkey = dec16(key, sizeof key, hex, nkey);
	md5(key, nkey, key2, 0);  /* so even if HMAC_SHA1 is broken, encryption key is protected */

	if(encrypt){
		safewrite(v2hdr, AESbsize);
		genrandom(buf,2*AESbsize); /* CBC is semantically secure if IV is unpredictable. */
		setupAESstate(&aes, key, nkey, buf);  /* use first AESbsize bytes as IV */
		aesCBCencrypt(buf+AESbsize, AESbsize, &aes);  /* use second AESbsize bytes as initial plaintext */
		safewrite(buf, 2*AESbsize);
		dstate = hmac_sha1(buf+AESbsize, AESbsize, key2, MD5dlen, 0, 0);
		while(1){
			n = Bread(&bin, buf, BUF);
			if(n < 0){
				msg = "read error";
				goto Exit;
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
		Bread(&bin, buf, AESbsize);
		if(memcmp(buf, v2hdr, AESbsize) == 0){
			saferead(buf, 2*AESbsize);  /* read IV and random initial plaintext */
			setupAESstate(&aes, key, nkey, buf);
			dstate = hmac_sha1(buf+AESbsize, AESbsize, key2, MD5dlen, 0, 0);
			aesCBCdecrypt(buf+AESbsize, AESbsize, &aes);
			saferead(buf, SHA1dlen);
			while((n = Bread(&bin, buf+SHA1dlen, BUF)) > 0){
				dstate = hmac_sha1(buf, n, key2, MD5dlen, 0, dstate);
				aesCBCdecrypt(buf, n, &aes);
				safewrite(buf, n);
				memmove(buf, buf+n, SHA1dlen);  /* these bytes are not yet decrypted */
			}
			hmac_sha1(0, 0, key2, MD5dlen, buf+SHA1dlen, dstate);
			if(memcmp(buf, buf+SHA1dlen, SHA1dlen) != 0){
				msg = "decrypted file failed to authenticate!";
				goto Exit;
			}
		}else{ /* compatibility with past mistake */
			// if file was encrypted with bad aescbc use this:
			//         memset(key, 0, AESmaxkey);
			//    else assume we're decrypting secstore files
			setupAESstate(&aes, key, 0, buf);
			saferead(buf, CHK);
			aesCBCdecrypt(buf, CHK, &aes);
			while((n = Bread(&bin, buf+CHK, BUF)) > 0){
				aesCBCdecrypt(buf+CHK, n, &aes);
				safewrite(buf, n);
				memmove(buf, buf+n, CHK);
			}
			if(memcmp(buf, "XXXXXXXXXXXXXXXX", CHK) != 0){
				msg = "decrypted file failed to authenticate";
				goto Exit;
			}
		}
	}
 Exit:
	memset(key, 0, sizeof(key));
	memset(key2, 0, sizeof(key2));
	memset(buf, 0, sizeof(buf));
	if(msg != nil)
		fprint(2, "%s\n", msg);
	exits(msg);
	return 1;	/* gcc */
}
