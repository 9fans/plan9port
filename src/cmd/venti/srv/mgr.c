/*
 * mirror manager.
 * a work in progress.
 * use at your own risk.
 */

#include "stdinc.h"
#include <regexp.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"

#ifdef PLAN9PORT
#define sp s.sp
#define ep e.ep
#endif

void sendmail(char *content, char *subject, char *msg);
#define TIME "[0-9]+/[0-9]+ [0-9]+:[0-9]+:[0-9]+"

char *mirrorregexp =
	"^" TIME " ("
		"([^ ]+ \\([0-9,]+-[0-9,]+\\))"
		"|(  copy [0-9,]+-[0-9,]+ (data|hole|directory|tail))"
		"|(  sha1 [0-9,]+-[0-9,]+)"
		"|([^ ]+: [0-9,]+ used mirrored)"
		"|([^ \\-]+-[^ \\-]+( mirrored| sealed| empty)+)"
	")$";
Reprog *mirrorprog;

char *verifyregexp =
	"^" TIME " ("
		"([^ ]+: unsealed [0-9,]+ bytes)"
	")$";
Reprog *verifyprog;

#undef pipe
enum
{
	LogSize = 4*1024*1024	// TODO: make smaller
};

VtLog *errlog;

typedef struct Mirror Mirror;
struct Mirror
{
	char *src;
	char *dst;
};

typedef struct Conf Conf;
struct Conf
{
	Mirror *mirror;
	int nmirror;
	char **verify;
	int nverify;
	char *httpaddr;
	char *webroot;
	char *smtp;
	char *mailfrom;
	char *mailto;
	int mirrorfreq;
	int verifyfreq;
};

typedef struct Job Job;
struct Job
{
	char *name;
	QLock lk;
	char *argv[10];
	int oldok;
	int newok;
	VtLog *oldlog;
	VtLog *newlog;
	int pid;
	int pipe;
	int nrun;
	vlong freq;
	vlong runstart;
	vlong runend;
	double offset;
	int (*ok)(char*);
};

Job *job;
int njob;
char *bin;

vlong time0;
Conf conf;

void
usage(void)
{
	fprint(2, "usage: mgr [-s] [-b bin/venti/] venti.conf\n");
	threadexitsall(0);
}

int
rdconf(char *file, Conf *conf)
{
	char *s, *line, *flds[10];
	int i, ok;
	IFile f;

	if(readifile(&f, file) < 0)
		return -1;
	memset(conf, 0, sizeof *conf);
	ok = -1;
	line = nil;
	for(;;){
		s = ifileline(&f);
		if(s == nil){
			ok = 0;
			break;
		}
		line = estrdup(s);
		i = getfields(s, flds, nelem(flds), 1, " \t\r");
		if(i <= 0 || strcmp(flds[0], "mgr") != 0) {
			/* do nothing */
		}else if(i == 4 && strcmp(flds[1], "mirror") == 0) {
			if(conf->nmirror%64 == 0)
				conf->mirror = vtrealloc(conf->mirror, (conf->nmirror+64)*sizeof(conf->mirror[0]));
			conf->mirror[conf->nmirror].src = vtstrdup(flds[2]);
			conf->mirror[conf->nmirror].dst = vtstrdup(flds[3]);
			conf->nmirror++;
		}else if(i == 3 && strcmp(flds[1], "mirrorfreq") == 0) {
			conf->mirrorfreq = atoi(flds[2]);
		}else if(i == 3 && strcmp(flds[1], "verify") == 0) {
			if(conf->nverify%64 == 0)
				conf->verify = vtrealloc(conf->verify, (conf->nverify+64)*sizeof(conf->verify[0]));
			conf->verify[conf->nverify++] = vtstrdup(flds[2]);
		}else if(i == 3 && strcmp(flds[1], "verifyfreq") == 0) {
			conf->verifyfreq = atoi(flds[2]);
		}else if(i == 3 && strcmp(flds[1], "httpaddr") == 0){
			if(conf->httpaddr){
				seterr(EAdmin, "duplicate httpaddr lines in configuration file %s", file);
				break;
			}
			conf->httpaddr = estrdup(flds[2]);
		}else if(i == 3 && strcmp(flds[1], "webroot") == 0){
			if(conf->webroot){
				seterr(EAdmin, "duplicate webroot lines in configuration file %s", file);
				break;
			}
			conf->webroot = estrdup(flds[2]);
		}else if(i == 3 && strcmp(flds[1], "smtp") == 0) {
			if(conf->smtp){
				seterr(EAdmin, "duplicate smtp lines in configuration file %s", file);
				break;
			}
			conf->smtp = estrdup(flds[2]);
		}else if(i == 3 && strcmp(flds[1], "mailfrom") == 0) {
			if(conf->mailfrom){
				seterr(EAdmin, "duplicate mailfrom lines in configuration file %s", file);
				break;
			}
			conf->mailfrom = estrdup(flds[2]);
		}else if(i == 3 && strcmp(flds[1], "mailto") == 0) {
			if(conf->mailto){
				seterr(EAdmin, "duplicate mailto lines in configuration file %s", file);
				break;
			}
			conf->mailto = estrdup(flds[2]);
		}else{
			seterr(EAdmin, "illegal line '%s' in configuration file %s", line, file);
			break;
		}
		free(line);
		line = nil;
	}
	free(line);
	freeifile(&f);
	return ok;
}

