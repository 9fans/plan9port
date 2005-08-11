#include <u.h>
#include <libc.h>
#include <bin.h>
#include <httpd.h>

typedef struct Strings		Strings;

struct Strings
{
	char	*s1;
	char	*s2;
};

static	char*		abspath(HConnect *cc, char *origpath, char *curdir);
static	int		getc(HConnect*);
static	char*		getword(HConnect*);
static	Strings		parseuri(HConnect *c, char*);
static	Strings		stripsearch(char*);

/*
 * parse the next request line
 * returns:
 *	1 ok
 *	0 eof
 *	-1 error
 */
int
hparsereq(HConnect *c, int timeout)
{
	Strings ss;
	char *vs, *v, *search, *uri, *origuri, *extra;

	if(c->bin != nil){
		hfail(c, HInternal);
		return -1;
	}

	/*
	 * serve requests until a magic request.
	 * later requests have to come quickly.
	 * only works for http/1.1 or later.
	 */
	if(timeout)
		alarm(timeout);
	if(hgethead(c, 0) < 0)
		return -1;
	if(timeout)
		alarm(0);
	c->reqtime = time(nil);
	c->req.meth = getword(c);
	if(c->req.meth == nil){
		hfail(c, HSyntax);
		return -1;
	}
	uri = getword(c);
	if(uri == nil || strlen(uri) == 0){
		hfail(c, HSyntax);
		return -1;
	}
	v = getword(c);
	if(v == nil){
		if(strcmp(c->req.meth, "GET") != 0){
			hfail(c, HUnimp, c->req.meth);
			return -1;
		}
		c->req.vermaj = 0;
		c->req.vermin = 9;
	}else{
		vs = v;
		if(strncmp(vs, "HTTP/", 5) != 0){
			hfail(c, HUnkVers, vs);
			return -1;
		}
		vs += 5;
		c->req.vermaj = strtoul(vs, &vs, 10);
		if(*vs != '.' || c->req.vermaj != 1){
			hfail(c, HUnkVers, vs);
			return -1;
		}
		vs++;
		c->req.vermin = strtoul(vs, &vs, 10);
		if(*vs != '\0'){
			hfail(c, HUnkVers, vs);
			return -1;
		}

		extra = getword(c);
		if(extra != nil){
			hfail(c, HSyntax);
			return -1;
		}
	}

	/*
	 * the fragment is not supposed to be sent
	 * strip it 'cause some clients send it
	 */
	origuri = uri;
	uri = strchr(origuri, '#');
	if(uri != nil)
		*uri = 0;

	/*
	 * http/1.1 requires the server to accept absolute
	 * or relative uri's.  convert to relative with an absolute path
	 */
	if(http11(c)){
		ss = parseuri(c, origuri);
		uri = ss.s1;
		c->req.urihost = ss.s2;
		if(uri == nil){
			hfail(c, HBadReq, uri);
			return -1;
		}
		origuri = uri;
	}

	/*
	 * munge uri for search, protection, and magic
	 */
	ss = stripsearch(origuri);
	origuri = ss.s1;
	search = ss.s2;
	uri = hurlunesc(c, origuri);
	uri = abspath(c, uri, "/");
	if(uri == nil || uri[0] == '\0'){
		hfail(c, HNotFound, "no object specified");
		return -1;
	}

	c->req.uri = uri;
	c->req.search = search;

	return 1;
}

static Strings
parseuri(HConnect *c, char *uri)
{
	Strings ss;
	char *urihost, *p;

	urihost = nil;
	if(uri[0] != '/'){
		if(cistrncmp(uri, "http://", 7) != 0){
			ss.s1 = nil;
			ss.s2 = nil;
			return ss;
		}
		uri += 5;	/* skip http: */
	}

	/*
	 * anything starting with // is a host name or number
	 * hostnames constists of letters, digits, - and .
	 * for now, just ignore any port given
	 */
	if(uri[0] == '/' && uri[1] == '/'){
		urihost = uri + 2;
		p = strchr(urihost, '/');
		if(p == nil)
			uri = hstrdup(c, "/");
		else{
			uri = hstrdup(c, p);
			*p = '\0';
		}
		p = strchr(urihost, ':');
		if(p != nil)
			*p = '\0';
	}

	if(uri[0] != '/' || uri[1] == '/'){
		ss.s1 = nil;
		ss.s2 = nil;
		return ss;
	}

	ss.s1 = uri;
	ss.s2 = hlower(urihost);
	return ss;
}
static Strings
stripsearch(char *uri)
{
	Strings ss;
	char *search;

	search = strchr(uri, '?');
	if(search != nil)
		*search++ = 0;
	ss.s1 = uri;
	ss.s2 = search;
	return ss;
}

/*
 *  to circumscribe the accessible files we have to eliminate ..'s
 *  and resolve all names from the root.
 */
static char*
abspath(HConnect *cc, char *origpath, char *curdir)
{
	char *p, *sp, *path, *work, *rpath;
	int len, n, c;

	if(curdir == nil)
		curdir = "/";
	if(origpath == nil)
		origpath = "";
	work = hstrdup(cc, origpath);
	path = work;

	/*
	 * remove any really special characters
	 */
	for(sp = "`;| "; *sp; sp++){
		p = strchr(path, *sp);
		if(p)
			*p = 0;
	}

	len = strlen(curdir) + strlen(path) + 2 + UTFmax;
	if(len < 10)
		len = 10;
	rpath = halloc(cc, len);
	if(*path == '/')
		rpath[0] = 0;
	else
		strcpy(rpath, curdir);
	n = strlen(rpath);

	while(path){
		p = strchr(path, '/');
		if(p)
			*p++ = 0;
		if(strcmp(path, "..") == 0){
			while(n > 1){
				n--;
				c = rpath[n];
				rpath[n] = 0;
				if(c == '/')
					break;
			}
		}else if(strcmp(path, ".") == 0){
			;
		}else if(n == 1)
			n += snprint(rpath+n, len-n, "%s", path);
		else
			n += snprint(rpath+n, len-n, "/%s", path);
		path = p;
	}

	if(strncmp(rpath, "/bin/", 5) == 0)
		strcpy(rpath, "/");
	return rpath;
}

static char*
getword(HConnect *c)
{
	char *buf;
	int ch, n;

	while((ch = getc(c)) == ' ' || ch == '\t' || ch == '\r')
		;
	if(ch == '\n')
		return nil;
	n = 0;
	buf = halloc(c, 1);
	for(;;){
		switch(ch){
		case ' ':
		case '\t':
		case '\r':
		case '\n':
			buf[n] = '\0';
			return hstrdup(c, buf);
		}

		if(n < HMaxWord-1){
			buf = bingrow(&c->bin, buf, n, n + 1, 0);
			if(buf == nil)
				return nil;
			buf[n++] = ch;
		}
		ch = getc(c);
	}
}

static int
getc(HConnect *c)
{
	if(c->hpos < c->hstop)
		return *c->hpos++;
	return '\n';
}
