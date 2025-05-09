#include "rc.h"
#include "exec.h"
#include "io.h"
#include "fns.h"

enum {
	NBUF = 8192,
};

void
vpfmt(io *f, char *fmt, va_list ap)
{
	for(;*fmt;fmt++) {
		if(*fmt!='%') {
			pchr(f, *fmt);
			continue;
		}
		if(*++fmt == '\0')		/* "blah%"? */
			break;
		switch(*fmt){
		case 'c':
			pchr(f, va_arg(ap, int));
			break;
		case 'd':
			pdec(f, va_arg(ap, int));
			break;
		case 'o':
			poct(f, va_arg(ap, unsigned));
			break;
		case 'p':
			pptr(f, va_arg(ap, void*));
			break;
		case 'Q':
			pquo(f, va_arg(ap, char *));
			break;
		case 'q':
			pwrd(f, va_arg(ap, char *));
			break;
		case 's':
			pstr(f, va_arg(ap, char *));
			break;
		case 't':
			pcmd(f, va_arg(ap, tree *));
			break;
		case 'v':
			pval(f, va_arg(ap, word *));
			break;
		default:
			pchr(f, *fmt);
			break;
		}
	}
}

void
pfmt(io *f, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vpfmt(f, fmt, ap);
	va_end(ap);
}

void
pchr(io *b, int c)
{
	if(b->bufp>=b->ebuf)
		flushio(b);
	*b->bufp++=c;
}

int
rchr(io *b)
{
	if(b->bufp>=b->ebuf)
		return emptyiobuf(b);
	return *b->bufp++;
}

char*
rstr(io *b, char *stop)
{
	char *s, *p;
	int l, m, n;

	do {
		l = rchr(b);
		if(l == EOF)
			return 0;
	} while(l && strchr(stop, l));
	b->bufp--;

	s = 0;
	l = 0;
	for(;;){
		p = (char*)b->bufp;
		n = (char*)b->ebuf - p;
		if(n > 0){
			for(m = 0; m < n; m++){
				if(strchr(stop, p[m])==0)
					continue;

				b->bufp += m+1;
				if(m > 0 || s==0){
					s = erealloc(s, l+m+1);
					memmove(s+l, p, m);
					l += m;
				}
				s[l]='\0';
				return s;
			}
			s = erealloc(s, l+m+1);
			memmove(s+l, p, m);
			l += m;
			b->bufp += m;
		}
		if(emptyiobuf(b) == EOF){
			if(s) s[l]='\0';
			return s;
		}
		b->bufp--;
	}
}

void
pquo(io *f, char *s)
{
	pchr(f, '\'');
	for(;*s;s++){
		if(*s=='\'')
			pchr(f, *s);
		pchr(f, *s);
	}
	pchr(f, '\'');
}

void
pwrd(io *f, char *s)
{
	char *t;
	for(t = s;*t;t++) if(*t >= 0 && (*t <= ' ' || strchr("`^#*[]=|\\?${}()'<>&;", *t))) break;
	if(t==s || *t)
		pquo(f, s);
	else pstr(f, s);
}

void
pptr(io *f, void *p)
{
	static char hex[] = "0123456789ABCDEF";
	unsigned long long v;
	int n;

	v = (unsigned long long)p;
	if(sizeof(v) == sizeof(p) && v>>32)
		for(n = 60;n>=32;n-=4) pchr(f, hex[(v>>n)&0xF]);
	for(n = 28;n>=0;n-=4) pchr(f, hex[(v>>n)&0xF]);
}

void
pstr(io *f, char *s)
{
	if(s==0)
		s="(null)";
	while(*s) pchr(f, *s++);
}

void
pdec(io *f, int n)
{
	if(n<0){
		n=-n;
		if(n>=0){
			pchr(f, '-');
			pdec(f, n);
			return;
		}
		/* n is two's complement minimum integer */
		n = 1-n;
		pchr(f, '-');
		pdec(f, n/10);
		pchr(f, n%10+'1');
		return;
	}
	if(n>9)
		pdec(f, n/10);
	pchr(f, n%10+'0');
}

void
poct(io *f, unsigned n)
{
	if(n>7)
		poct(f, n>>3);
	pchr(f, (n&7)+'0');
}

void
pval(io *f, word *a)
{
	if(a==0)
		return;
	while(a->next && a->next->word){
		pwrd(f, (char *)a->word);
		pchr(f, ' ');
		a = a->next;
	}
	pwrd(f, (char *)a->word);
}

io*
newio(unsigned char *buf, int len, int fd)
{
	io *f = new(io);
	f->buf = buf;
	f->bufp = buf;
	f->ebuf = buf+len;
	f->fd = fd;
	return f;
}

/*
 * Open a string buffer for writing.
 */
io*
openiostr(void)
{
	unsigned char *buf = emalloc(100+1);
	memset(buf, '\0', 100+1);
	return newio(buf, 100, -1);
}

/*
 * Return the buf, free the io
 */
char*
closeiostr(io *f)
{
	void *buf = f->buf;
	free(f);
	return buf;
}

/*
 * Use a open file descriptor for reading.
 */
io*
openiofd(int fd)
{
	return newio(emalloc(NBUF), 0, fd);
}

/*
 * Open a corebuffer to read.  EOF occurs after reading len
 * characters from buf.
 */
io*
openiocore(void *buf, int len)
{
	return newio(buf, len, -1);
}

void
flushio(io *f)
{
	int n;

	if(f->fd<0){
		n = f->ebuf - f->buf;
		f->buf = erealloc(f->buf, n+n+1);
		f->bufp = f->buf + n;
		f->ebuf = f->bufp + n;
		memset(f->bufp, '\0', n+1);
	}
	else{
		n = f->bufp - f->buf;
		if(n && Write(f->fd, f->buf, n) != n){
			if(ntrap)
				dotrap();
		}
		f->bufp = f->buf;
		f->ebuf = f->buf+NBUF;
	}
}

void
closeio(io *f)
{
	if(f->fd>=0) Close(f->fd);
	free(closeiostr(f));
}

int
emptyiobuf(io *f)
{
	int n;
	if(f->fd<0 || (n = Read(f->fd, f->buf, NBUF))<=0) return EOF;
	f->bufp = f->buf;
	f->ebuf = f->buf + n;
	return *f->bufp++;
}