static QLock loglk;
static char *logbuf;

char*
logtext(VtLog *l)
{
	int i;
	char *p;
	VtLogChunk *c;

	p = logbuf;
	c = l->w;
	for(i=0; i<l->nchunk; i++) {
		if(++c == l->chunk+l->nchunk)
			c = l->chunk;
		memmove(p, c->p, c->wp - c->p);
		p += c->wp - c->p;
	}
	*p = 0;
	return logbuf;
}


typedef struct HttpObj	HttpObj;

static int fromwebdir(HConnect*);

enum
{
	ObjNameSize	= 64,
	MaxObjs		= 64
};

struct HttpObj
{
	char	name[ObjNameSize];
	int	(*f)(HConnect*);
};

static HttpObj	objs[MaxObjs];
static void httpproc(void*);

static HConnect*
mkconnect(void)
{
	HConnect *c;

	c = mallocz(sizeof(HConnect), 1);
	if(c == nil)
		sysfatal("out of memory");
	c->replog = nil;
	c->hpos = c->header;
	c->hstop = c->header;
	return c;
}

static int
preq(HConnect *c)
{
	if(hparseheaders(c, 0) < 0)
		return -1;
	if(strcmp(c->req.meth, "GET") != 0
	&& strcmp(c->req.meth, "HEAD") != 0)
		return hunallowed(c, "GET, HEAD");
	if(c->head.expectother || c->head.expectcont)
		return hfail(c, HExpectFail, nil);
	return 0;
}

int
hsettype(HConnect *c, char *type)
{
	Hio *hout;
	int r;

	r = preq(c);
	if(r < 0)
		return r;

	hout = &c->hout;
	if(c->req.vermaj){
		hokheaders(c);
		hprint(hout, "Content-type: %s\r\n", type);
		if(http11(c))
			hprint(hout, "Transfer-Encoding: chunked\r\n");
		hprint(hout, "\r\n");
	}

	if(http11(c))
		hxferenc(hout, 1);
	else
		c->head.closeit = 1;
	return 0;
}

int
hsethtml(HConnect *c)
{
	return hsettype(c, "text/html; charset=utf-8");
}

int
hsettext(HConnect *c)
{
	return hsettype(c, "text/plain; charset=utf-8");
}

int
hnotfound(HConnect *c)
{
	int r;

	r = preq(c);
	if(r < 0)
		return r;
	return hfail(c, HNotFound, c->req.uri);
}

