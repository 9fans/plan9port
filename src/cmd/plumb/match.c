#include <u.h>
#include <libc.h>
#include <bio.h>
#include <regexp.h>
#include <thread.h>
#include <plumb.h>
#include "plumber.h"

/*
static char*
nonnil(char *s)
{
	if(s == nil)
		return "";
	return s;
}
*/

int
verbis(int obj, Plumbmsg *m, Rule *r)
{
	switch(obj){
	default:
		fprint(2, "unimplemented 'is' object %d\n", obj);
		break;
	case OData:
		return strcmp(m->data, r->qarg) == 0;
	case ODst:
		return strcmp(m->dst, r->qarg) == 0;
	case OType:
		return strcmp(m->type, r->qarg) == 0;
	case OWdir:
		return strcmp(m->wdir, r->qarg) == 0;
	case OSrc:
		return strcmp(m->src, r->qarg) == 0;
	}
	return 0;
}

static void
setvar(Resub rs[10], char *match[10])
{
	int i, n;

	for(i=0; i<10; i++){
		free(match[i]);
		match[i] = nil;
	}
	for(i=0; i<10 && rs[i].s.sp!=nil; i++){
		n = rs[i].e.ep-rs[i].s.sp;
		match[i] = emalloc(n+1);
		memmove(match[i], rs[i].s.sp, n);
		match[i][n] = '\0';
	}
}

int
clickmatch(Reprog *re, char *text, Resub rs[10], int click)
{
	char *clickp;
	int i, w;
	Rune r;

	/* click is in characters, not bytes */
	for(i=0; i<click && text[i]!='\0'; i+=w)
		w = chartorune(&r, text+i);
	clickp = text+i;
	for(i=0; i<=click; i++){
		memset(rs, 0, 10*sizeof(Resub));
		if(regexec(re, text+i, rs, 10))
			if(rs[0].s.sp<=clickp && clickp<=rs[0].e.ep)
				return 1;
	}
	return 0;
}

int
verbmatches(int obj, Plumbmsg *m, Rule *r, Exec *e)
{
	Resub rs[10];
	char *clickval, *alltext;
	int p0, p1, ntext;

	memset(rs, 0, sizeof rs);
	ntext = -1;
	switch(obj){
	default:
		fprint(2, "unimplemented 'matches' object %d\n", obj);
		break;
	case OData:
		clickval = plumblookup(m->attr, "click");
		if(clickval == nil){
			alltext = m->data;
			ntext = m->ndata;
			goto caseAlltext;
		}
		if(!clickmatch(r->regex, m->data, rs, atoi(clickval)))
			break;
		p0 = rs[0].s.sp - m->data;
		p1 = rs[0].e.ep - m->data;
		if(e->p0 >=0 && !(p0==e->p0 && p1==e->p1))
			break;
		e->clearclick = 1;
		e->setdata = 1;
		e->p0 = p0;
		e->p1 = p1;
		setvar(rs, e->match);
		return 1;
	case ODst:
		alltext = m->dst;
		goto caseAlltext;
	case OType:
		alltext = m->type;
		goto caseAlltext;
	case OWdir:
		alltext = m->wdir;
		goto caseAlltext;
	case OSrc:
		alltext = m->src;
		/* fall through */
	caseAlltext:
		/* must match full text */
		if(ntext < 0)
			ntext = strlen(alltext);
		if(!regexec(r->regex, alltext, rs, 10) || rs[0].s.sp!=alltext || rs[0].e.ep!=alltext+ntext)
			break;
		setvar(rs, e->match);
		return 1;
	}
	return 0;
}

int
isfile(char *file, ulong maskon, ulong maskoff)
{
	Dir *d;
	int mode;

	d = dirstat(file);
	if(d == nil)
		return 0;
	mode = d->mode;
	free(d);
	if((mode & maskon) == 0)
		return 0;
	if(mode & maskoff)
		return 0;
	return 1;
}

char*
absolute(char *dir, char *file)
{
	char *p;

	if(file[0] == '/')
		return estrdup(file);
	p = emalloc(strlen(dir)+1+strlen(file)+1);
	sprint(p, "%s/%s", dir, file);
	return cleanname(p);
}

int
verbisfile(int obj, Plumbmsg *m, Rule *r, Exec *e, ulong maskon, ulong maskoff, char **var)
{
	char *file;

	switch(obj){
	default:
		fprint(2, "unimplemented 'isfile' object %d\n", obj);
		break;
	case OArg:
		file = absolute(m->wdir, expand(e, r->arg, nil));
		if(isfile(file, maskon, maskoff)){
			*var = file;
			return 1;
		}
		free(file);
		break;
	case OData:
	case OWdir:
		file = absolute(m->wdir, obj==OData? m->data : m->wdir);
		if(isfile(file, maskon, maskoff)){
			*var = file;
			return 1;
		}
		free(file);
		break;
	}
	return 0;
}

int
verbset(int obj, Plumbmsg *m, Rule *r, Exec *e)
{
	char *new;

	switch(obj){
	default:
		fprint(2, "unimplemented 'is' object %d\n", obj);
		break;
	case OData:
		new = estrdup(expand(e, r->arg, nil));
		m->ndata = strlen(new);
		free(m->data);
		m->data = new;
		e->p0 = -1;
		e->p1 = -1;
		e->setdata = 0;
		return 1;
	case ODst:
		new = estrdup(expand(e, r->arg, nil));
		free(m->dst);
		m->dst = new;
		return 1;
	case OType:
		new = estrdup(expand(e, r->arg, nil));
		free(m->type);
		m->type = new;
		return 1;
	case OWdir:
		new = estrdup(expand(e, r->arg, nil));
		free(m->wdir);
		m->wdir = new;
		return 1;
	case OSrc:
		new = estrdup(expand(e, r->arg, nil));
		free(m->src);
		m->src = new;
		return 1;
	}
	return 0;
}

