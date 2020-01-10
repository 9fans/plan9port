#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include <mach.h>
#define Extern extern
#include "acid.h"

static List **tail;

List*
construct(Node *l)
{
	List *lh, **save;

	save = tail;
	lh = 0;
	tail = &lh;
	build(l);
	tail = save;

	return lh;
}

int
listlen(List *l)
{
	int len;

	len = 0;
	while(l) {
		len++;
		l = l->next;
	}
	return len;
}

void
build(Node *n)
{
	List *l;
	Node res;

	if(n == 0)
		return;

	switch(n->op) {
	case OLIST:
		build(n->left);
		build(n->right);
		return;
	default:
		expr(n, &res);
		l = al(res.type);
		l->store = res.store;
		*tail = l;
		tail = &l->next;
	}
}

List*
addlist(List *l, List *r)
{
	List *f;

	if(l == 0)
		return r;

	for(f = l; f->next; f = f->next)
		;
	f->next = r;

	return l;
}

void
append(Node *r, Node *list, Node *val)
{
	List *l, *f;

	l = al(val->type);
	l->store = val->store;
	l->next = 0;

	r->op = OCONST;
	r->type = TLIST;

	if(list->store.u.l == 0) {
		list->store.u.l = l;
		r->store.u.l = l;
		return;
	}
	for(f = list->store.u.l; f->next; f = f->next)
		;
	f->next = l;
	r->store.u.l = list->store.u.l;
}

int
listcmp(List *l, List *r)
{
	if(l == r)
		return 1;

	while(l) {
		if(r == 0)
			return 0;
		if(l->type != r->type)
			return 0;
		switch(l->type) {
		case TINT:
			if(l->store.u.ival != r->store.u.ival)
				return 0;
			break;
		case TFLOAT:
			if(l->store.u.fval != r->store.u.fval)
				return 0;
			break;
		case TSTRING:
			if(scmp(l->store.u.string, r->store.u.string) == 0)
				return 0;
			break;
		case TLIST:
			if(listcmp(l->store.u.l, r->store.u.l) == 0)
				return 0;
			break;
		}
		l = l->next;
		r = r->next;
	}
	if(l != r)
		return 0;
	return 1;
}

void
nthelem(List *l, int n, Node *res)
{
	if(n < 0)
		error("negative index in []");

	while(l && n--)
		l = l->next;

	res->op = OCONST;
	if(l == 0) {
		res->type = TLIST;
		res->store.u.l = 0;
		return;
	}
	res->type = l->type;
	res->store = l->store;
}

void
delete(List *l, int n, Node *res)
{
	List **tl;

	if(n < 0)
		error("negative index in delete");

	res->op = OCONST;
	res->type = TLIST;
	res->store.u.l = l;

	for(tl = &res->store.u.l; l && n--; l = l->next)
		tl = &l->next;

	if(l == 0)
		error("element beyond end of list");
	*tl = l->next;
}

List*
listvar(char *s, long v)
{
	List *l, *tl;

	tl = al(TLIST);

	l = al(TSTRING);
	tl->store.u.l = l;
	l->store.fmt = 's';
	l->store.u.string = strnode(s);
	l->next = al(TINT);
	l = l->next;
	l->store.fmt = 'X';
	l->store.u.ival = v;

	return tl;
}

static List*
listregisters(Map *map, Regs *regs)
{
	List **tail, *l2, *l;
	Regdesc *rp;
	u64int v;

	l2 = 0;
	tail = &l2;
	for(rp=mach->reglist; rp->name; rp++){
		if(rget(regs, rp->name, &v) < 0)
			continue;
		l = al(TSTRING);
		l->store.fmt = 's';
		l->store.u.string = strnode(rp->name);
		*tail = l;
		tail = &l->next;
		l = al(TINT);
		l->store.fmt = 'X';
		l->store.u.ival = v;
		*tail = l;
		tail = &l->next;
	}
	return l2;
}

static List*
listlocals(Map *map, Regs *regs, Symbol *fn, int class)
{
	int i;
	u32int val;
	Symbol s;
	List **tail, *l2;

	l2 = 0;
	tail = &l2;
	if(fn == nil)
		return l2;
	for(i = 0; indexlsym(fn, i, &s)>=0; i++) {
		if(s.class != class)
			continue;
		if(class == CAUTO && (s.name==0 || s.name[0] == '.'))
			continue;
		if(lget4(map, regs, s.loc, &val) < 0)
			continue;
		*tail = listvar(s.name, val);
		tail = &(*tail)->next;
	}
	return l2;
}

static List*
listparams(Map *map, Regs *regs, Symbol *fn)
{
	return listlocals(map, regs, fn, CPARAM);
}

static List*
listautos(Map *map, Regs *regs, Symbol *fn)
{
	return listlocals(map, regs, fn, CAUTO);
}

int
trlist(Map *map, Regs *regs, u64int pc, u64int callerpc, Symbol *sym, int depth)
{
	List *q, *l;
	static List **tail;

	if (tracelist == 0)		/* first time */
		tail = &tracelist;

	q = al(TLIST);
	*tail = q;
	tail = &q->next;

	l = al(TINT);			/* Function address */
	q->store.u.l = l;
	l->store.u.ival = sym ? sym->loc.addr : pc;
	l->store.fmt = 'X';

	l->next = al(TINT);		/* actual pc address */
	l = l->next;
	l->store.u.ival = pc;
	l->store.fmt = 'X';

	l->next = al(TINT);		/* called from address */
	l = l->next;
	l->store.u.ival = callerpc;
	l->store.fmt = 'X';

	l->next = al(TLIST);		/* make list of params */
	l = l->next;
	if(sym)
		l->store.u.l = listparams(map, regs, sym);

	l->next = al(TLIST);		/* make list of locals */
	l = l->next;
	if(sym)
		l->store.u.l = listautos(map, regs, sym);

	l->next = al(TLIST);		/* make list of registers */
	l = l->next;
	l->store.u.l = listregisters(map, regs);

	return depth<40;
}