static int
xloglist(HConnect *c)
{
	if(hsettype(c, "text/html") < 0)
		return -1;
	vtloghlist(&c->hout);
	hflush(&c->hout);
	return 0;
}

static int
strpcmp(const void *va, const void *vb)
{
	return strcmp(*(char**)va, *(char**)vb);
}

void
vtloghlist(Hio *h)
{
	char **p;
	int i, n;

	hprint(h, "<html><head>\n");
	hprint(h, "<title>Venti Server Logs</title>\n");
	hprint(h, "</head><body>\n");
	hprint(h, "<b>Venti Server Logs</b>\n<p>\n");

	p = vtlognames(&n);
	qsort(p, n, sizeof(p[0]), strpcmp);
	for(i=0; i<n; i++)
		hprint(h, "<a href=\"/log?log=%s\">%s</a><br>\n", p[i], p[i]);
	vtfree(p);
	hprint(h, "</body></html>\n");
}

void
vtloghdump(Hio *h, VtLog *l)
{
	int i;
	VtLogChunk *c;
	char *name;

	name = l ? l->name : "&lt;nil&gt;";

	hprint(h, "<html><head>\n");
	hprint(h, "<title>Venti Server Log: %s</title>\n", name);
	hprint(h, "</head><body>\n");
	hprint(h, "<b>Venti Server Log: %s</b>\n<p>\n", name);

	if(l){
		c = l->w;
		for(i=0; i<l->nchunk; i++){
			if(++c == l->chunk+l->nchunk)
				c = l->chunk;
			hwrite(h, c->p, c->wp-c->p);
		}
	}
	hprint(h, "</body></html>\n");
}


char*
hargstr(HConnect *c, char *name, char *def)
{
	HSPairs *p;

	for(p=c->req.searchpairs; p; p=p->next)
		if(strcmp(p->s, name) == 0)
			return p->t;
	return def;
}

static int
xlog(HConnect *c)
{
	char *name;
	VtLog *l;

	name = hargstr(c, "log", "");
	if(!name[0])
		return xloglist(c);
	l = vtlogopen(name, 0);
	if(l == nil)
		return hnotfound(c);
	if(hsettype(c, "text/html") < 0){
		vtlogclose(l);
		return -1;
	}
	vtloghdump(&c->hout, l);
	vtlogclose(l);
	hflush(&c->hout);
	return 0;
}

static void
httpdproc(void *vaddress)
{
	HConnect *c;
	char *address, ndir[NETPATHLEN], dir[NETPATHLEN];
	int ctl, nctl, data;

	address = vaddress;
	ctl = announce(address, dir);
	if(ctl < 0){
		sysfatal("announce %s: %r", address);
		return;
	}

	if(0) print("announce ctl %d dir %s\n", ctl, dir);
	for(;;){
		/*
		 *  wait for a call (or an error)
		 */
		nctl = listen(dir, ndir);
		if(0) print("httpd listen %d %s...\n", nctl, ndir);
		if(nctl < 0){
			fprint(2, "mgr: httpd can't listen on %s: %r\n", address);
			return;
		}

		data = accept(ctl, ndir);
		if(0) print("httpd accept %d...\n", data);
		if(data < 0){
			fprint(2, "mgr: httpd accept: %r\n");
			close(nctl);
			continue;
		}
		if(0) print("httpd close nctl %d\n", nctl);
		close(nctl);
		c = mkconnect();
		hinit(&c->hin, data, Hread);
		hinit(&c->hout, data, Hwrite);
		vtproc(httpproc, c);
	}
}

