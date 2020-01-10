#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include <mach.h>
#define Extern extern
#include "acid.h"
#include "y.tab.h"

static int syren;

Lsym*
unique(char *buf, Symbol *s)
{
	Lsym *l;
	int i, renamed;

	renamed = 0;
	strcpy(buf, s->xname);
	for(;;) {
		l = look(buf);
		if(l == 0 || (l->lexval == Tid && l->v->set == 0))
			break;

		if(syren == 0 && !quiet) {
			print("Symbol renames:\n");
			syren = 1;
		}
		i = strlen(buf)+1;
		memmove(buf+1, buf, i);
		buf[0] = '$';
		renamed++;
		if(renamed > 5 && !quiet) {
			print("Too many renames; must be X source!\n");
			break;
		}
	}
	if(renamed && !quiet)
		print("\t%s=%s %c/%L\n", s->xname, buf, s->type, s->loc);
	if(l == 0)
		l = enter(buf, Tid);
	s->aux = l;
	return l;
}

void
varsym(void)
{
	Lsym *l;
	Fhdr *fp;

	l = mkvar("symbols");
	if(l->v->set)
		return;

	l->v->set = 1;
	l->v->type = TLIST;
	l->v->store.u.l = nil;

	for(fp=fhdrlist; fp; fp=fp->next){
		if(fp->ftype == FCORE)
			continue;
		addvarsym(fp);
	}
	if(l->v->store.u.l == nil)
		print("no debugging symbols\n");
}

void
addvarsym(Fhdr *fp)
{
	int i;
	Symbol s;
	Lsym *l;
	String *file;
	u64int v;
	char buf[65536];	/* Some of those C++ names are really big */
	List *list, **tail, *tl;

	if(fp == nil)
		return;

	l = look("symbols");
	if(l == nil)
		return;

	l->v->set = 1;
	l->v->type = TLIST;
	tail = &l->v->store.u.l;
	while(*tail)
		tail = &(*tail)->next;

	file = strnode(fp->filename);
	for(i=0; findexsym(fp, i, &s)>=0; i++){
		switch(s.type) {
		case 'T':
		case 'L':
		case 'D':
		case 'B':
		case 'b':
		case 'd':
		case 'l':
		case 't':
			if(s.name[0] == '.')
				continue;
			if(s.loc.type != LADDR)
				continue;
			v = s.loc.addr;
			tl = al(TLIST);
			*tail = tl;
			tail = &tl->next;

			l = unique(buf, &s);
			l->v->set = 1;
			l->v->type = TINT;
			l->v->store.u.ival = v;
			if(l->v->store.comt == 0)
				l->v->store.fmt = 'X';

			/* Enter as list of { name, type, value, file, xname } */
			list = al(TSTRING);
			tl->store.u.l = list;
			list->store.u.string = strnode(buf);
			list->store.fmt = 's';

			list->next = al(TINT);
			list = list->next;
			list->store.fmt = 'c';
			list->store.u.ival = s.type;

			list->next = al(TINT);
			list = list->next;
			list->store.fmt = 'X';
			list->store.u.ival = v;

			list->next = al(TSTRING);
			list = list->next;
			list->store.fmt = 's';
			list->store.u.string = file;

			list->next = al(TSTRING);
			list = list->next;
			list->store.fmt = 's';
			list->store.u.string = strnode(s.name);
		}
	}
	*tail = nil;
}

static int
infile(List *list, char *file, char **name)
{
	/* name */
	if(list->type != TSTRING)
		return 0;
	*name = list->store.u.string->string;
	if(list->next == nil)
		return 0;
	list = list->next;

	/* type character */
	if(list->next == nil)
		return 0;
	list = list->next;

	/* address */
	if(list->next == nil)
		return 0;
	list = list->next;

	/* file */
	if(list->type != TSTRING)
		return 0;
	return strcmp(list->store.u.string->string, file) == 0;
}

