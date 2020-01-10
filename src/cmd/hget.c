#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <bio.h>
#include <ip.h>
#include <libsec.h>
#include <auth.h>
#include <thread.h>

typedef struct URL URL;
struct URL
{
	int	method;
	char	*host;
	char	*port;
	char	*page;
	char	*etag;
	char	*redirect;
	char	*postbody;
	char	*cred;
	long	mtime;
};

typedef struct Range Range;
struct Range
{
	long	start;	/* only 2 gig supported, tdb */
	long	end;
};

typedef struct Out Out;
struct Out
{
	int fd;
	int offset;				/* notional current offset in output */
	int written;			/* number of bytes successfully transferred to output */
	DigestState *curr;		/* digest state up to offset (if known) */
	DigestState *hiwat;		/* digest state of all bytes written */
};

enum
{
	Http,
	Https,
	Ftp,
	Other
};

enum
{
	Eof = 0,
	Error = -1,
	Server = -2,
	Changed = -3
};

int debug;
char *ofile;


int	doftp(URL*, URL*, Range*, Out*, long);
int	dohttp(URL*, URL*,  Range*, Out*, long);
int	crackurl(URL*, char*);
Range*	crackrange(char*);
int	getheader(int, char*, int);
int	httpheaders(int, int, URL*, Range*);
int	httprcode(int);
int	cistrncmp(char*, char*, int);
int	cistrcmp(char*, char*);
void	initibuf(void);
int	readline(int, char*, int);
int	readibuf(int, char*, int);
int	dfprint(int, char*, ...);
void	unreadline(char*);
int	output(Out*, char*, int);
void	setoffset(Out*, int);

int	verbose;
char	*net;
char	tcpdir[NETPATHLEN];
int	headerprint;

struct {
	char	*name;
	int	(*f)(URL*, URL*, Range*, Out*, long);
} method[] = {
	{ "http",	dohttp },
	{ "https",	dohttp },
	{ "ftp",	doftp },
	{ "_______",	nil },
};