static void
httpproc(void *v)
{
	HConnect *c;
	int ok, i, n;

	c = v;

	for(;;){
		/*
		 * No timeout because the signal appears to hit every
		 * proc, not just us.
		 */
		if(hparsereq(c, 0) < 0)
			break;

		for(i = 0; i < MaxObjs && objs[i].name[0]; i++){
			n = strlen(objs[i].name);
			if((objs[i].name[n-1] == '/' && strncmp(c->req.uri, objs[i].name, n) == 0)
			|| (objs[i].name[n-1] != '/' && strcmp(c->req.uri, objs[i].name) == 0)){
				ok = (*objs[i].f)(c);
				goto found;
			}
		}
		ok = fromwebdir(c);
	found:
		hflush(&c->hout);
		if(c->head.closeit)
			ok = -1;
		hreqcleanup(c);

		if(ok < 0)
			break;
	}
	hreqcleanup(c);
	close(c->hin.fd);
	free(c);
}

static int
httpdobj(char *name, int (*f)(HConnect*))
{
	int i;

	if(name == nil || strlen(name) >= ObjNameSize)
		return -1;
	for(i = 0; i < MaxObjs; i++){
		if(objs[i].name[0] == '\0'){
			strcpy(objs[i].name, name);
			objs[i].f = f;
			return 0;
		}
		if(strcmp(objs[i].name, name) == 0)
			return -1;
	}
	return -1;
}


struct {
	char *ext;
	char *type;
} exttab[] = {
	".html",	"text/html",
	".txt",	"text/plain",
	".xml",	"text/xml",
	".png",	"image/png",
	".gif",	"image/gif",
	0
};

static int
fromwebdir(HConnect *c)
{
	char buf[4096], *p, *ext, *type;
	int i, fd, n, defaulted;
	Dir *d;

	if(conf.webroot == nil || strstr(c->req.uri, ".."))
		return hnotfound(c);
	snprint(buf, sizeof buf-20, "%s/%s", conf.webroot, c->req.uri+1);
	defaulted = 0;
reopen:
	if((fd = open(buf, OREAD)) < 0)
		return hnotfound(c);
	d = dirfstat(fd);
	if(d == nil){
		close(fd);
		return hnotfound(c);
	}
	if(d->mode&DMDIR){
		if(!defaulted){
			defaulted = 1;
			strcat(buf, "/index.html");
			free(d);
			close(fd);
			goto reopen;
		}
		free(d);
		return hnotfound(c);
	}
	free(d);
	p = buf+strlen(buf);
	type = "application/octet-stream";
	for(i=0; exttab[i].ext; i++){
		ext = exttab[i].ext;
		if(p-strlen(ext) >= buf && strcmp(p-strlen(ext), ext) == 0){
			type = exttab[i].type;
			break;
		}
	}
	if(hsettype(c, type) < 0){
		close(fd);
		return 0;
	}
	while((n = read(fd, buf, sizeof buf)) > 0)
		if(hwrite(&c->hout, buf, n) < 0)
			break;
	close(fd);
	hflush(&c->hout);
	return 0;
}

static int
hmanager(HConnect *c)
{
	Hio *hout;
	int r;
	int i, k;
	Job *j;
	VtLog *l;
	VtLogChunk *ch;

	r = hsethtml(c);
	if(r < 0)
		return r;

	hout = &c->hout;
	hprint(hout, "<html><head><title>venti mgr status</title></head>\n");
	hprint(hout, "<body><h2>venti mgr status</h2>\n");

	for(i=0; i<njob; i++) {
		j = &job[i];
		hprint(hout, "<b>");
		if(j->nrun == 0)
			hprint(hout, "----/--/-- --:--:--");
		else
			hprint(hout, "%+T", (long)(j->runstart + time0));
		hprint(hout, " %s", j->name);
		if(j->nrun > 0) {
			if(j->newok == -1) {
				hprint(hout, " (running)");
			} else if(!j->newok) {
				hprint(hout, " <font color=\"#cc0000\">(FAILED)</font>");
			}
		}
		hprint(hout, "</b>\n");
		hprint(hout, "<font size=-1><pre>\n");
		l = j->newlog;
		ch = l->w;
		for(k=0; k<l->nchunk; k++){
			if(++ch == l->chunk+l->nchunk)
				ch = l->chunk;
			hwrite(hout, ch->p, ch->wp-ch->p);
		}
		hprint(hout, "</pre></font>\n");
		hprint(hout, "\n");
	}
	hprint(hout, "</body></html>\n");
	hflush(hout);
	return 0;
}

