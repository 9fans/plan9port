#include "dat.h"
#include <mp.h>
#include <libsec.h>

typedef struct Sshrsastate Sshrsastate;

enum {
	CReadpub,
	CWritechal,
	CReadresp,
};
struct State
{
	RSApriv *priv;
	Key *k;
	mpint *resp;
	int phase;
};

static RSApriv*
readrsapriv(char *s)
{
	RSApriv *priv;

	priv = rsaprivalloc();

	strtoul(s, &s, 10);
	if((priv->pub.ek=strtomp(s, &s, 16, nil)) == nil)
		goto Error;
	if((priv->dk=strtomp(s, &s, 16, nil)) == nil)
		goto Error;
	if((priv->pub.n=strtomp(s, &s, 16, nil)) == nil)
		goto Error;
	if((priv->p=strtomp(s, &s, 16, nil)) == nil)
		goto Error;
	if((priv->q=strtomp(s, &s, 16, nil)) == nil)
		goto Error;
	if((priv->kp=strtomp(s, &s, 16, nil)) == nil)
		goto Error;
	if((priv->kq=strtomp(s, &s, 16, nil)) == nil)
		goto Error;
	if((priv->c2=strtomp(s, &s, 16, nil)) == nil)
		goto Error;

	return priv;

Error:
	rsaprivfree(priv);
	return nil;
}

int
sshinit(Fsstate *fss, 
sshrsaopen(Key *k, char*, int client)
{
	Sshrsastate *s;

	fmtinstall('B', mpconv);
	assert(client);
	s = emalloc(sizeof *s);
	s->priv = readrsapriv(s_to_c(k->data));
	s->k = k;
	if(s->priv == nil){
		agentlog("error parsing ssh key %s", k->file);
		free(s);
		return nil;
	}
	return s;
}

int
sshrsaread(void *va, void *buf, int n)
{
	Sshrsastate *s;

	s = va;
	switch(s->phase){
	case Readpub:
		s->phase = Done;
		return snprint(buf, n, "%B", s->priv->pub.n);
	case Readresp:
		s->phase = Done;
		return snprint(buf, n, "%B", s->resp);
	default:
		return 0;
	}
}

int
sshrsawrite(void *va, void *vbuf, int n)
{
	mpint *m;
	char *buf;
	Sshrsastate *s;

	s = va;
	if((s->k->flags&Fconfirmuse) && confirm("ssh use") < 0)
		return -1;

	buf = emalloc(n+1);
	memmove(buf, vbuf, n);
	buf[n] = '\0';
	m = strtomp(buf, nil, 16, nil);
	free(buf);
	if(m == nil){
		werrstr("bad bignum");
		return -1;
	}

	agentlog("ssh use");
	m = rsadecrypt(s->priv, m, m);
	s->resp = m;
	s->phase = Readresp;
	return n;
}

void
sshrsaclose(void *v)
{
	Sshrsastate *s;

	s = v;
	rsaprivfree(s->priv);
	mpfree(s->resp);
	free(s);
}

Proto sshrsa = {
.name=	"ssh-rsa",
.perm=	0666,
.open=	sshrsaopen,
.read=	sshrsaread,
.write=	sshrsawrite,
.close=	sshrsaclose,
};
