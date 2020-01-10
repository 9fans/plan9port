/*
 * Remote file system editing client.
 * Only talks to acme - external programs do all the hard work.
 *
 * If you add a plumbing rule:

# /n/ paths go to simulator in acme
kind is text
data matches '[a-zA-Z0-9_\-./]+('$addr')?'
data matches '(/n/[a-zA-Z0-9_\-./]+)('$addr')?'
plumb to netfileedit
plumb client Netfiles

 * then plumbed paths starting with /n/ will find their way here.
 *
 * Perhaps on startup should look for windows named /n/ and attach to them?
 * Or might that be too aggressive?
 */

#include <u.h>
#include <libc.h>
#include <thread.h>
#include <9pclient.h>
#include <plumb.h>
#include "acme.h"

char *root = "/n/";

void
usage(void)
{
	fprint(2, "usage: Netfiles\n");
	threadexitsall("usage");
}

extern int chatty9pclient;
int debug;
#define dprint if(debug>1)print
#define cprint if(debug)print
Win *mkwin(char*);
int do3(Win *w, char *arg);

enum {
	STACK = 128*1024
};

enum {
	Put,
	Get,
	Del,
	Delete,
	Debug,
	XXX
};

char *cmds[] = {
	"Put",
	"Get",
	"Del",
	"Delete",
	"Debug",
	nil
};

char *debugstr[] = {
	"off",
	"minimal",
	"chatty"
};

typedef struct Arg Arg;
struct Arg
{
	char *file;
	char *addr;
	Channel *c;
};

Arg*
arg(char *file, char *addr, Channel *c)
{
	Arg *a;

	a = emalloc(sizeof *a);
	a->file = estrdup(file);
	a->addr = estrdup(addr);
	a->c = c;
	return a;
}

Win*
winbyid(int id)
{
	Win *w;

	for(w=windows; w; w=w->next)
		if(w->id == id)
			return w;
	return nil;
}

/*
 * return Win* of a window named name or name/
 * assumes name is cleaned.
 */
Win*
nametowin(char *name)
{
	char *index, *p, *next;
	int len, n;
	Win *w;

	index = winindex();
	len = strlen(name);
	for(p=index; p && *p; p=next){
		if((next = strchr(p, '\n')) != nil)
			*next++ = 0;
		if(strlen(p) <= 5*12)
			continue;
		if(memcmp(p+5*12, name, len)!=0)
			continue;
		if(p[5*12+len]!=' ' && (p[5*12+len]!='/' || p[5*12+len+1]!=' '))
			continue;
		n = atoi(p);
		if((w = winbyid(n)) != nil){
			free(index);
			return w;
		}
	}
	free(index);
	return nil;
}


/*
 * look for s in list
 */
int
lookup(char *s, char **list)
{
	int i;

	for(i=0; list[i]; i++)
		if(strcmp(list[i], s) == 0)
			return i;
	return -1;
}

/*
 * move to top of file
 */
void
wintop(Win *w)
{
	winaddr(w, "#0");
	winctl(w, "dot=addr");
	winctl(w, "show");
}

int
isdot(Win *w, uint xq0, uint xq1)
{
	uint q0, q1;

	winctl(w, "addr=dot");
	q0 = winreadaddr(w, &q1);
	return xq0==q0 && xq1==q1;
}

/*
 * Expand the click further than acme usually does -- all non-white space is okay.
 */
char*
expandarg(Win *w, Event *e)
{
	uint q0, q1;

	if(e->c2 == 'l')	/* in tag - no choice but to accept acme's expansion */
		return estrdup(e->text);
	winaddr(w, ",");
	winctl(w, "addr=dot");

	q0 = winreadaddr(w, &q1);
	cprint("acme expanded %d-%d into %d-%d (dot %d-%d)\n",
		e->oq0, e->oq1, e->q0, e->q1, q0, q1);

	if(e->oq0 == e->oq1 && e->q0 != e->q1 && !isdot(w, e->q0, e->q1)){
		winaddr(w, "#%ud+#1-/[^ \t\\n]*/,#%ud-#1+/[^ \t\\n]*/", e->q0, e->q1);
		q0 = winreadaddr(w, &q1);
		cprint("\tre-expand to %d-%d\n", q0, q1);
	}else
		winaddr(w, "#%ud,#%ud", e->q0, e->q1);
	return winmread(w, "xdata");
}

/*
 * handle a plumbing message
 */
