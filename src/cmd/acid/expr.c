#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include <mach.h>
#define Extern extern
#include "acid.h"

static int fsize[256];

static void
initfsize(void)
{
	fsize['A'] = 4;
	fsize['B'] = 4;
	fsize['C'] = 1;
	fsize['D'] = 4;
	fsize['F'] = 8;
	fsize['G'] = 8;
	fsize['O'] = 4;
	fsize['Q'] = 4;
	fsize['R'] = 4;
	fsize['S'] = 4;
	fsize['U'] = 4;
	fsize['V'] = 8;
	fsize['X'] = 4;
	fsize['Y'] = 8;
	fsize['W'] = 8;
	fsize['Z'] = 8;
	fsize['a'] = 4;
	fsize['b'] = 1;
	fsize['c'] = 1;
	fsize['d'] = 2;
	fsize['f'] = 4;
	fsize['g'] = 4;
	fsize['o'] = 2;
	fsize['q'] = 2;
	fsize['r'] = 2;
	fsize['s'] = 4;
	fsize['u'] = 2;
	fsize['x'] = 2;
}

int
fmtsize(Value *v)
{
	int ret;

	switch(v->store.fmt) {
	default:
		return  fsize[(unsigned char)v->store.fmt];
	case 'i':
	case 'I':
		if(v->type != TINT || mach == 0)
			error("no size for i fmt pointer ++/--");
		ret = (*mach->instsize)(symmap, v->store.u.ival);
		if(ret < 0) {
			ret = (*mach->instsize)(symmap, v->store.u.ival);
			if(ret < 0)
				error("%r");
		}
		return ret;
	}
}

Lsym*
chklval(Node *lp)
{
	Node res;
	Lsym *s;

	if(lp->op == ONAME)
		return lp->sym;

	if(lp->op == OCALL){
		s = chklval(lp->left);
		if(strcmp(s->name, "var") == 0
		&& (lp->builtin || s->proc == 0)){
			if(lp->right == 0)
				error("var(string): arg count");
			expr(lp->right, &res);
			if(res.type != TSTRING)
				error("var(string): arg type");
			return mkvar(res.store.u.string->string);
		}
	}
	error("need l-value");
	return nil;
}

void
olist(Node *n, Node *res)
{
	expr(n->left, res);
	expr(n->right, res);
}

void
oeval(Node *n, Node *res)
{
	expr(n->left, res);
	if(res->type != TCODE)
		error("bad type for eval");
	expr(res->store.u.cc, res);
}

void
ocast(Node *n, Node *res)
{
	if(n->sym->lt == 0)
		error("%s is not a complex type", n->sym->name);

	expr(n->left, res);
	res->store.comt = n->sym->lt;
	res->store.fmt = 'a';
}

void
oindm(Node *n, Node *res)
{
	Map *m;
	Node l;

	m = cormap;
	if(m == 0)
		m = symmap;
	expr(n->left, &l);
	switch(l.type){
	default:
		error("bad type for *");
	case TINT:
		if(m == 0)
			error("no map for *");
		indir(m, l.store.u.ival, l.store.fmt, res);
		res->store.comt = l.store.comt;
		break;
	case TREG:
		indirreg(correg, l.store.u.reg.name, l.store.fmt, res);
		res->store.comt = l.store.comt;
		break;
	case TCON:
		*res = *l.store.u.con;
		res->store.comt = l.store.comt;
		break;
	}
}

void
oindc(Node *n, Node *res)
{
	Map *m;
	Node l;

	m = symmap;
	if(m == 0)
		m = cormap;
	expr(n->left, &l);
	if(l.type != TINT)
		error("bad type for @");
	if(m == 0)
		error("no map for @");
	indir(m, l.store.u.ival, l.store.fmt, res);
	res->store.comt = l.store.comt;
}

void
oframe(Node *n, Node *res)
{
	char *p;
	Node *lp;
	u64int ival;
	Frtype *f;

	p = n->sym->name;
	while(*p && *p == '$')
		p++;
	lp = n->left;
	if(localaddr(cormap, acidregs, p, lp->sym->name, &ival) < 0)
		error("colon: %r");

	res->store.u.ival = ival;
	res->op = OCONST;
	res->store.fmt = 'X';
	res->type = TINT;

	/* Try and set comt */
	for(f = n->sym->local; f; f = f->next) {
		if(f->var == lp->sym) {
			res->store.comt = f->type;
			res->store.fmt = 'a';
			break;
		}
	}
}

