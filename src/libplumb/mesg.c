#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <fs.h>
#include "plumb.h"

static char attrbuf[4096];

char *home;
static Fsys *fsplumb;
static int pfd = -1;
static Fid *pfid;
int
plumbopen(char *name, int omode)
{
	int fd;

	if(fsplumb == nil)
		fsplumb = nsmount("plumb", "");
	if(fsplumb == nil)
		return -1;
	/*
	 * It's important that when we send something,
	 * we find out whether it was a valid plumb write.
	 * (If it isn't, the client might fall back to some
	 * other mechanism or indicate to the user what happened.)
	 * We can't use a pipe for this, so we have to use the
	 * fid interface.  But we need to return a fd. 
	 * Return a fd for /dev/null so that we return a unique
	 * file descriptor.  In plumbsend we'll look for pfd
	 * and use the recorded fid instead.
	 */
	if((omode&3) == OWRITE){
		if(pfd != -1){
			werrstr("already have plumb send open");
			return -1;
		}
		pfd = open("/dev/null", OWRITE);
		if(pfd < 0)
			return -1;
		pfid = fsopen(fsplumb, name, omode);
		if(pfid == nil){
			close(pfd);
			pfd = -1;
			return -1;
		}
		return pfd;
	}

	fd = fsopenfd(fsplumb, name, omode);
	return fd;
}

int
plumbsend(int fd, Plumbmsg *m)
{
	char *buf;
	int n;

	if(fd != pfd){
		werrstr("fd is not the plumber");
		return -1;
	}
	buf = plumbpack(m, &n);
	if(buf == nil)
		return -1;
	n = fswrite(pfid, buf, n);
	free(buf);
	return n;
}

static int
Strlen(char *s)
{
	if(s == nil)
		return 0;
	return strlen(s);
}

static char*
Strcpy(char *s, char *t)
{
	if(t == nil)
		return s;
	return strcpy(s, t) + strlen(t);
}

/* quote attribute value, if necessary */
static char*
quote(char *s)
{
	char *t;
	int c;

	if(s == nil){
		attrbuf[0] = '\0';
		return attrbuf;
	}
	if(strpbrk(s, " '=\t") == nil)
		return s;
	t = attrbuf;
	*t++ = '\'';
	while(t < attrbuf+sizeof attrbuf-2){
		c = *s++;
		if(c == '\0')
			break;
		*t++ = c;
		if(c == '\'')
			*t++ = c;
	}
	*t++ = '\'';
	*t = '\0';
	return attrbuf;
}

char*
plumbpackattr(Plumbattr *attr)
{
	int n;
	Plumbattr *a;
	char *s, *t;

	if(attr == nil)
		return nil;
	n = 0;
	for(a=attr; a!=nil; a=a->next)
		n += Strlen(a->name) + 1 + Strlen(quote(a->value)) + 1;
	s = malloc(n);
	if(s == nil)
		return nil;
	t = s;
	*t = '\0';
	for(a=attr; a!=nil; a=a->next){
		if(t != s)
			*t++ = ' ';
		strcpy(t, a->name);
		strcat(t, "=");
		strcat(t, quote(a->value));
		t += strlen(t);
	}
	if(t > s+n)
		abort();
	return s;
}

char*
plumblookup(Plumbattr *attr, char *name)
{
	while(attr){
		if(strcmp(attr->name, name) == 0)
			return attr->value;
		attr = attr->next;
	}
	return nil;
}

char*
plumbpack(Plumbmsg *m, int *np)
{
	int n, ndata;
	char *buf, *p, *attr;

	ndata = m->ndata;
	if(ndata < 0)
		ndata = Strlen(m->data);
	attr = plumbpackattr(m->attr);
	n = Strlen(m->src)+1 + Strlen(m->dst)+1 + Strlen(m->wdir)+1 +
		Strlen(m->type)+1 + Strlen(attr)+1 + 16 + ndata;
	buf = malloc(n+1);	/* +1 for '\0' */
	if(buf == nil){
		free(attr);
		return nil;
	}
	p = Strcpy(buf, m->src);
	*p++ = '\n';
	p = Strcpy(p, m->dst);
	*p++ = '\n';
	p = Strcpy(p, m->wdir);
	*p++ = '\n';
	p = Strcpy(p, m->type);
	*p++ = '\n';
	p = Strcpy(p, attr);
	*p++ = '\n';
	p += sprint(p, "%d\n", ndata);
	memmove(p, m->data, ndata);
	*np = (p-buf)+ndata;
	buf[*np] = '\0';	/* null terminate just in case */
	if(*np >= n+1)
		abort();
	free(attr);
	return buf;
}

static int
plumbline(char **linep, char *buf, int i, int n, int *bad)
{
	int starti;
	char *p;

	if(*bad)
		return i;
	starti = i;
	while(i<n && buf[i]!='\n')
		i++;
	if(i == n)
		*bad = 1;
	else{
		p = malloc((i-starti) + 1);
		if(p == nil)
			*bad = 1;
		else{
			memmove(p, buf+starti, i-starti);
			p[i-starti] = '\0';
		}
		*linep = p;
		i++;
	}
	return i;
}

