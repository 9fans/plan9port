#include <u.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "a.h"

AUTOLIB(ssl)

static void
httpsinit(void)
{
	ERR_load_crypto_strings();
	ERR_load_SSL_strings();
	SSL_load_error_strings();
	SSL_library_init();
}

struct Pfd
{
	BIO *sbio;
};

static Pfd*
opensslconnect(char *host)
{
	Pfd *pfd;
	BIO *sbio;
	SSL_CTX *ctx;
	SSL *ssl;
	static int didinit;
	char buf[1024];

	if(!didinit){
		httpsinit();
		didinit = 1;
	}

	ctx = SSL_CTX_new(SSLv23_client_method());
	sbio = BIO_new_ssl_connect(ctx);
	BIO_get_ssl(sbio, &ssl);
	SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

	snprint(buf, sizeof buf, "%s:https", host);
	BIO_set_conn_hostname(sbio, buf);

	if(BIO_do_connect(sbio) <= 0 || BIO_do_handshake(sbio) <= 0){
		ERR_error_string_n(ERR_get_error(), buf, sizeof buf);
		BIO_free_all(sbio);
		werrstr("openssl: %s", buf);
		return nil;
	}

	pfd = emalloc(sizeof *pfd);
	pfd->sbio = sbio;
	return pfd;
}

static void
opensslclose(Pfd *pfd)
{
	if(pfd == nil)
		return;
	BIO_free_all(pfd->sbio);
	free(pfd);
}

static int
opensslwrite(Pfd *pfd, void *v, int n)
{
	int m, total;
	char *p;

	p = v;
	total = 0;
	while(total < n){
		if((m = BIO_write(pfd->sbio, p+total, n-total)) <= 0){
			if(total == 0)
				return m;
			return total;
		}
		total += m;
	}
	return total;
}

static int
opensslread(Pfd *pfd, void *v, int n)
{
	return BIO_read(pfd->sbio, v, n);
}

Protocol https =
{
	opensslconnect,
	opensslread,
	opensslwrite,
	opensslclose
};