void
oindex(Node *n, Node *res)
{
	Node l, r;

	expr(n->left, &l);
	expr(n->right, &r);

	if(r.type != TINT)
		error("bad type for []");

	switch(l.type) {
	default:
		error("lhs[] has bad type");
	case TINT:
		indir(cormap, l.store.u.ival+(r.store.u.ival*fsize[(unsigned char)l.store.fmt]), l.store.fmt, res);
		res->store.comt = l.store.comt;
		res->store.fmt = l.store.fmt;
		break;
	case TLIST:
		nthelem(l.store.u.l, r.store.u.ival, res);
		break;
	case TSTRING:
		res->store.u.ival = 0;
		if(r.store.u.ival >= 0 && r.store.u.ival < l.store.u.string->len) {
			int xx8;	/* to get around bug in vc */
			xx8 = r.store.u.ival;
			res->store.u.ival = l.store.u.string->string[xx8];
		}
		res->op = OCONST;
		res->type = TINT;
		res->store.fmt = 'c';
		break;
	}
}

void
oappend(Node *n, Node *res)
{
	Node r, l;

	expr(n->left, &l);
	expr(n->right, &r);
	if(l.type != TLIST)
		error("must append to list");
	append(res, &l, &r);
}

void
odelete(Node *n, Node *res)
{
	Node l, r;

	expr(n->left, &l);
	expr(n->right, &r);
	if(l.type != TLIST)
		error("must delete from list");
	if(r.type != TINT)
		error("delete index must be integer");

	delete(l.store.u.l, r.store.u.ival, res);
}

void
ohead(Node *n, Node *res)
{
	Node l;

	expr(n->left, &l);
	if(l.type != TLIST)
		error("head needs list");
	res->op = OCONST;
	if(l.store.u.l) {
		res->type = l.store.u.l->type;
		res->store = l.store.u.l->store;
	}
	else {
		res->type = TLIST;
		res->store.u.l = 0;
	}
}

void
otail(Node *n, Node *res)
{
	Node l;

	expr(n->left, &l);
	if(l.type != TLIST)
		error("tail needs list");
	res->op = OCONST;
	res->type = TLIST;
	if(l.store.u.l)
		res->store.u.l = l.store.u.l->next;
	else
		res->store.u.l = 0;
}

void
oconst(Node *n, Node *res)
{
	res->op = OCONST;
	res->type = n->type;
	res->store = n->store;
	res->store.comt = n->store.comt;
}

void
oname(Node *n, Node *res)
{
	Value *v;

	v = n->sym->v;
	if(v->set == 0)
		error("%s used but not set", n->sym->name);
	res->op = OCONST;
	res->type = v->type;
	res->store = v->store;
	res->store.comt = v->store.comt;
}

void
octruct(Node *n, Node *res)
{
	res->op = OCONST;
	res->type = TLIST;
	res->store.u.l = construct(n->left);
}

void
oasgn(Node *n, Node *res)
{
	Node *lp, r;
	Node aes;
	Value *v;

	lp = n->left;
	switch(lp->op) {
	case OINDM:
		expr(lp->left, &aes);
		if(aes.type == TREG)
			windirreg(correg, aes.store.u.reg.name, n->right, res);
		else
			windir(cormap, aes, n->right, res);
		break;
	case OINDC:
		expr(lp->left, &aes);
		windir(symmap, aes, n->right, res);
		break;
	default:
		v = chklval(lp)->v;
		expr(n->right, &r);
		v->set = 1;
		v->type = r.type;
		v->store = r.store;
		res->op = OCONST;
		res->type = v->type;
		res->store = v->store;
		res->store.comt = v->store.comt;
	}
}