void
plumbfree(Plumbmsg *m)
{
	Plumbattr *a, *next;

	free(m->src);
	free(m->dst);
	free(m->wdir);
	free(m->type);
	for(a=m->attr; a!=nil; a=next){
		next = a->next;
		free(a->name);
		free(a->value);
		free(a);
	}
	free(m->data);
	free(m);
}

Plumbattr*
plumbunpackattr(char *p)
{
	Plumbattr *attr, *prev, *a;
	char *q, *v;
	int c, quoting;

	attr = prev = nil;
	while(*p!='\0' && *p!='\n'){
		while(*p==' ' || *p=='\t')
			p++;
		if(*p == '\0')
			break;
		for(q=p; *q!='\0' && *q!='\n' && *q!=' ' && *q!='\t'; q++)
			if(*q == '=')
				break;
		if(*q != '=')
			break;	/* malformed attribute */
		a = malloc(sizeof(Plumbattr));
		if(a == nil)
			break;
		a->name = malloc(q-p+1);
		if(a->name == nil){
			free(a);
			break;
		}
		memmove(a->name, p, q-p);
		a->name[q-p] = '\0';
		/* process quotes in value */
		q++;	/* skip '=' */
		v = attrbuf;
		quoting = 0;
		while(*q!='\0' && *q!='\n'){
			if(v >= attrbuf+sizeof attrbuf)
				break;
			c = *q++;
			if(quoting){
				if(c == '\''){
					if(*q == '\'')
						q++;
					else{
						quoting = 0;
						continue;
					}
				}
			}else{
				if(c==' ' || c=='\t')
					break;
				if(c == '\''){
					quoting = 1;
					continue;
				}
			}
			*v++ = c;
		}
		a->value = malloc(v-attrbuf+1);
		if(a->value == nil){
			free(a->name);
			free(a);
			break;
		}
		memmove(a->value, attrbuf, v-attrbuf);
		a->value[v-attrbuf] = '\0';
		a->next = nil;
		if(prev == nil)
			attr = a;
		else
			prev->next = a;
		prev = a;
		p = q;
	}
	return attr;
}

Plumbattr*
plumbaddattr(Plumbattr *attr, Plumbattr *new)
{
	Plumbattr *l;

	l = attr;
	if(l == nil)
		return new;
	while(l->next != nil)
		l = l->next;
	l->next = new;
	return attr;
}

Plumbattr*
plumbdelattr(Plumbattr *attr, char *name)
{
	Plumbattr *l, *prev;

	prev = nil;
	for(l=attr; l!=nil; l=l->next){
		if(strcmp(name, l->name) == 0)
			break;
		prev = l;
	}
	if(l == nil)
		return nil;
	if(prev)
		prev->next = l->next;
	else
		attr = l->next;
	free(l->name);
	free(l->value);
	free(l);
	return attr;
}

Plumbmsg*
plumbunpackpartial(char *buf, int n, int *morep)
{
	Plumbmsg *m;
	int i, bad;
	char *ntext, *attr;

	m = malloc(sizeof(Plumbmsg));
	if(m == nil)
		return nil;
	memset(m, 0, sizeof(Plumbmsg));
	if(morep != nil)
		*morep = 0;
	bad = 0;
	i = plumbline(&m->src, buf, 0, n, &bad);
	i = plumbline(&m->dst, buf, i, n, &bad);
	i = plumbline(&m->wdir, buf, i, n, &bad);
	i = plumbline(&m->type, buf, i, n, &bad);
	i = plumbline(&attr, buf, i, n, &bad);
	m->attr = plumbunpackattr(attr);
	free(attr);
	i = plumbline(&ntext, buf, i, n, &bad);
	m->ndata = atoi(ntext);
	if(m->ndata != n-i){
		bad = 1;
		if(morep!=nil && m->ndata>n-i)
			*morep = m->ndata - (n-i);
	}
	free(ntext);
	if(!bad){
		m->data = malloc(n-i+1);	/* +1 for '\0' */
		if(m->data == nil)
			bad = 1;
		else{
			memmove(m->data, buf+i, m->ndata);
			m->ndata = n-i;
			/* null-terminate in case it's text */
			m->data[m->ndata] = '\0';
		}
	}
	if(bad){
		plumbfree(m);
		m = nil;
	}
	return m;
}

Plumbmsg*
plumbunpack(char *buf, int n)
{
	return plumbunpackpartial(buf, n, nil);
}

Plumbmsg*
plumbrecv(int fd)
{
	char *buf;
	Plumbmsg *m;
	int n, more;

	buf = malloc(8192);
	if(buf == nil)
		return nil;
	n = read(fd, buf, 8192);
	m = nil;
	if(n > 0){
		m = plumbunpackpartial(buf, n, &more);
		if(m==nil && more>0){
			/* we now know how many more bytes to read for complete message */
			buf = realloc(buf, n+more);
			if(buf == nil)
				return nil;
			if(readn(fd, buf+n, more) == more)
				m = plumbunpackpartial(buf, n+more, nil);
		}
	}
	free(buf);
	return m;
}