void
piper(void *v)
{
	Job *j;
	char buf[512];
	VtLog *l;
	int n;
	int fd;
	char *p;
	int ok;

	j = v;
	fd = j->pipe;
	l = j->newlog;
	while((n = read(fd, buf, 512-1)) > 0) {
		buf[n] = 0;
		if(l != nil)
			vtlogprint(l, "%s", buf);
	}
	qlock(&loglk);
	p = logtext(l);
	ok = j->ok(p);
	qunlock(&loglk);
	j->newok = ok;
	close(fd);
}

void
kickjob(Job *j)
{
	int i;
	int fd[3];
	int p[2];
	VtLog *l;

	if((fd[0] = open("/dev/null", ORDWR)) < 0) {
		vtlogprint(errlog, "%T open /dev/null: %r\n");
		return;
	}
	if(pipe(p) < 0) {
		vtlogprint(errlog, "%T pipe: %r\n");
		close(fd[0]);
		return;
	}
	qlock(&j->lk);
	l = j->oldlog;
	j->oldlog = j->newlog;
	j->newlog = l;
	qlock(&l->lk);
	for(i=0; i<l->nchunk; i++)
		l->chunk[i].wp = l->chunk[i].p;
	qunlock(&l->lk);
	j->oldok = j->newok;
	j->newok = -1;
	qunlock(&j->lk);

	fd[1] = p[1];
	fd[2] = p[1];
	j->pid = threadspawn(fd, j->argv[0], j->argv);
	if(j->pid < 0) {
		vtlogprint(errlog, "%T exec %s: %r\n", j->argv[0]);
		close(fd[0]);
		close(fd[1]);
		close(p[0]);
	}
	// fd[0], fd[1], fd[2] are closed now
	j->pipe = p[0];
	j->nrun++;
	vtproc(piper, j);
}

int
getline(Resub *text, Resub *line)
{
	char *p;

	if(text->sp >= text->ep)
		return -1;
	line->sp = text->sp;
	p = memchr(text->sp, '\n', text->ep - text->sp);
	if(p == nil) {
		line->ep = text->ep;
		text->sp = text->ep;
	} else {
		line->ep = p;
		text->sp = p+1;
	}
	return 0;
}

int
verifyok(char *output)
{
	Resub text, line, m;

	text.sp = output;
	text.ep = output+strlen(output);
	while(getline(&text, &line) >= 0) {
		*line.ep = 0;
		memset(&m, 0, sizeof m);
		if(!regexec(verifyprog, line.sp, nil, 0))
			return 0;
		*line.ep = '\n';
	}
	return 1;
}

int
mirrorok(char *output)
{
	Resub text, line, m;

	text.sp = output;
	text.ep = output+strlen(output);
	while(getline(&text, &line) >= 0) {
		*line.ep = 0;
		memset(&m, 0, sizeof m);
		if(!regexec(mirrorprog, line.sp, nil, 0))
			return 0;
		*line.ep = '\n';
	}
	return 1;
}

void
mkjob(Job *j, ...)
{
	int i;
	char *p;
	va_list arg;

	memset(j, 0, sizeof *j);
	i = 0;
	va_start(arg, j);
	while((p = va_arg(arg, char*)) != nil) {
		j->argv[i++] = p;
		if(i >= nelem(j->argv))
			sysfatal("job argv size too small");
	}
	j->argv[i] = nil;
	j->oldlog = vtlogopen(smprint("log%ld.0", j-job), LogSize);
	j->newlog = vtlogopen(smprint("log%ld.1", j-job), LogSize);
	va_end(arg);
}