void
oadd(Node *n, Node *res)
{
	Node l, r;

	expr(n->left, &l);
	expr(n->right, &r);
	res->store.fmt = l.store.fmt;
	res->op = OCONST;
	res->type = TFLOAT;
	switch(l.type) {
	default:
		error("bad lhs type +");
	case TINT:
		switch(r.type) {
		case TINT:
			res->type = TINT;
			res->store.u.ival = l.store.u.ival+r.store.u.ival;
			break;
		case TFLOAT:
			res->store.u.fval = l.store.u.ival+r.store.u.fval;
			break;
		default:
			error("bad rhs type +");
		}
		break;
	case TFLOAT:
		switch(r.type) {
		case TINT:
			res->store.u.fval = l.store.u.fval+r.store.u.ival;
			break;
		case TFLOAT:
			res->store.u.fval = l.store.u.fval+r.store.u.fval;
			break;
		default:
			error("bad rhs type +");
		}
		break;
	case TSTRING:
		if(r.type == TSTRING) {
			res->type = TSTRING;
			res->store.fmt = 's';
			res->store.u.string = stradd(l.store.u.string, r.store.u.string);
			break;
		}
		error("bad rhs for +");
	case TLIST:
		res->type = TLIST;
		switch(r.type) {
		case TLIST:
			res->store.u.l = addlist(l.store.u.l, r.store.u.l);
			break;
		default:
			r.left = 0;
			r.right = 0;
			res->store.u.l = addlist(l.store.u.l, construct(&r));
			break;
		}
	}
}

void
osub(Node *n, Node *res)
{
	Node l, r;

	expr(n->left, &l);
	expr(n->right, &r);
	res->store.fmt = l.store.fmt;
	res->op = OCONST;
	res->type = TFLOAT;
	switch(l.type) {
	default:
		error("bad lhs type -");
	case TINT:
		switch(r.type) {
		case TINT:
			res->type = TINT;
			res->store.u.ival = l.store.u.ival-r.store.u.ival;
			break;
		case TFLOAT:
			res->store.u.fval = l.store.u.ival-r.store.u.fval;
			break;
		default:
			error("bad rhs type -");
		}
		break;
	case TFLOAT:
		switch(r.type) {
		case TINT:
			res->store.u.fval = l.store.u.fval-r.store.u.ival;
			break;
		case TFLOAT:
			res->store.u.fval = l.store.u.fval-r.store.u.fval;
			break;
		default:
			error("bad rhs type -");
		}
		break;
	}
}

void
omul(Node *n, Node *res)
{
	Node l, r;

	expr(n->left, &l);
	expr(n->right, &r);
	res->store.fmt = l.store.fmt;
	res->op = OCONST;
	res->type = TFLOAT;
	switch(l.type) {
	default:
		error("bad lhs type *");
	case TINT:
		switch(r.type) {
		case TINT:
			res->type = TINT;
			res->store.u.ival = l.store.u.ival*r.store.u.ival;
			break;
		case TFLOAT:
			res->store.u.fval = l.store.u.ival*r.store.u.fval;
			break;
		default:
			error("bad rhs type *");
		}
		break;
	case TFLOAT:
		switch(r.type) {
		case TINT:
			res->store.u.fval = l.store.u.fval*r.store.u.ival;
			break;
		case TFLOAT:
			res->store.u.fval = l.store.u.fval*r.store.u.fval;
			break;
		default:
			error("bad rhs type *");
		}
		break;
	}
}

void
odiv(Node *n, Node *res)
{
	Node l, r;

	expr(n->left, &l);
	expr(n->right, &r);
	res->store.fmt = l.store.fmt;
	res->op = OCONST;
	res->type = TFLOAT;
	switch(l.type) {
	default:
		error("bad lhs type /");
	case TINT:
		switch(r.type) {
		case TINT:
			res->type = TINT;
			if(r.store.u.ival == 0)
				error("zero divide");
			res->store.u.ival = l.store.u.ival/r.store.u.ival;
			break;
		case TFLOAT:
			if(r.store.u.fval == 0)
				error("zero divide");
			res->store.u.fval = l.store.u.ival/r.store.u.fval;
			break;
		default:
			error("bad rhs type /");
		}
		break;
	case TFLOAT:
		switch(r.type) {
		case TINT:
			res->store.u.fval = l.store.u.fval/r.store.u.ival;
			break;
		case TFLOAT:
			res->store.u.fval = l.store.u.fval/r.store.u.fval;
			break;
		default:
			error("bad rhs type /");
		}
		break;
	}
}

void
omod(Node *n, Node *res)
{
	Node l, r;

	expr(n->left, &l);
	expr(n->right, &r);
	res->store.fmt = l.store.fmt;
	res->op = OCONST;
	res->type = TINT;
	if(l.type != TINT || r.type != TINT)
		error("bad expr type %");
	res->store.u.ival = l.store.u.ival%r.store.u.ival;
}

void
olsh(Node *n, Node *res)
{
	Node l, r;

	expr(n->left, &l);
	expr(n->right, &r);
	res->store.fmt = l.store.fmt;
	res->op = OCONST;
	res->type = TINT;
	if(l.type != TINT || r.type != TINT)
		error("bad expr type <<");
	res->store.u.ival = l.store.u.ival<<r.store.u.ival;
}