int
verbadd(int obj, Plumbmsg *m, Rule *r, Exec *e)
{
	switch(obj){
	default:
		fprint(2, "unimplemented 'add' object %d\n", obj);
		break;
	case OAttr:
		m->attr = plumbaddattr(m->attr, plumbunpackattr(expand(e, r->arg, nil)));
		return 1;
	}
	return 0;
}

int
verbdelete(int obj, Plumbmsg *m, Rule *r, Exec *e)
{
	char *a;

	switch(obj){
	default:
		fprint(2, "unimplemented 'delete' object %d\n", obj);
		break;
	case OAttr:
		a = expand(e, r->arg, nil);
		if(plumblookup(m->attr, a) == nil)
			break;
		m->attr = plumbdelattr(m->attr, a);
		return 1;
	}
	return 0;
}

int
matchpat(Plumbmsg *m, Exec *e, Rule *r)
{
	switch(r->verb){
	default:
		fprint(2, "unimplemented verb %d\n", r->verb);
		break;
	case VAdd:
		return verbadd(r->obj, m, r, e);
	case VDelete:
		return verbdelete(r->obj, m, r, e);
	case VIs:
		return verbis(r->obj, m, r);
	case VIsdir:
		return verbisfile(r->obj, m, r, e, DMDIR, 0, &e->dir);
	case VIsfile:
		return verbisfile(r->obj, m, r, e, ~DMDIR, DMDIR, &e->file);
	case VMatches:
		return verbmatches(r->obj, m, r, e);
	case VSet:
		verbset(r->obj, m, r, e);
		return 1;
	}
	return 0;
}

void
freeexec(Exec *exec)
{
	int i;

	if(exec == nil)
		return;
	free(exec->dir);
	free(exec->file);
	for(i=0; i<10; i++)
		free(exec->match[i]);
	free(exec);
}

Exec*
newexec(Plumbmsg *m)
{
	Exec *exec;

	exec = emalloc(sizeof(Exec));
	exec->msg = m;
	exec->p0 = -1;
	exec->p1 = -1;
	return exec;
}

void
rewrite(Plumbmsg *m, Exec *e)
{
	Plumbattr *a, *prev;

	if(e->clearclick){
		prev = nil;
		for(a=m->attr; a!=nil; a=a->next){
			if(strcmp(a->name, "click") == 0){
				if(prev == nil)
					m->attr = a->next;
				else
					prev->next = a->next;
				free(a->name);
				free(a->value);
				free(a);
				break;
			}
			prev = a;
		}
		if(e->setdata){
			free(m->data);
			m->data = estrdup(expand(e, "$0", nil));
			m->ndata = strlen(m->data);
		}
	}
}

char**
buildargv(char *s, Exec *e)
{
	char **av;
	int ac;

	ac = 0;
	av = nil;
	for(;;){
		av = erealloc(av, (ac+1) * sizeof(char*));
		av[ac] = nil;
		while(*s==' ' || *s=='\t')
			s++;
		if(*s == '\0')
			break;
		av[ac++] = estrdup(expand(e, s, &s));
	}
	return av;
}

Exec*
matchruleset(Plumbmsg *m, Ruleset *rs)
{
	int i;
	Exec *exec;

	if(m->dst!=nil && m->dst[0]!='\0' && rs->port!=nil && strcmp(m->dst, rs->port)!=0)
		return nil;
	exec = newexec(m);
	for(i=0; i<rs->npat; i++)
		if(!matchpat(m, exec, rs->pat[i])){
			freeexec(exec);
			return nil;
		}
	if(rs->port!=nil && (m->dst==nil || m->dst[0]=='\0')){
		free(m->dst);
		m->dst = estrdup(rs->port);
	}
	rewrite(m, exec);
	return exec;
}

enum
{
	NARGS		= 100,
	NARGCHAR	= 8*1024,
	EXECSTACK 	= 32*1024+(NARGS+1)*sizeof(char*)+NARGCHAR
};

/* copy argv to stack and free the incoming strings, so we don't leak argument vectors */
void
stackargv(char **inargv, char *argv[NARGS+1], char args[NARGCHAR])
{
	int i, n;
	char *s, *a;

	s = args;
	for(i=0; i<NARGS; i++){
		a = inargv[i];
		if(a == nil)
			break;
		n = strlen(a)+1;
		if((s-args)+n >= NARGCHAR)	/* too many characters */
			break;
		argv[i] = s;
		memmove(s, a, n);
		s += n;
		free(a);
	}
	argv[i] = nil;
}


void
execproc(void *v)
{
	int fd[3];
	char **av;
	char *args[NARGS+1], argc[NARGCHAR];

	fd[0] = open("/dev/null", OREAD);
	fd[1] = dup(1, -1);
	fd[2] = dup(2, -1);
	av = v;
	stackargv(av, args, argc);
	free(av);
	threadexec(nil, fd, args[0], args);
	threadexits("can't exec");
}

char*
startup(Ruleset *rs, Exec *e)
{
	char **argv;
	int i;

	if(rs != nil)
		for(i=0; i<rs->nact; i++){
			if(rs->act[i]->verb == VStart)
				goto Found;
			if(rs->act[i]->verb == VClient){
				if(e->msg->dst==nil || e->msg->dst[0]=='\0')
					return "no port for \"client\" rule";
				e->holdforclient = 1;
				goto Found;
			}
		}
	return "no start action for plumb message";

Found:
	argv = buildargv(rs->act[i]->arg, e);
	if(argv[0] == nil)
		return "empty argument list";
	threadcreate(execproc, argv, EXECSTACK);
	return nil;
}