void
delvarsym(char *file)
{
	char *name;
	Lsym *l;
	List **lp, *p;

	l = look("symbols");
	if(l == nil)
		return;

	if(l->v->type != TLIST)
		return;

	for(lp=&l->v->store.u.l; *lp; lp=&(*lp)->next){
		while(*lp){
			p = *lp;
			if(p->type != TLIST)
				break;
			if(!infile(p->store.u.l, file, &name))
				break;
			*lp = p->next;
			/* XXX remove from hash tables */
		}
		if(*lp == nil)
			break;
	}
}

void
varreg(void)
{
	Lsym *l;
	Value *v;
	Regdesc *r;
	List **tail, *li;

	l = mkvar("registers");
	v = l->v;
	v->set = 1;
	v->type = TLIST;
	v->store.u.l = 0;
	tail = &v->store.u.l;

	if(mach == nil)
		return;

	for(r = mach->reglist; r->name; r++) {
		l = mkvar(r->name);
		v = l->v;
		v->set = 1;
		v->store.u.reg.name = r->name;
		v->store.u.reg.thread = 0;
		v->store.fmt = r->format;
		v->type = TREG;

		li = al(TSTRING);
		li->store.u.string = strnode(r->name);
		li->store.fmt = 's';
		*tail = li;
		tail = &li->next;
	}

	l = mkvar("bpinst");	/* Breakpoint text */
	v = l->v;
	v->type = TSTRING;
	v->store.fmt = 's';
	v->set = 1;
	v->store.u.string = gmalloc(sizeof(String));
	v->store.u.string->len = mach->bpsize;
	v->store.u.string->string = gmalloc(mach->bpsize);
	memmove(v->store.u.string->string, mach->bpinst, mach->bpsize);
}

void
loadvars(void)
{
	Lsym *l;
	Value *v;

	l =  mkvar("proc");
	v = l->v;
	v->type = TINT;
	v->store.fmt = 'X';
	v->set = 1;
	v->store.u.ival = 0;

	l = mkvar("pid");		/* Current process */
	v = l->v;
	v->type = TINT;
	v->store.fmt = 'D';
	v->set = 1;
	v->store.u.ival = 0;

	mkvar("notes");			/* Pending notes */

	l = mkvar("proclist");		/* Attached processes */
	l->v->type = TLIST;
}

String*
strnodlen(char *name, int len)
{
	String *s;

	s = gmalloc(sizeof(String)+len+1);
	s->string = (char*)s+sizeof(String);
	s->len = len;
	if(name != 0)
		memmove(s->string, name, len);
	s->string[len] = '\0';

	s->gc.gclink = gcl;
	gcl = (Gc*)s;

	return s;
}

String*
strnode(char *name)
{
	return strnodlen(name, strlen(name));
}

String*
runenode(Rune *name)
{
	int len;
	Rune *p;
	String *s;

	p = name;
	for(len = 0; *p; p++)
		len++;

	len++;
	len *= sizeof(Rune);
	s = gmalloc(sizeof(String)+len);
	s->string = (char*)s+sizeof(String);
	s->len = len;
	memmove(s->string, name, len);

	s->gc.gclink = gcl;
	gcl = (Gc*)s;

	return s;
}

String*
stradd(String *l, String *r)
{
	int len;
	String *s;

	len = l->len+r->len;
	s = gmalloc(sizeof(String)+len+1);
	s->gc.gclink = gcl;
	gcl = (Gc*)s;
	s->len = len;
	s->string = (char*)s+sizeof(String);
	memmove(s->string, l->string, l->len);
	memmove(s->string+l->len, r->string, r->len);
	s->string[s->len] = 0;
	return s;
}

int
scmp(String *sr, String *sl)
{
	if(sr->len != sl->len)
		return 0;

	if(memcmp(sr->string, sl->string, sl->len))
		return 0;

	return 1;
}