void
orsh(Node *n, Node *res)
{
	Node l, r;

	expr(n->left, &l);
	expr(n->right, &r);
	res->store.fmt = l.store.fmt;
	res->op = OCONST;
	res->type = TINT;
	if(l.type != TINT || r.type != TINT)
		error("bad expr type >>");
	res->store.u.ival = (unsigned)l.store.u.ival>>r.store.u.ival;
}

void
olt(Node *n, Node *res)
{
	Node l, r;

	expr(n->left, &l);
	expr(n->right, &r);

	res->store.fmt = l.store.fmt;
	res->op = OCONST;
	res->type = TINT;
	switch(l.type) {
	default:
		error("bad lhs type <");
	case TINT:
		switch(r.type) {
		case TINT:
			res->store.u.ival = l.store.u.ival < r.store.u.ival;
			break;
		case TFLOAT:
			res->store.u.ival = l.store.u.ival < r.store.u.fval;
			break;
		default:
			error("bad rhs type <");
		}
		break;
	case TFLOAT:
		switch(r.type) {
		case TINT:
			res->store.u.ival = l.store.u.fval < r.store.u.ival;
			break;
		case TFLOAT:
			res->store.u.ival = l.store.u.fval < r.store.u.fval;
			break;
		default:
			error("bad rhs type <");
		}
		break;
	}
}

void
ogt(Node *n, Node *res)
{
	Node l, r;

	expr(n->left, &l);
	expr(n->right, &r);
	res->store.fmt = 'D';
	res->op = OCONST;
	res->type = TINT;
	switch(l.type) {
	default:
		error("bad lhs type >");
	case TINT:
		switch(r.type) {
		case TINT:
			res->store.u.ival = l.store.u.ival > r.store.u.ival;
			break;
		case TFLOAT:
			res->store.u.ival = l.store.u.ival > r.store.u.fval;
			break;
		default:
			error("bad rhs type >");
		}
		break;
	case TFLOAT:
		switch(r.type) {
		case TINT:
			res->store.u.ival = l.store.u.fval > r.store.u.ival;
			break;
		case TFLOAT:
			res->store.u.ival = l.store.u.fval > r.store.u.fval;
			break;
		default:
			error("bad rhs type >");
		}
		break;
	}
}

void
oleq(Node *n, Node *res)
{
	Node l, r;

	expr(n->left, &l);
	expr(n->right, &r);
	res->store.fmt = 'D';
	res->op = OCONST;
	res->type = TINT;
	switch(l.type) {
	default:
		error("bad expr type <=");
	case TINT:
		switch(r.type) {
		case TINT:
			res->store.u.ival = l.store.u.ival <= r.store.u.ival;
			break;
		case TFLOAT:
			res->store.u.ival = l.store.u.ival <= r.store.u.fval;
			break;
		default:
			error("bad expr type <=");
		}
		break;
	case TFLOAT:
		switch(r.type) {
		case TINT:
			res->store.u.ival = l.store.u.fval <= r.store.u.ival;
			break;
		case TFLOAT:
			res->store.u.ival = l.store.u.fval <= r.store.u.fval;
			break;
		default:
			error("bad expr type <=");
		}
		break;
	}
}

void
ogeq(Node *n, Node *res)
{
	Node l, r;

	expr(n->left, &l);
	expr(n->right, &r);
	res->store.fmt = 'D';
	res->op = OCONST;
	res->type = TINT;
	switch(l.type) {
	default:
		error("bad lhs type >=");
	case TINT:
		switch(r.type) {
		case TINT:
			res->store.u.ival = l.store.u.ival >= r.store.u.ival;
			break;
		case TFLOAT:
			res->store.u.ival = l.store.u.ival >= r.store.u.fval;
			break;
		default:
			error("bad rhs type >=");
		}
		break;
	case TFLOAT:
		switch(r.type) {
		case TINT:
			res->store.u.ival = l.store.u.fval >= r.store.u.ival;
			break;
		case TFLOAT:
			res->store.u.ival = l.store.u.fval >= r.store.u.fval;
			break;
		default:
			error("bad rhs type >=");
		}
		break;
	}
}