void
usage(void)
{
	fprint(2, "usage: %s [-hv] [-o outfile] [-p body] [-x netmtpt] url\n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char **argv)
{
	URL u;
	Range r;
	int errs, n;
	ulong mtime;
	Dir *d;
	char postbody[4096], *p, *e, *t, *hpx;
	URL px; /* Proxy */
	Out out;

	ofile = nil;
	p = postbody;
	e = p + sizeof(postbody);
	r.start = 0;
	r.end = -1;
	mtime = 0;
	memset(&u, 0, sizeof(u));
	memset(&px, 0, sizeof(px));
	hpx = getenv("httpproxy");

	ARGBEGIN {
	case 'o':
		ofile = ARGF();
		break;
	case 'd':
		debug = 1;
		break;
	case 'h':
		headerprint = 1;
		break;
	case 'v':
		verbose = 1;
		break;
	case 'x':
		net = ARGF();
		if(net == nil)
			usage();
		break;
	case 'p':
		t = ARGF();
		if(t == nil)
			usage();
		if(p != postbody)
			p = seprint(p, e, "&%s", t);
		else
			p = seprint(p, e, "%s", t);
		u.postbody = postbody;

		break;
	default:
		usage();
	} ARGEND;

	if(net != nil){
		if(strlen(net) > sizeof(tcpdir)-5)
			sysfatal("network mount point too long");
		snprint(tcpdir, sizeof(tcpdir), "%s/tcp", net);
	} else
		snprint(tcpdir, sizeof(tcpdir), "tcp");

	if(argc != 1)
		usage();


	out.fd = 1;
	out.written = 0;
	out.offset = 0;
	out.curr = nil;
	out.hiwat = nil;
	if(ofile != nil){
		d = dirstat(ofile);
		if(d == nil){
			out.fd = create(ofile, OWRITE, 0664);
			if(out.fd < 0)
				sysfatal("creating %s: %r", ofile);
		} else {
			out.fd = open(ofile, OWRITE);
			if(out.fd < 0)
				sysfatal("can't open %s: %r", ofile);
			r.start = d->length;
			mtime = d->mtime;
			free(d);
		}
	}

	errs = 0;

	if(crackurl(&u, argv[0]) < 0)
		sysfatal("%r");
	if(hpx && crackurl(&px, hpx) < 0)
		sysfatal("%r");

	for(;;){
		setoffset(&out, 0);
		/* transfer data */
		werrstr("");
		n = (*method[u.method].f)(&u, &px, &r, &out, mtime);

		switch(n){
		case Eof:
			threadexitsall(0);
			break;
		case Error:
			if(errs++ < 10)
				continue;
			sysfatal("too many errors with no progress %r");
			break;
		case Server:
			sysfatal("server returned: %r");
			break;
		}

		/* forward progress */
		errs = 0;
		r.start += n;
		if(r.start >= r.end)
			break;
	}

	threadexitsall(0);
}

int
crackurl(URL *u, char *s)
{
	char *p;
	int i;

	if(u->host != nil){
		free(u->host);
		u->host = nil;
	}
	if(u->page != nil){
		free(u->page);
		u->page = nil;
	}

	/* get type */
	u->method = Other;
	for(p = s; *p; p++){
		if(*p == '/'){
			u->method = Http;
			p = s;
			break;
		}
		if(*p == ':' && *(p+1)=='/' && *(p+2)=='/'){
			*p = 0;
			p += 3;
			for(i = 0; i < nelem(method); i++){
				if(cistrcmp(s, method[i].name) == 0){
					u->method = i;
					break;
				}
			}
			break;
		}
	}

	if(u->method == Other){
		werrstr("unsupported URL type %s", s);
		return -1;
	}

	/* get system */
	s = p;
	p = strchr(s, '/');
	if(p == nil){
		u->host = strdup(s);
		u->page = strdup("/");
	} else {
		u->page = strdup(p);
		*p = 0;
		u->host = strdup(s);
		*p = '/';
	}

	if(p = strchr(u->host, ':')) {
		*p++ = 0;
		u->port = p;
	} else
		u->port = method[u->method].name;

	if(*(u->host) == 0){
		werrstr("bad url, null host");
		return -1;
	}

	return 0;
}

char *day[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

char *month[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

struct
{
	int	fd;
	long	mtime;
} note;

void
catch(void *v, char *s)
{
	Dir d;

	USED(v);
	USED(s);

	nulldir(&d);
	d.mtime = note.mtime;
	if(dirfwstat(note.fd, &d) < 0)
		sysfatal("catch: can't dirfwstat: %r");
	noted(NDFLT);
}

int
dohttp(URL *u, URL *px, Range *r, Out *out, long mtime)
{
	int fd, cfd;
	int redirect, auth, loop;
	int n, rv, code;
	long tot, vtime;
	Tm *tm;
	char buf[1024];
	char err[ERRMAX];


	/*  always move back to a previous 512 byte bound because some
	 *  servers can't seem to deal with requests that start at the
	 *  end of the file
	 */
	if(r->start)
		r->start = ((r->start-1)/512)*512;

	/* loop for redirects, requires reading both response code and headers */
	fd = -1;
	for(loop = 0; loop < 32; loop++){
		if(px->host == nil){
			fd = dial(netmkaddr(u->host, tcpdir, u->port), 0, 0, 0);
		} else {
			fd = dial(netmkaddr(px->host, tcpdir, px->port), 0, 0, 0);
		}
		if(fd < 0)
			return Error;

		if(u->method == Https){
			int tfd;
			TLSconn conn;

			memset(&conn, 0, sizeof conn);
			tfd = tlsClient(fd, &conn);
			if(tfd < 0){
				fprint(2, "tlsClient: %r\n");
				close(fd);
				return Error;
			}
			/* BUG: check cert here? */
			if(conn.cert)
				free(conn.cert);
			close(fd);
			fd = tfd;
		}

		/* write request, use range if not start of file */
		if(u->postbody == nil){
			if(px->host == nil){
				dfprint(fd,	"GET %s HTTP/1.0\r\n"
						"Host: %s\r\n"
						"User-agent: Plan9/hget\r\n"
						"Cache-Control: no-cache\r\n"
						"Pragma: no-cache\r\n",
						u->page, u->host);
			} else {
				dfprint(fd,	"GET http://%s%s HTTP/1.0\r\n"
						"Host: %s\r\n"
						"User-agent: Plan9/hget\r\n"
						"Cache-Control: no-cache\r\n"
						"Pragma: no-cache\r\n",
						u->host, u->page, u->host);
			}
			if(u->cred)
				dfprint(fd,	"Authorization: Basic %s\r\n",
						u->cred);
		} else {
			if(px->host == nil){
				dfprint(fd,	"POST %s HTTP/1.0\r\n"
						"Host: %s\r\n"
						"Content-type: application/x-www-form-urlencoded\r\n"
						"Content-length: %d\r\n"
						"User-agent: Plan9/hget\r\n"
						"\r\n",
						u->page, u->host, strlen(u->postbody));
			} else {
				dfprint(fd, "POST http://%s%s HTTP/1.0\r\n"
						"Host: %s\r\n"
						"Content-type: application/x-www-form-urlencoded\r\n"
						"Content-length: %d\r\n"
						"User-agent: Plan9/hget\r\n"
						"\r\n",
						u->host, u->page, u->host, strlen(u->postbody));
			}
			dfprint(fd,	"%s", u->postbody);
		}
		if(r->start != 0){
			dfprint(fd, "Range: bytes=%d-\n", r->start);
			if(u->etag != nil){
				dfprint(fd, "If-range: %s\n", u->etag);
			} else {
				tm = gmtime(mtime);
				dfprint(fd, "If-range: %s, %d %s %d %2d:%2.2d:%2.2d GMT\n",
					day[tm->wday], tm->mday, month[tm->mon],
					tm->year+1900, tm->hour, tm->min, tm->sec);
			}
		}
		if((cfd = open("/mnt/webcookies/http", ORDWR)) >= 0){
			if(fprint(cfd, "http://%s%s", u->host, u->page) > 0){
				while((n = read(cfd, buf, sizeof buf)) > 0){
					if(debug)
						write(2, buf, n);
					write(fd, buf, n);
				}
			}else{
				close(cfd);
				cfd = -1;
			}
		}

		dfprint(fd, "\r\n", u->host);

		auth = 0;
		redirect = 0;
		initibuf();
		code = httprcode(fd);
		switch(code){
		case Error:	/* connection timed out */
		case Eof:
			close(fd);
			close(cfd);
			return code;

		case 200:	/* OK */
		case 201:	/* Created */
		case 202:	/* Accepted */
			if(ofile == nil && r->start != 0)
				sysfatal("page changed underfoot");
			break;

		case 204:	/* No Content */
			sysfatal("No Content");

		case 206:	/* Partial Content */
			setoffset(out, r->start);
			break;

		case 301:	/* Moved Permanently */
		case 302:	/* Moved Temporarily */
			redirect = 1;
			u->postbody = nil;
			break;

		case 304:	/* Not Modified */
			break;

		case 400:	/* Bad Request */
			sysfatal("Bad Request");

		case 401:	/* Unauthorized */
			if (auth)
				sysfatal("Authentication failed");
			auth = 1;
			break;

		case 402:	/* ??? */
			sysfatal("Unauthorized");

		case 403:	/* Forbidden */
			sysfatal("Forbidden by server");

		case 404:	/* Not Found */
			sysfatal("Not found on server");

		case 407:	/* Proxy Authentication */
			sysfatal("Proxy authentication required");

		case 500:	/* Internal server error */
			sysfatal("Server choked");

		case 501:	/* Not implemented */
			sysfatal("Server can't do it!");

		case 502:	/* Bad gateway */
			sysfatal("Bad gateway");

		case 503:	/* Service unavailable */
			sysfatal("Service unavailable");

		default:
			sysfatal("Unknown response code %d", code);
		}

		if(u->redirect != nil){
			free(u->redirect);
			u->redirect = nil;
		}

		rv = httpheaders(fd, cfd, u, r);
		close(cfd);
		if(rv != 0){
			close(fd);
			return rv;
		}

		if(!redirect && !auth)
			break;

		if (redirect){
			if(u->redirect == nil)
				sysfatal("redirect: no URL");
			if(crackurl(u, u->redirect) < 0)
				sysfatal("redirect: %r");
		}
	}

	/* transfer whatever you get */
	if(ofile != nil && u->mtime != 0){
		note.fd = out->fd;
		note.mtime = u->mtime;
		notify(catch);
	}

	tot = 0;
	vtime = 0;
	for(;;){
		n = readibuf(fd, buf, sizeof(buf));
		if(n <= 0)
			break;
		if(output(out, buf, n) != n)
			break;
		tot += n;
		if(verbose && (vtime != time(0) || r->start == r->end)) {
			vtime = time(0);
			fprint(2, "%ld %ld\n", r->start+tot, r->end);
		}
	}
	notify(nil);
	close(fd);

	if(ofile != nil && u->mtime != 0){
		Dir d;

		rerrstr(err, sizeof err);
		nulldir(&d);
		d.mtime = u->mtime;
		if(dirfwstat(out->fd, &d) < 0)
			fprint(2, "couldn't set mtime: %r\n");
		errstr(err, sizeof err);
	}

	return tot;
}

/* get the http response code */
int
httprcode(int fd)
{
	int n;
	char *p;
	char buf[256];

	n = readline(fd, buf, sizeof(buf)-1);
	if(n <= 0)
		return n;
	if(debug)
		fprint(2, "%d <- %s\n", fd, buf);
	p = strchr(buf, ' ');
	if(strncmp(buf, "HTTP/", 5) != 0 || p == nil){
		werrstr("bad response from server");
		return -1;
	}
	buf[n] = 0;
	return atoi(p+1);
}

/* read in and crack the http headers, update u and r */
void	hhetag(char*, URL*, Range*);
void	hhmtime(char*, URL*, Range*);
void	hhclen(char*, URL*, Range*);
void	hhcrange(char*, URL*, Range*);
void	hhuri(char*, URL*, Range*);
void	hhlocation(char*, URL*, Range*);
void	hhauth(char*, URL*, Range*);

struct {
	char *name;
	void (*f)(char*, URL*, Range*);
} headers[] = {
	{ "etag:", hhetag },
	{ "last-modified:", hhmtime },
	{ "content-length:", hhclen },
	{ "content-range:", hhcrange },
	{ "uri:", hhuri },
	{ "location:", hhlocation },
	{ "WWW-Authenticate:", hhauth },
};
int
httpheaders(int fd, int cfd, URL *u, Range *r)
{
	char buf[2048];
	char *p;
	int i, n;

	for(;;){
		n = getheader(fd, buf, sizeof(buf));
		if(n <= 0)
			break;
		if(cfd >= 0)
			fprint(cfd, "%s\n", buf);
		for(i = 0; i < nelem(headers); i++){
			n = strlen(headers[i].name);
			if(cistrncmp(buf, headers[i].name, n) == 0){
				/* skip field name and leading white */
				p = buf + n;
				while(*p == ' ' || *p == '\t')
					p++;

				(*headers[i].f)(p, u, r);
				break;
			}
		}
	}
	return n;
}

/*
 *  read a single mime header, collect continuations.
 *
 *  this routine assumes that there is a blank line twixt
 *  the header and the message body, otherwise bytes will
 *  be lost.
 */
int
getheader(int fd, char *buf, int n)
{
	char *p, *e;
	int i;

	n--;
	p = buf;
	for(e = p + n; ; p += i){
		i = readline(fd, p, e-p);
		if(i < 0)
			return i;

		if(p == buf){
			/* first line */
			if(strchr(buf, ':') == nil)
				break;		/* end of headers */
		} else {
			/* continuation line */
			if(*p != ' ' && *p != '\t'){
				unreadline(p);
				*p = 0;
				break;		/* end of this header */
			}
		}
	}
	if(headerprint)
		print("%s\n", buf);

	if(debug)
		fprint(2, "%d <- %s\n", fd, buf);
	return p-buf;
}

void
hhetag(char *p, URL *u, Range *r)
{
	USED(r);

	if(u->etag != nil){
		if(strcmp(u->etag, p) != 0)
			sysfatal("file changed underfoot");
	} else
		u->etag = strdup(p);
}

char*	monthchars = "janfebmaraprmayjunjulaugsepoctnovdec";

void
hhmtime(char *p, URL *u, Range *r)
{
	char *month, *day, *yr, *hms;
	char *fields[6];
	Tm tm, now;
	int i;

	USED(r);

	i = getfields(p, fields, 6, 1, " \t");
	if(i < 5)
		return;

	day = fields[1];
	month = fields[2];
	yr = fields[3];
	hms = fields[4];

	/* default time */
	now = *gmtime(time(0));
	tm = now;
	tm.yday = 0;

	/* convert ascii month to a number twixt 1 and 12 */
	if(*month >= '0' && *month <= '9'){
		tm.mon = atoi(month) - 1;
		if(tm.mon < 0 || tm.mon > 11)
			tm.mon = 5;
	} else {
		for(p = month; *p; p++)
			*p = tolower((uchar)*p);
		for(i = 0; i < 12; i++)
			if(strncmp(&monthchars[i*3], month, 3) == 0){
				tm.mon = i;
				break;
			}
	}

	tm.mday = atoi(day);

	if(hms) {
		tm.hour = strtoul(hms, &p, 10);
		if(*p == ':') {
			p++;
			tm.min = strtoul(p, &p, 10);
			if(*p == ':') {
				p++;
				tm.sec = strtoul(p, &p, 10);
			}
		}
		if(tolower((uchar)*p) == 'p')
			tm.hour += 12;
	}

	if(yr) {
		tm.year = atoi(yr);
		if(tm.year >= 1900)
			tm.year -= 1900;
	} else {
		if(tm.mon > now.mon || (tm.mon == now.mon && tm.mday > now.mday+1))
			tm.year--;
	}

	strcpy(tm.zone, "GMT");
	/* convert to epoch seconds */
	u->mtime = tm2sec(&tm);
}

void
hhclen(char *p, URL *u, Range *r)
{
	USED(u);

	r->end = atoi(p);
}

void
hhcrange(char *p, URL *u, Range *r)
{
	char *x;
	vlong l;

	USED(u);
	l = 0;
	x = strchr(p, '/');
	if(x)
		l = atoll(x+1);
	if(l == 0)
	x = strchr(p, '-');
	if(x)
		l = atoll(x+1);
	if(l)
		r->end = l;
}

void
hhuri(char *p, URL *u, Range *r)
{
	USED(r);

	if(*p != '<')
		return;
	u->redirect = strdup(p+1);
	p = strchr(u->redirect, '>');
	if(p != nil)
		*p = 0;
}

void
hhlocation(char *p, URL *u, Range *r)
{
	USED(r);

	u->redirect = strdup(p);
}

void
hhauth(char *p, URL *u, Range *r)
{
	char *f[4];
	UserPasswd *up;
	char *s, cred[64];

	USED(r);

	if (cistrncmp(p, "basic ", 6) != 0)
		sysfatal("only Basic authentication supported");

	if (gettokens(p, f, nelem(f), "\"") < 2)
		sysfatal("garbled auth data");

	if ((up = auth_getuserpasswd(auth_getkey, "proto=pass service=http dom=%q relm=%q",
	    	u->host, f[1])) == nil)
			sysfatal("cannot authenticate");

	s = smprint("%s:%s", up->user, up->passwd);
	if(enc64(cred, sizeof(cred), (uchar *)s, strlen(s)) == -1)
		sysfatal("enc64");
  		free(s);

	assert(u->cred = strdup(cred));
}

enum
{
	/* ftp return codes */
	Extra=		1,
	Success=	2,
	Incomplete=	3,
	TempFail=	4,
	PermFail=	5,

	Nnetdir=	64,	/* max length of network directory paths */
	Ndialstr=	64		/* max length of dial strings */
};

int ftpcmd(int, char*, ...);
int ftprcode(int, char*, int);
int hello(int);
int logon(int);
int xfertype(int, char*);
int passive(int, URL*);
int active(int, URL*);
int ftpxfer(int, Out*, Range*);
int terminateftp(int, int);
int getaddrport(char*, uchar*, uchar*);
int ftprestart(int, Out*, URL*, Range*, long);

int
doftp(URL *u, URL *px, Range *r, Out *out, long mtime)
{
	int pid, ctl, data, rv;
	Waitmsg *w;
	char msg[64];

	/* untested, proxy dosn't work with ftp (I think) */
	if(px->host == nil){
		ctl = dial(netmkaddr(u->host, tcpdir, u->port), 0, 0, 0);
	} else {
		ctl = dial(netmkaddr(px->host, tcpdir, px->port), 0, 0, 0);
	}

	if(ctl < 0)
		return Error;
	if(net == nil)
		strcpy(tcpdir, "tcp");

	initibuf();

	rv = hello(ctl);
	if(rv < 0)
		return terminateftp(ctl, rv);

	rv = logon(ctl);
	if(rv < 0)
		return terminateftp(ctl, rv);

	rv = xfertype(ctl, "I");
	if(rv < 0)
		return terminateftp(ctl, rv);

	/* if file is up to date and the right size, stop */
	if(ftprestart(ctl, out, u, r, mtime) > 0){
		close(ctl);
		return Eof;
	}

	/* first try passive mode, then active */
	data = passive(ctl, u);
	if(data < 0){
		data = active(ctl, u);
		if(data < 0)
			return Error;
	}

	/* fork */
	switch(pid = fork()){
	case -1:
		close(data);
		return terminateftp(ctl, Error);
	case 0:
		ftpxfer(data, out, r);
		close(data);
		#undef _exits
		_exits(0);
	default:
		close(data);
		break;
	}

	/* wait for reply message */
	rv = ftprcode(ctl, msg, sizeof(msg));
	close(ctl);

	/* wait for process to terminate */
	w = nil;
	for(;;){
		free(w);
		w = wait();
		if(w == nil)
			return Error;
		if(w->pid == pid){
			if(w->msg[0] == 0){
				free(w);
				break;
			}
			werrstr("xfer: %s", w->msg);
			free(w);
			return Error;
		}
	}

	switch(rv){
	case Success:
		return Eof;
	case TempFail:
		return Server;
	default:
		return Error;
	}
}

int
ftpcmd(int ctl, char *fmt, ...)
{
	va_list arg;
	char buf[2*1024], *s;

	va_start(arg, fmt);
	s = vseprint(buf, buf + (sizeof(buf)-4) / sizeof(*buf), fmt, arg);
	va_end(arg);
	if(debug)
		fprint(2, "%d -> %s\n", ctl, buf);
	*s++ = '\r';
	*s++ = '\n';
	if(write(ctl, buf, s - buf) != s - buf)
		return -1;
	return 0;
}

int
ftprcode(int ctl, char *msg, int len)
{
	int rv;
	int i;
	char *p;

	len--;	/* room for terminating null */
	for(;;){
		*msg = 0;
		i = readline(ctl, msg, len);
		if(i < 0)
			break;
		if(debug)
			fprint(2, "%d <- %s\n", ctl, msg);

		/* stop if not a continuation */
		rv = strtol(msg, &p, 10);
		if(rv >= 100 && rv < 600 && p==msg+3 && *p == ' ')
			return rv/100;
	}
	*msg = 0;

	return -1;
}

int
hello(int ctl)
{
	char msg[1024];

	/* wait for hello from other side */
	if(ftprcode(ctl, msg, sizeof(msg)) != Success){
		werrstr("HELLO: %s", msg);
		return Server;
	}
	return 0;
}

int
getdec(char *p, int n)
{
	int x = 0;
	int i;

	for(i = 0; i < n; i++)
		x = x*10 + (*p++ - '0');
	return x;
}

int
ftprestart(int ctl, Out *out, URL *u, Range *r, long mtime)
{
	Tm tm;
	char msg[1024];
	long x, rmtime;

	ftpcmd(ctl, "MDTM %s", u->page);
	if(ftprcode(ctl, msg, sizeof(msg)) != Success){
		r->start = 0;
		return 0;		/* need to do something */
	}

	/* decode modification time */
	if(strlen(msg) < 4 + 4 + 2 + 2 + 2 + 2 + 2){
		r->start = 0;
		return 0;		/* need to do something */
	}
	memset(&tm, 0, sizeof(tm));
	tm.year = getdec(msg+4, 4) - 1900;
	tm.mon = getdec(msg+4+4, 2) - 1;
	tm.mday = getdec(msg+4+4+2, 2);
	tm.hour = getdec(msg+4+4+2+2, 2);
	tm.min = getdec(msg+4+4+2+2+2, 2);
	tm.sec = getdec(msg+4+4+2+2+2+2, 2);
	strcpy(tm.zone, "GMT");
	rmtime = tm2sec(&tm);
	if(rmtime > mtime)
		r->start = 0;

	/* get size */
	ftpcmd(ctl, "SIZE %s", u->page);
	if(ftprcode(ctl, msg, sizeof(msg)) == Success){
		x = atol(msg+4);
		if(r->start == x)
			return 1;	/* we're up to date */
		r->end = x;
	}

	/* seek to restart point */
	if(r->start > 0){
		ftpcmd(ctl, "REST %lud", r->start);
		if(ftprcode(ctl, msg, sizeof(msg)) == Incomplete){
			setoffset(out, r->start);
		}else
			r->start = 0;
	}

	return 0;	/* need to do something */
}

int
logon(int ctl)
{
	char msg[1024];

	/* login anonymous */
	ftpcmd(ctl, "USER anonymous");
	switch(ftprcode(ctl, msg, sizeof(msg))){
	case Success:
		return 0;
	case Incomplete:
		break;	/* need password */
	default:
		werrstr("USER: %s", msg);
		return Server;
	}

	/* send user id as password */
	sprint(msg, "%s@closedmind.org", getuser());
	ftpcmd(ctl, "PASS %s", msg);
	if(ftprcode(ctl, msg, sizeof(msg)) != Success){
		werrstr("PASS: %s", msg);
		return Server;
	}

	return 0;
}

int
xfertype(int ctl, char *t)
{
	char msg[1024];

	ftpcmd(ctl, "TYPE %s", t);
	if(ftprcode(ctl, msg, sizeof(msg)) != Success){
		werrstr("TYPE %s: %s", t, msg);
		return Server;
	}

	return 0;
}

int
passive(int ctl, URL *u)
{
	char msg[1024];
	char ipaddr[32];
	char *f[6];
	char *p;
	int fd;
	int port;
	char aport[12];

	ftpcmd(ctl, "PASV");
	if(ftprcode(ctl, msg, sizeof(msg)) != Success)
		return Error;

	/* get address and port number from reply, this is AI */
	p = strchr(msg, '(');
	if(p == nil){
		for(p = msg+3; *p; p++)
			if(isdigit((uchar)*p))
				break;
	} else
		p++;
	if(getfields(p, f, 6, 0, ",)") < 6){
		werrstr("ftp protocol botch");
		return Server;
	}
	snprint(ipaddr, sizeof(ipaddr), "%s.%s.%s.%s",
		f[0], f[1], f[2], f[3]);
	port = ((atoi(f[4])&0xff)<<8) + (atoi(f[5])&0xff);
	sprint(aport, "%d", port);

	/* open data connection */
	fd = dial(netmkaddr(ipaddr, tcpdir, aport), 0, 0, 0);
	if(fd < 0){
		werrstr("passive mode failed: %r");
		return Error;
	}

	/* tell remote to send a file */
	ftpcmd(ctl, "RETR %s", u->page);
	if(ftprcode(ctl, msg, sizeof(msg)) != Extra){
		werrstr("RETR %s: %s", u->page, msg);
		return Error;
	}
	return fd;
}

int
active(int ctl, URL *u)
{
	char msg[1024];
	char dir[40], ldir[40];
	uchar ipaddr[4];
	uchar port[2];
	int lcfd, dfd, afd;

	/* announce a port for the call back */
	snprint(msg, sizeof(msg), "%s!*!0", tcpdir);
	afd = announce(msg, dir);
	if(afd < 0)
		return Error;

	/* get a local address/port of the annoucement */
	if(getaddrport(dir, ipaddr, port) < 0){
		close(afd);
		return Error;
	}

	/* tell remote side address and port*/
	ftpcmd(ctl, "PORT %d,%d,%d,%d,%d,%d", ipaddr[0], ipaddr[1], ipaddr[2],
		ipaddr[3], port[0], port[1]);
	if(ftprcode(ctl, msg, sizeof(msg)) != Success){
		close(afd);
		werrstr("active: %s", msg);
		return Error;
	}

	/* tell remote to send a file */
	ftpcmd(ctl, "RETR %s", u->page);
	if(ftprcode(ctl, msg, sizeof(msg)) != Extra){
		close(afd);
		werrstr("RETR: %s", msg);
		return Server;
	}

	/* wait for a connection */
	lcfd = listen(dir, ldir);
	if(lcfd < 0){
		close(afd);
		return Error;
	}
	dfd = accept(lcfd, ldir);
	if(dfd < 0){
		close(afd);
		close(lcfd);
		return Error;
	}
	close(afd);
	close(lcfd);

	return dfd;
}

int
ftpxfer(int in, Out *out, Range *r)
{
	char buf[1024];
	long vtime;
	int i, n;

	vtime = 0;
	for(n = 0;;n += i){
		i = read(in, buf, sizeof(buf));
		if(i == 0)
			break;
		if(i < 0)
			return Error;
		if(output(out, buf, i) != i)
			return Error;
		r->start += i;
		if(verbose && (vtime != time(0) || r->start == r->end)) {
			vtime = time(0);
			fprint(2, "%ld %ld\n", r->start, r->end);
		}
	}
	return n;
}

int
terminateftp(int ctl, int rv)
{
	close(ctl);
	return rv;
}

/*
 * case insensitive strcmp (why aren't these in libc?)
 */
int
cistrncmp(char *a, char *b, int n)
{
	while(n-- > 0){
		if(tolower((uchar)*a++) != tolower((uchar)*b++))
			return -1;
	}
	return 0;
}

int
cistrcmp(char *a, char *b)
{
	while(*a || *b)
		if(tolower((uchar)*a++) != tolower((uchar)*b++))
			return -1;

	return 0;
}

/*
 *  buffered io
 */
struct
{
	char *rp;
	char *wp;
	char buf[4*1024];
} b;

void
initibuf(void)
{
	b.rp = b.wp = b.buf;
}

/*
 *  read a possibly buffered line, strip off trailing while
 */
int
readline(int fd, char *buf, int len)
{
	int n;
	char *p;
	int eof = 0;

	len--;

	for(p = buf;;){
		if(b.rp >= b.wp){
			n = read(fd, b.wp, sizeof(b.buf)/2);
			if(n < 0)
				return -1;
			if(n == 0){
				eof = 1;
				break;
			}
			b.wp += n;
		}
		n = *b.rp++;
		if(len > 0){
			*p++ = n;
			len--;
		}
		if(n == '\n')
			break;
	}

	/* drop trailing white */
	for(;;){
		if(p <= buf)
			break;
		n = *(p-1);
		if(n != ' ' && n != '\t' && n != '\r' && n != '\n')
			break;
		p--;
	}
	*p = 0;

	if(eof && p == buf)
		return -1;

	return p-buf;
}

void
unreadline(char *line)
{
	int i, n;

	i = strlen(line);
	n = b.wp-b.rp;
	memmove(&b.buf[i+1], b.rp, n);
	memmove(b.buf, line, i);
	b.buf[i] = '\n';
	b.rp = b.buf;
	b.wp = b.rp + i + 1 + n;
}

int
readibuf(int fd, char *buf, int len)
{
	int n;

	n = b.wp-b.rp;
	if(n > 0){
		if(n > len)
			n = len;
		memmove(buf, b.rp, n);
		b.rp += n;
		return n;
	}
	return read(fd, buf, len);
}

int
dfprint(int fd, char *fmt, ...)
{
	char buf[4*1024];
	va_list arg;

	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	if(debug)
		fprint(2, "%d -> %s", fd, buf);
	return fprint(fd, "%s", buf);
}

int
getaddrport(char *dir, uchar *ipaddr, uchar *port)
{
	char buf[256];
	int fd, i;
	char *p;

	snprint(buf, sizeof(buf), "%s/local", dir);
	fd = open(buf, OREAD);
	if(fd < 0)
		return -1;
	i = read(fd, buf, sizeof(buf)-1);
	close(fd);
	if(i <= 0)
		return -1;
	buf[i] = 0;
	p = strchr(buf, '!');
	if(p != nil)
		*p++ = 0;
	v4parseip(ipaddr, buf);
	i = atoi(p);
	port[0] = i>>8;
	port[1] = i;
	return 0;
}

void
md5free(DigestState *state)
{
	uchar x[MD5dlen];
	md5(nil, 0, x, state);
}

DigestState*
md5dup(DigestState *state)
{
	DigestState *s2;

	s2 = malloc(sizeof(DigestState));
	if(s2 == nil)
		sysfatal("malloc: %r");
	*s2 = *state;
	s2->malloced = 1;
	return s2;
}

void
setoffset(Out *out, int offset)
{
	md5free(out->curr);
	if(offset == 0)
		out->curr = md5(nil, 0, nil, nil);
	else
		out->curr = nil;
	out->offset = offset;
}

/*
 * write some output, discarding it (but keeping track)
 * if we've already written it. if we've gone backwards,
 * verify that everything previously written matches
 * that which would have been written from the current
 * output.
 */
int
output(Out *out, char *buf, int nb)
{
	int n, d;
	uchar m0[MD5dlen], m1[MD5dlen];

	n = nb;
	d = out->written - out->offset;
	assert(d >= 0);
	if(d > 0){
		if(n < d){
			if(out->curr != nil)
				md5((uchar*)buf, n, nil, out->curr);
			out->offset += n;
			return n;
		}
		if(out->curr != nil){
			md5((uchar*)buf, d, m0, out->curr);
			out->curr = nil;
			md5(nil, 0, m1, md5dup(out->hiwat));
			if(memcmp(m0, m1, MD5dlen) != 0){
				fprint(2, "integrity check failure at offset %d\n", out->written);
				return -1;
			}
		}
		buf += d;
		n -= d;
		out->offset += d;
	}
	if(n > 0){
		out->hiwat = md5((uchar*)buf, n, nil, out->hiwat);
		n = write(out->fd, buf, n);
		if(n > 0){
			out->offset += n;
			out->written += n;
		}
	}
	return n + d;
}