void
doplumb(void *vm)
{
	char *addr;
	Plumbmsg *m;
	Win *w;

	m = vm;
	if(m->ndata >= 1024){
		fprint(2, "insanely long file name (%d bytes) in plumb message (%.32s...)\n",
			m->ndata, m->data);
		plumbfree(m);
		return;
	}

	addr = plumblookup(m->attr, "addr");
	w = nametowin(m->data);
	if(w == nil)
		w = mkwin(m->data);
	winaddr(w, "%s", addr);
	winctl(w, "dot=addr");
	winctl(w, "show");
/*	windecref(w); */
	plumbfree(m);
}

/*
 * dispatch messages from the plumber
 */
void
plumbthread(void *v)
{
	CFid *fid;
	Plumbmsg *m;

	threadsetname("plumbthread");
	fid = plumbopenfid("netfileedit", OREAD);
	if(fid == nil){
		fprint(2, "cannot open plumb/netfileedit: %r\n");
		return;
	}
	while((m = plumbrecvfid(fid)) != nil)
		threadcreate(doplumb, m, STACK);
	fsclose(fid);
}

/*
 * parse /n/system/path
 */
int
parsename(char *name, char **server, char **path)
{
	char *p, *nul;

	cleanname(name);
	if(strncmp(name, "/n/", 3) != 0 && name[3] == 0)
		return -1;
	nul = nil;
	if((p = strchr(name+3, '/')) == nil)
		*path = estrdup("/");
	else{
		*path = estrdup(p);
		*p = 0;
		nul = p;
	}
	p = name+3;
	if(p[0] == 0){
		free(*path);
		*server = *path = nil;
		if(nul)
			*nul = '/';
		return -1;
	}
	*server = estrdup(p);
	if(nul)
		*nul = '/';
	return 0;
}

/*
 * shell out to find the type of a given file
 */
char*
filestat(char *server, char *path)
{
	char *type;
	static struct {
		char *server;
		char *path;
		char *type;
	} cache;

	if(cache.server && strcmp(server, cache.server) == 0)
	if(cache.path && strcmp(path, cache.path) == 0){
		type = estrdup(cache.type);
		cprint("9 netfilestat %q %q => %s (cached)\n", server, path, type);
		return type;
	}

	type = sysrun(2, "9 netfilestat %q %q", server, path);

	free(cache.server);
	free(cache.path);
	free(cache.type);
	cache.server = estrdup(server);
	cache.path = estrdup(path);
	cache.type = estrdup(type);

	cprint("9 netfilestat %q %q => %s\n", server, path, type);
	return type;
}

/*
 * manage a single window
 */
void
filethread(void *v)
{
	char *arg, *name, *p, *server, *path, *type;
	Arg *a;
	Channel *c;
	Event *e;
	Win *w;

	a = v;
	threadsetname("file %s", a->file);
	w = newwin();
	winname(w, a->file);
	winprint(w, "tag", "Get Put Look ");
	c = wineventchan(w);

	goto caseGet;

	while((e=recvp(c)) != nil){
		if(e->c1!='K')
			dprint("acme %E\n", e);
		if(e->c1=='M')
		switch(e->c2){
		case 'x':
		case 'X':
			switch(lookup(e->text, cmds)){
			caseGet:
			case Get:
				server = nil;
				path = nil;
				if(parsename(name=wingetname(w), &server, &path) < 0){
					fprint(2, "Netfiles: bad name %s\n", name);
					goto out;
				}
				type = filestat(server, path);
				if(type == nil)
					type = estrdup("");
				if(strcmp(type, "file")==0 || strcmp(type, "directory")==0){
					winaddr(w, ",");
					winprint(w, "data", "[reading...]");
					winaddr(w, ",");
					cprint("9 netfileget %s%q %q\n",
						strcmp(type, "file") == 0 ? "" : "-d", server, path);
					if(strcmp(type, "file")==0)
						twait(pipetowin(w, "data", 2, "9 netfileget %q %q", server, path));
					else
						twait(pipetowin(w, "data", 2, "9 netfileget -d %q %q | winid=%d mc", server, path, w->id));
					cleanname(name);
					if(strcmp(type, "directory")==0){
						p = name+strlen(name);
						if(p[-1] != '/'){
							p[0] = '/';
							p[1] = 0;
						}
					}
					winname(w, name);
					wintop(w);
					winctl(w, "clean");
					if(a && a->addr){
						winaddr(w, "%s", a->addr);
						winctl(w, "dot=addr");
						winctl(w, "show");
					}
				}
				free(type);
			out:
				free(server);
				free(path);
				if(a){
					if(a->c){
						sendp(a->c, w);
						a->c = nil;
					}
					free(a->file);
					free(a->addr);
					free(a);
					a = nil;
				}
				break;
			case Put:
				server = nil;
				path = nil;
				if(parsename(name=wingetname(w), &server, &path) < 0){
					fprint(2, "Netfiles: bad name %s\n", name);
					goto out;
				}
				cprint("9 netfileput %q %q\n", server, path);
				if(twait(pipewinto(w, "body", 2, "9 netfileput %q %q", server, path)) >= 0){
					cleanname(name);
					winname(w, name);
					winctl(w, "clean");
				}
				free(server);
				free(path);
				break;
			case Del:
				winctl(w, "del");
				break;
			case Delete:
				winctl(w, "delete");
				break;
			case Debug:
				debug = (debug+1)%3;
				print("Netfiles debug %s\n", debugstr[debug]);
				break;
			default:
				winwriteevent(w, e);
				break;
			}
			break;
		case 'l':
		case 'L':
			arg = expandarg(w, e);
			if(arg!=nil && do3(w, arg) < 0)
				winwriteevent(w, e);
			free(arg);
			break;
		}
	}
	winfree(w);
}