void
oeq(Node *n, Node *res)
{
	Node l, r;

	expr(n->left, &l);
	expr(n->right, &r);
	res->store.fmt = 'D';
	res->op = OCONST;
	res->type = TINT;
	res->store.u.ival = 0;
	switch(l.type) {
	default:
		break;
	case TINT:
		switch(r.type) {
		case TINT:
			res->store.u.ival = l.store.u.ival == r.store.u.ival;
			break;
		case TFLOAT:
			res->store.u.ival = l.store.u.ival == r.store.u.fval;
			break;
		default:
			break;
		}
		break;
	case TFLOAT:
		switch(r.type) {
		case TINT:
			res->store.u.ival = l.store.u.fval == r.store.u.ival;
			break;
		case TFLOAT:
			res->store.u.ival = l.store.u.fval == r.store.u.fval;
			break;
		default:
			break;
		}
		break;
	case TSTRING:
		if(r.type == TSTRING) {
			res->store.u.ival = scmp(r.store.u.string, l.store.u.string);
			break;
		}
		break;
	case TLIST:
		if(r.type == TLIST) {
			res->store.u.ival = listcmp(l.store.u.l, r.store.u.l);
			break;
		}
		break;
	}
	if(n->op == ONEQ)
		res->store.u.ival = !res->store.u.ival;
}


void
oland(Node *n, Node *res)
{
	Node l, r;

	expr(n->left, &l);
	expr(n->right, &r);
	res->store.fmt = l.store.fmt;
	res->op = OCONST;
	res->type = TINT;
	if(l.type != TINT || r.type != TINT)
		error("bad expr type &");
	res->store.u.ival = l.store.u.ival&r.store.u.ival;
}

void
oxor(Node *n, Node *res)
{
	Node l, r;

	expr(n->left, &l);
	expr(n->right, &r);
	res->store.fmt = l.store.fmt;
	res->op = OCONST;
	res->type = TINT;
	if(l.type != TINT || r.type != TINT)
		error("bad expr type ^");
	res->store.u.ival = l.store.u.ival^r.store.u.ival;
}

void
olor(Node *n, Node *res)
{
	Node l, r;

	expr(n->left, &l);
	expr(n->right, &r);
	res->store.fmt = l.store.fmt;
	res->op = OCONST;
	res->type = TINT;
	if(l.type != TINT || r.type != TINT)
		error("bad expr type |");
	res->store.u.ival = l.store.u.ival|r.store.u.ival;
}

void
ocand(Node *n, Node *res)
{
	Node l, r;

	res->store.fmt = 'D';
	res->op = OCONST;
	res->type = TINT;
	res->store.u.ival = 0;
	expr(n->left, &l);
	res->store.fmt = l.store.fmt;
	if(bool(&l) == 0)
		return;
	expr(n->right, &r);
	if(bool(&r) == 0)
		return;
	res->store.u.ival = 1;
}

void
onot(Node *n, Node *res)
{
	Node l;

	res->op = OCONST;
	res->type = TINT;
	res->store.u.ival = 0;
	expr(n->left, &l);
	if(bool(&l) == 0)
		res->store.u.ival = 1;
}

void
ocor(Node *n, Node *res)
{
	Node l, r;

	res->op = OCONST;
	res->type = TINT;
	res->store.u.ival = 0;
	expr(n->left, &l);
	if(bool(&l)) {
		res->store.u.ival = 1;
		return;
	}
	expr(n->right, &r);
	if(bool(&r)) {
		res->store.u.ival = 1;
		return;
	}
}

void
oeinc(Node *n, Node *res)
{
	Value *v;

	v = chklval(n->left)->v;
	res->op = OCONST;
	res->type = v->type;
	switch(v->type) {
	case TINT:
		if(n->op == OEDEC)
			v->store.u.ival -= fmtsize(v);
		else
			v->store.u.ival += fmtsize(v);
		break;
	case TFLOAT:
		if(n->op == OEDEC)
			v->store.u.fval--;
		else
			v->store.u.fval++;
		break;
	default:
		error("bad type for pre --/++");
	}
	res->store = v->store;
}

void
opinc(Node *n, Node *res)
{
	Value *v;

	v = chklval(n->left)->v;
	res->op = OCONST;
	res->type = v->type;
	res->store = v->store;
	switch(v->type) {
	case TINT:
		if(n->op == OPDEC)
			v->store.u.ival -= fmtsize(v);
		else
			v->store.u.ival += fmtsize(v);
		break;
	case TFLOAT:
		if(n->op == OPDEC)
			v->store.u.fval--;
		else
			v->store.u.fval++;
		break;
	default:
		error("bad type for post --/++");
	}
}

