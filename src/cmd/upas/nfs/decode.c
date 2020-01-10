/* Quick and dirty RFC 2047 */

#include "a.h"

static int
unhex1(char c)
{
	if('0' <= c && c <= '9')
		return c-'0';
	if('a' <= c && c <= 'f')
		return c-'a'+10;
	if('A' <= c && c <= 'F')
		return c-'A'+10;
	return 15;
}

static int
unhex(char *s)
{
	return unhex1(s[0])*16+unhex1(s[1]);
}

int
_decqp(uchar *out, int lim, char *in, int n, int underscores)
{
	char *p, *ep;
	uchar *eout, *out0;

	out0 = out;
	eout = out+lim;
	for(p=in, ep=in+n; p<ep && out<eout; ){
		if(underscores && *p == '_'){
			*out++ = ' ';
			p++;
		}
		else if(*p == '='){
			if(p+1 >= ep)
				break;
			if(*(p+1) == '\n'){
				p += 2;
				continue;
			}
			if(p+3 > ep)
				break;
			*out++ = unhex(p+1);
			p += 3;
		}else
			*out++ = *p++;
	}
	return out-out0;
}

int
decqp(uchar *out, int lim, char *in, int n)
{
	return _decqp(out, lim, in, n, 0);
}

char*
decode(int kind, char *s, int *len)
{
	char *t;
	int l;

	if(s == nil)
		return s;
	switch(kind){
	case QuotedPrintable:
	case QuotedPrintableU:
		l = strlen(s)+1;
		t = emalloc(l);
		l = _decqp((uchar*)t, l, s, l-1, kind==QuotedPrintableU);
		*len = l;
		t[l] = 0;
		return t;

	case Base64:
		l = strlen(s)+1;
		t = emalloc(l);
		l = dec64((uchar*)t, l, s, l-1);
		*len = l;
		t[l] = 0;
		return t;

	default:
		*len = strlen(s);
		return estrdup(s);
	}
}

struct {
	char *mime;
	char *tcs;
} tcstab[] = {
	"iso-8859-2",		"8859-2",
	"iso-8859-3",		"8859-3",
	"iso-8859-4",		"8859-4",
	"iso-8859-5",		"8859-5",
	"iso-8859-6",		"8859-6",
	"iso-8859-7",		"8859-7",
	"iso-8859-8",		"8859-8",
	"iso-8859-9",		"8859-9",
	"iso-8859-10",	"8859-10",
	"iso-8859-15",	"8859-15",
	"big5",			"big5",
	"iso-2022-jp",	"jis-kanji",
	"windows-1250",	"windows-1250",
	"windows-1251",	"windows-1251",
	"windows-1252",	"windows-1252",
	"windows-1253",	"windows-1253",
	"windows-1254",	"windows-1254",
	"windows-1255",	"windows-1255",
	"windows-1256",	"windows-1256",
	"windows-1257",	"windows-1257",
	"windows-1258",	"windows-1258",
	"koi8-r",			"koi8"
};

typedef struct Writeargs Writeargs;
struct Writeargs
{
	int fd;
	char *s;
};

static void
twriter(void *v)
{
	Writeargs *w;

	w = v;
	write(w->fd, w->s, strlen(w->s));
	close(w->fd);
	free(w->s);
	free(w);
}