/*
 * handle a button 3 click
 */
int
do3(Win *w, char *text)
{
	char *addr, *name, *type, *server, *path, *p, *q;
	static char lastfail[1000];

	if(text[0] == '/'){
		p = nil;
		name = estrdup(text);
	}else{
		p = wingetname(w);
		if(text[0] != ':'){
			q = strrchr(p, '/');
			*(q+1) = 0;
		}
		name = emalloc(strlen(p)+1+strlen(text)+1);
		strcpy(name, p);
		if(text[0] != ':')
			strcat(name, "/");
		strcat(name, text);
	}
	cprint("button3 %s %s => %s\n", p, text, name);
	if((addr = strchr(name, ':')) != nil)
		*addr++ = 0;
	cleanname(name);
	cprint("b3 \t=> name=%s addr=%s\n", name, addr);
	if(strcmp(name, lastfail) == 0){
		cprint("b3 \t=> nonexistent (cached)\n");
		free(name);
		return -1;
	}
	if(parsename(name, &server, &path) < 0){
		cprint("b3 \t=> parsename failed\n");
		free(name);
		return -1;
	}
	type = filestat(server, path);
	free(server);
	free(path);
	if(strcmp(type, "file")==0 || strcmp(type, "directory")==0){
		w = nametowin(name);
		if(w == nil){
			w = mkwin(name);
			cprint("b3 \t=> creating new window %d\n", w->id);
		}else
			cprint("b3 \t=> reusing window %d\n", w->id);
		if(addr){
			winaddr(w, "%s", addr);
			winctl(w, "dot=addr");
		}
		winctl(w, "show");
		free(name);
		free(type);
		return 0;
	}
	/*
	 * remember last name that didn't exist so that
	 * only the first right-click is slow when searching for text.
	 */
	cprint("b3 caching %s => type %s\n", name, type);
	strecpy(lastfail, lastfail+sizeof lastfail, name);
	free(name);
	free(type);
	return -1;
}

Win*
mkwin(char *name)
{
	Arg *a;
	Channel *c;
	Win *w;

	c = chancreate(sizeof(void*), 0);
	a = arg(name, nil, c);
	threadcreate(filethread, a, STACK);
	w = recvp(c);
	chanfree(c);
	return w;
}

void
loopthread(void *v)
{
	QLock lk;

	threadsetname("loopthread");
	qlock(&lk);
	qlock(&lk);
}

void
threadmain(int argc, char **argv)
{
	ARGBEGIN{
	case '9':
		chatty9pclient = 1;
		break;
	case 'D':
		debug = 1;
		break;
	default:
		usage();
	}ARGEND

	if(argc)
		usage();

	cprint("netfiles starting\n");

	threadnotify(nil, 0);	/* set up correct default handlers */

	fmtinstall('E', eventfmt);
	doquote = needsrcquote;
	quotefmtinstall();

	twaitinit();
	threadcreate(plumbthread, nil, STACK);
	threadcreate(loopthread, nil, STACK);
	threadexits(nil);
}