void
ocall(Node *n, Node *res)
{
	Lsym *s;
	Rplace *rsav;

	res->op = OCONST;		/* Default return value */
	res->type = TLIST;
	res->store.u.l = 0;

	s = chklval(n->left);
	if(n->builtin && !s->builtin){
		error("no builtin %s", s->name);
		return;
	}
	if(s->builtin && (n->builtin || s->proc == 0)) {
		(*s->builtin)(res, n->right);
		return;
	}
	if(s->proc == 0)
		error("no function %s", s->name);

	rsav = ret;
	call(s->name, n->right, s->proc->left, s->proc->right, res);
	ret = rsav;
}

void
ofmt(Node *n, Node *res)
{
	expr(n->left, res);
	res->store.fmt = n->right->store.u.ival;
}

void
ouplus(Node *n, Node *res)
{
	expr(n->left, res);
}

void
owhat(Node *n, Node *res)
{
	res->op = OCONST;		/* Default return value */
	res->type = TLIST;
	res->store.u.l = 0;
	whatis(n->sym);
}

void (*expop[NUMO])(Node*, Node*);

static void
initexpop(void)
{
	expop[ONAME] = oname;
	expop[OCONST] = oconst;
	expop[OMUL] = omul;
	expop[ODIV] = odiv;
	expop[OMOD] = omod;
	expop[OADD] = oadd;
	expop[OSUB] = osub;
	expop[ORSH] = orsh;
	expop[OLSH] = olsh;
	expop[OLT] = olt;
	expop[OGT] = ogt;
	expop[OLEQ] = oleq;
	expop[OGEQ] = ogeq;
	expop[OEQ] = oeq;
	expop[ONEQ] = oeq;
	expop[OLAND] = oland;
	expop[OXOR] = oxor;
	expop[OLOR] = olor;
	expop[OCAND] = ocand;
	expop[OCOR] = ocor;
	expop[OASGN] = oasgn;
	expop[OINDM] = oindm;
	expop[OEDEC] = oeinc;
	expop[OEINC] = oeinc;
	expop[OPINC] = opinc;
	expop[OPDEC] = opinc;
	expop[ONOT] = onot;
	expop[OIF] = 0;
	expop[ODO] = 0;
	expop[OLIST] = olist;
	expop[OCALL] = ocall;
	expop[OCTRUCT] = octruct;
	expop[OWHILE] =0;
	expop[OELSE] = 0;
	expop[OHEAD] = ohead;
	expop[OTAIL] = otail;
	expop[OAPPEND] = oappend;
	expop[ORET] = 0;
	expop[OINDEX] =oindex;
	expop[OINDC] = oindc;
	expop[ODOT] = odot;
	expop[OLOCAL] =0;
	expop[OFRAME] = oframe;
	expop[OCOMPLEX] =0;
	expop[ODELETE] = odelete;
	expop[OCAST] = ocast;
	expop[OFMT] = ofmt;
	expop[OEVAL] = oeval;
	expop[OWHAT] = owhat;
	expop[OUPLUS] = ouplus;
}

void
initexpr(void)
{
	initfsize();
	initexpop();
}

int
acidregsrw(Regs *r, char *name, u64int *u, int isr)
{
	Lsym *l;
	Value *v;
	Node *n;
	u64int addr;

	if(!isr){
		werrstr("cannot write registers");
		return -1;
	}
	USED(r);
	l = look(name);
	if(l == nil){
		werrstr("register %s not found", name);
		return -1;
	}
	v = l->v;
	switch(v->type){
	default:
		werrstr("*%s: bad type", name);
		return -1;
	case TREG:
		if(correg == nil){
			werrstr("*%s: register %s not mapped", name, v->store.u.reg);
			return -1;
		}
		return rget(correg, v->store.u.reg.name, u);
	case TCON:
		n = v->store.u.con;
		if(n->op != OCONST || n->type != TINT){
			werrstr("*%s: bad register constant", name);
			return -1;
		}
		*u = n->store.u.ival;
		return 0;
	case TINT:
		if(cormap == nil){
			werrstr("*%s: core not mapped", name);
			return -1;
		}
		addr = v->store.u.ival;
		/* XXX should use format to determine size */
		if(geta(cormap, addr, u) < 0)
			return -1;
		return 0;
	}
}