char*
tcs(char *charset, char *s)
{
	char *buf;
	int i, n, nbuf;
	int fd[3], p[2], pp[2];
	uchar *us;
	char *t, *u;
	Rune r;
	Writeargs *w;

	if(s == nil || charset == nil || *s == 0)
		return s;

	if(cistrcmp(charset, "utf-8") == 0)
		return s;
	if(cistrcmp(charset, "iso-8859-1") == 0 || cistrcmp(charset, "us-ascii") == 0){
latin1:
		n = 0;
		for(us=(uchar*)s; *us; us++)
			n += runelen(*us);
		n++;
		t = emalloc(n);
		for(us=(uchar*)s, u=t; *us; us++){
			r = *us;
			u += runetochar(u, &r);
		}
		*u = 0;
		free(s);
		return t;
	}
	for(i=0; i<nelem(tcstab); i++)
		if(cistrcmp(charset, tcstab[i].mime) == 0)
			goto tcs;
	goto latin1;

tcs:
	if(pipe(p) < 0 || pipe(pp) < 0)
		sysfatal("pipe: %r");
	fd[0] = p[0];
	fd[1] = pp[0];
	fd[2] = dup(2, -1);
	if(threadspawnl(fd, "tcs", "tcs", "-f", tcstab[i].tcs, nil) < 0){
		close(p[0]);
		close(p[1]);
		close(pp[0]);
		close(pp[1]);
		close(fd[2]);
		goto latin1;
	}
	close(p[0]);
	close(pp[0]);

	nbuf = UTFmax*strlen(s)+100;	/* just a guess at worst case */
	buf = emalloc(nbuf);

	w = emalloc(sizeof *w);
	w->fd = p[1];
	w->s = estrdup(s);
	proccreate(twriter, w, STACK);

	n = readn(pp[1], buf, nbuf-1);
	close(pp[1]);
	if(n <= 0){
		free(buf);
		goto latin1;
	}
	buf[n] = 0;
	free(s);
	s = estrdup(buf);
	free(buf);
	return s;
}

char*
unrfc2047(char *s)
{
	char *p, *q, *t, *u, *v;
	int len;
	Rune r;
	Fmt fmt;

	if(s == nil)
		return nil;

	if(strstr(s, "=?") == nil)
		return s;

	fmtstrinit(&fmt);
	for(p=s; *p; ){
		/* =?charset?e?text?= */
		if(*p=='=' && *(p+1)=='?'){
			p += 2;
			q = strchr(p, '?');
			if(q == nil)
				goto emit;
			q++;
			if(*q == '?' || *(q+1) != '?')
				goto emit;
			t = q+2;
			u = strchr(t, '?');
			if(u == nil || *(u+1) != '=')
				goto emit;
			switch(*q){
			case 'q':
			case 'Q':
				*u = 0;
				v = decode(QuotedPrintableU, t, &len);
				break;
			case 'b':
			case 'B':
				*u = 0;
				v = decode(Base64, t, &len);
				break;
			default:
				goto emit;
			}
			*(q-1) = 0;
			v = tcs(p, v);
			fmtstrcpy(&fmt, v);
			free(v);
			p = u+2;
		}
	emit:
		p += chartorune(&r, p);
		fmtrune(&fmt, r);
	}
	p = fmtstrflush(&fmt);
	if(p == nil)
		sysfatal("out of memory");
	free(s);
	return p;
}

#ifdef TEST
char *test[] =
{
	"hello world",
	"hello =?iso-8859-1?q?this is some text?=",
	"=?US-ASCII?Q?Keith_Moore?=",
	"=?ISO-8859-1?Q?Keld_J=F8rn_Simonsen?=",
	"=?ISO-8859-1?Q?Andr=E9?= Pirard",
	"=?ISO-8859-1?B?SWYgeW91IGNhbiByZWFkIHRoaXMgeW8=?=",
	"=?ISO-8859-2?B?dSB1bmRlcnN0YW5kIHRoZSBleGFtcGxlLg==?=",
	"=?ISO-8859-1?Q?Olle_J=E4rnefors?=",
	"=?iso-2022-jp?B?GyRCTTVKISRKP006SiRyS34kPyQ3JEZKcz03JCIkahsoQg==?=",
	"=?UTF-8?B?Ik5pbHMgTy4gU2Vsw6VzZGFsIg==?="
};

void
threadmain(int argc, char **argv)
{
	int i;

	for(i=0; i<nelem(test); i++)
		print("%s\n\t%s\n", test[i], unrfc2047(estrdup(test[i])));
	threadexitsall(0);
}

#endif