void
manager(void *v)
{
	int i;
	Job *j;
	vlong now;

	USED(v);
	for(;; sleep(1000)) {
		for(i=0; i<njob; i++) {
			now = time(0) - time0;
			j = &job[i];
			if(j->pid > 0 || j->newok == -1) {
				// still running
				if(now - j->runstart > 2*j->freq) {
					//TODO: log slow running j
				}
				continue;
			}
			if((j->nrun > 0 && now - j->runend > j->freq)
			|| (j->nrun == 0 && now > (vlong)(j->offset*j->freq))) {
				j->runstart = now;
				j->runend = 0;
				kickjob(j);
			}
		}
	}
}

void
waitproc(void *v)
{
	Channel *c;
	Waitmsg *w;
	int i;
	Job *j;

	c = v;
	for(;;) {
		w = recvp(c);
		for(i=0; i<njob; i++) {
			j = &job[i];
			if(j->pid == w->pid) {
				j->pid = 0;
				j->runend = time(0) - time0;
				break;
			}
		}
		free(w);
	}
}

void
threadmain(int argc, char **argv)
{
	int i;
	int nofork;
	char *prog;
	Job *j;

	ventilogging = 1;
	ventifmtinstall();
#ifdef PLAN9PORT
	bin = unsharp("#9/bin/venti");
#else
	bin = "/bin/venti";
#endif
	nofork = 0;
	ARGBEGIN{
	case 'b':
		bin = EARGF(usage());
		break;
	case 's':
		nofork = 1;
		break;
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();
	if(rdconf(argv[0], &conf) < 0)
		sysfatal("reading config: %r");
	if(conf.httpaddr == nil)
		sysfatal("config has no httpaddr");
	if(conf.smtp != nil && conf.mailfrom == nil)
		sysfatal("config has smtp but no mailfrom");
	if(conf.smtp != nil && conf.mailto == nil)
		sysfatal("config has smtp but no mailto");
	if((mirrorprog = regcomp(mirrorregexp)) == nil)
		sysfatal("mirrorregexp did not complete");
	if((verifyprog = regcomp(verifyregexp)) == nil)
		sysfatal("verifyregexp did not complete");
	if(conf.nverify > 0 && conf.verifyfreq == 0)
		sysfatal("config has no verifyfreq");
	if(conf.nmirror > 0 && conf.mirrorfreq == 0)
		sysfatal("config has no mirrorfreq");

	time0 = time(0);
//	sendmail("startup", "mgr is starting\n");

	logbuf = vtmalloc(LogSize+1);	// +1 for NUL

	errlog = vtlogopen("errors", LogSize);
	job = vtmalloc((conf.nmirror+conf.nverify)*sizeof job[0]);
	prog = smprint("%s/mirrorarenas", bin);
	for(i=0; i<conf.nmirror; i++) {
		// job: /bin/venti/mirrorarenas -v src dst
		// filter output
		j = &job[njob++];
		mkjob(j, prog, "-v", conf.mirror[i].src, conf.mirror[i].dst, nil);
		j->name = smprint("mirror %s %s", conf.mirror[i].src, conf.mirror[i].dst);
		j->ok = mirrorok;
		j->freq = conf.mirrorfreq;	// 4 hours	// TODO: put in config
		j->offset = (double)i/conf.nmirror;
	}

	prog = smprint("%s/verifyarena", bin);
	for(i=0; i<conf.nverify; i++) {
		// job: /bin/venti/verifyarena -b 64M -s 1000 -v arena
		// filter output
		j = &job[njob++];
		mkjob(j, prog, "-b64M", "-s1000", conf.verify[i], nil);
		j->name = smprint("verify %s", conf.verify[i]);
		j->ok = verifyok;
		j->freq = conf.verifyfreq;
		j->offset = (double)i/conf.nverify;
	}

	httpdobj("/mgr", hmanager);
	httpdobj("/log", xlog);
	vtproc(httpdproc, conf.httpaddr);
	vtproc(waitproc, threadwaitchan());
	if(nofork)
		manager(nil);
	else
		vtproc(manager, nil);
}


void
qp(Biobuf *b, char *p)
{
	int n, nspace;

	nspace = 0;
	n = 0;
	for(; *p; p++) {
		if(*p == '\n') {
			if(nspace > 0) {
				nspace = 0;
				Bprint(b, "=\n");
			}
			Bputc(b, '\n');
			n = 0;
			continue;
		}
		if(n > 70) {
			Bprint(b, "=\n");
			nspace = 0;
			continue;
		}
		if(33 <= *p && *p <= 126 && *p != '=') {
			Bputc(b, *p);
			n++;
			nspace = 0;
			continue;
		}
		if(*p == ' ' || *p == '\t') {
			Bputc(b, *p);
			n++;
			nspace++;
			continue;
		}
		Bprint(b, "=%02X", (uchar)*p);
		n += 3;
		nspace = 0;
	}
}

int
smtpread(Biobuf *b, int code)
{
	char *p, *q;
	int n;

	while((p = Brdstr(b, '\n', 1)) != nil) {
		n = strtol(p, &q, 10);
		if(n == 0 || q != p+3) {
		error:
			vtlogprint(errlog, "sending mail: %s\n", p);
			free(p);
			return -1;
		}
		if(*q == ' ') {
			if(n == code) {
				free(p);
				return 0;
			}
			goto error;
		}
		if(*q != '-') {
			goto error;
		}
	}
	return -1;
}


void
sendmail(char *content, char *subject, char *msg)
{
	int fd;
	Biobuf *bin, *bout;

	if((fd = dial(conf.smtp, 0, 0, 0)) < 0) {
		vtlogprint(errlog, "dial %s: %r\n", conf.smtp);
		return;
	}
	bin = vtmalloc(sizeof *bin);
	bout = vtmalloc(sizeof *bout);
	Binit(bin, fd, OREAD);
	Binit(bout, fd, OWRITE);
	if(smtpread(bin, 220) < 0){
	error:
		close(fd);
		Bterm(bin);
		Bterm(bout);
		return;
	}

	Bprint(bout, "HELO venti-mgr\n");
	Bflush(bout);
	if(smtpread(bin, 250) < 0)
		goto error;

	Bprint(bout, "MAIL FROM:<%s>\n", conf.mailfrom);
	Bflush(bout);
	if(smtpread(bin, 250) < 0)
		goto error;

	Bprint(bout, "RCPT TO:<%s>\n", conf.mailfrom);
	Bflush(bout);
	if(smtpread(bin, 250) < 0)
		goto error;

	Bprint(bout, "DATA\n");
	Bflush(bout);
	if(smtpread(bin, 354) < 0)
		goto error;

	Bprint(bout, "From: \"venti mgr\" <%s>\n", conf.mailfrom);
	Bprint(bout, "To: <%s>\n", conf.mailto);
	Bprint(bout, "Subject: %s\n", subject);
	Bprint(bout, "MIME-Version: 1.0\n");
	Bprint(bout, "Content-Type: %s; charset=\"UTF-8\"\n", content);
	Bprint(bout, "Content-Transfer-Encoding: quoted-printable\n");
	Bprint(bout, "Message-ID: %08lux%08lux@venti.swtch.com\n", fastrand(), fastrand());
	Bprint(bout, "\n");
	qp(bout, msg);
	Bprint(bout, ".\n");
	Bflush(bout);
	if(smtpread(bin, 250) < 0)
		goto error;

	Bprint(bout, "QUIT\n");
	Bflush(bout);
	Bterm(bin);
	Bterm(bout);
	close(fd);
}
