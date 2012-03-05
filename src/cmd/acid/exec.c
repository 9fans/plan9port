#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include <mach.h>
#define Extern extern
#include "acid.h"

void
error(char *fmt, ...)
{
	int i;
	char buf[2048];
	va_list arg;

	/* Unstack io channels */
	if(iop != 0) {
		for(i = 1; i < iop; i++)
			Bterm(io[i]);
		bout = io[0];
		iop = 0;
	}

	ret = 0;
	gotint = 0;
	Bflush(bout);
	if(silent)
		silent = 0;
	else {
		va_start(arg, fmt);
		vseprint(buf, buf+sizeof(buf), fmt, arg);
		va_end(arg);
		fprint(2, "%Z: (error) %s\n", buf);
	}
	while(popio())
		;
	interactive = 1;
	longjmp(err, 1);
}

void
unwind(void)
{
	int i;
	Lsym *s;
	Value *v;

	for(i = 0; i < Hashsize; i++) {
		for(s = hash[i]; s; s = s->hash) {
			while(s->v->pop) {
				v = s->v->pop;
				free(s->v);
				s->v = v;
			}
		}
	}
}

void
execute(Node *n)
{
	Value *v;
	Lsym *sl;
	Node *l, *r;
	int i, s, e;
	Node res, xx;
	static int stmnt;

	gc();
	if(gotint)
		error("interrupted");

	if(n == 0)
		return;

	if(stmnt++ > 5000) {
		Bflush(bout);
		stmnt = 0;
	}

	l = n->left;
	r = n->right;

	switch(n->op) {
	default:
		expr(n, &res);
		if(ret || (res.type == TLIST && res.store.u.l == 0))
			break;
		prnt->right = &res;
		expr(prnt, &xx);
		break;
	case OASGN:
	case OCALL:
		expr(n, &res);
		break;
	case OCOMPLEX:
		decl(n);
		break;
	case OLOCAL:
		for(n = n->left; n; n = n->left) {
			if(ret == 0)
				error("local not in function");
			sl = n->sym;
			if(sl->v->ret == ret)
				error("%s declared twice", sl->name);
			v = gmalloc(sizeof(Value));
			v->ret = ret;
			v->pop = sl->v;
			sl->v = v;
			v->scope = 0;
			*(ret->tail) = sl;
			ret->tail = &v->scope;
			v->set = 0;
		}
		break;
	case ORET:
		if(ret == 0)
			error("return not in function");
		expr(n->left, ret->val);
		longjmp(ret->rlab, 1);
	case OLIST:
		execute(n->left);
		execute(n->right);
		break;
	case OIF:
		expr(l, &res);
		if(r && r->op == OELSE) {
			if(bool(&res))
				execute(r->left);
			else
				execute(r->right);
		}
		else if(bool(&res))
			execute(r);
		break;
	case OWHILE:
		for(;;) {
			expr(l, &res);
			if(!bool(&res))
				break;
			execute(r);
		}
		break;
	case ODO:
		expr(l->left, &res);
		if(res.type != TINT)
			error("loop must have integer start");
		s = res.store.u.ival;
		expr(l->right, &res);
		if(res.type != TINT)
			error("loop must have integer end");
		e = res.store.u.ival;
		for(i = s; i <= e; i++)
			execute(r);
		break;
	}
}

int
bool(Node *n)
{
	int true = 0;

	if(n->op != OCONST)
		fatal("bool: not const");

	switch(n->type) {
	case TINT:
		if(n->store.u.ival != 0)
			true = 1;
		break;
	case TFLOAT:
		if(n->store.u.fval != 0.0)
			true = 1;
		break;
	case TSTRING:
		if(n->store.u.string->len)
			true = 1;
		break;
	case TLIST:
		if(n->store.u.l)
			true = 1;
		break;
	}
	return true;
}

void
convflt(Node *r, char *flt)
{
	char c;

	c = flt[0];
	if(('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z')) {
		r->type = TSTRING;
		r->store.fmt = 's';
		r->store.u.string = strnode(flt);
	}
	else {
		r->type = TFLOAT;
		r->store.u.fval = atof(flt);
	}
}

void
indir(Map *m, u64int addr, char fmt, Node *r)
{
	int i;
	u32int ival;
	u64int vval;
	int ret;
	u8int cval;
	u16int sval;
	char buf[512], reg[12];

	r->op = OCONST;
	r->store.fmt = fmt;
	switch(fmt) {
	default:
		error("bad pointer format '%c' for *", fmt);
	case 'c':
	case 'C':
	case 'b':
		r->type = TINT;
		ret = get1(m, addr, &cval, 1);
		if (ret < 0)
			error("indir: %r");
		r->store.u.ival = cval;
		break;
	case 'x':
	case 'd':
	case 'u':
	case 'o':
	case 'q':
	case 'r':
		r->type = TINT;
		ret = get2(m, addr, &sval);
		if (ret < 0)
			error("indir: %r");
		r->store.u.ival = sval;
		break;
	case 'a':
	case 'A':
	case 'B':
	case 'X':
	case 'D':
	case 'U':
	case 'O':
	case 'Q':
		r->type = TINT;
		ret = get4(m, addr, &ival);
		if (ret < 0)
			error("indir: %r");
		r->store.u.ival = ival;
		break;
	case 'V':
	case 'W':
	case 'Y':
	case 'Z':
		r->type = TINT;
		ret = get8(m, addr, &vval);
		if (ret < 0)
			error("indir: %r");
		r->store.u.ival = vval;
		break;
	case 's':
		r->type = TSTRING;
		for(i = 0; i < sizeof(buf)-1; i++) {
			ret = get1(m, addr, (uchar*)&buf[i], 1);
			if (ret < 0)
				error("indir: %r");
			addr++;
			if(buf[i] == '\0')
				break;
		}
		buf[i] = 0;
		if(i == 0)
			strcpy(buf, "(null)");
		r->store.u.string = strnode(buf);
		break;
	case 'R':
		r->type = TSTRING;
		assert(sizeof(Rune) == 4);
		for(i = 0; i < sizeof(buf)-4; i += 4) {
			ret = get4(m, addr, &ival);
			if (ret < 0)
				error("indir: %r");
			memmove(buf+i, &ival, 4);
			addr += 4;
			if(ival == 0)
				break;
		}
		ival = 0;
		memmove(buf+i, &ival, 4);
		r->store.u.string = runenode((Rune*)buf);
		break;
	case 'i':
	case 'I':
		if ((*mach->das)(m, addr, fmt, buf, sizeof(buf)) < 0)
			error("indir: %r");
		r->type = TSTRING;
		r->store.fmt = 's';
		r->store.u.string = strnode(buf);
		break;
	case 'f':
		ret = get1(m, addr, (uchar*)buf, mach->szfloat);
		if (ret < 0)
			error("indir: %r");
		mach->ftoa32(buf, sizeof(buf), (void*) buf);
		convflt(r, buf);
		break;
	case 'g':
		ret = get1(m, addr, (uchar*)buf, mach->szfloat);
		if (ret < 0)
			error("indir: %r");
		mach->ftoa32(buf, sizeof(buf), (void*) buf);
		r->type = TSTRING;
		r->store.u.string = strnode(buf);
		break;
	case 'F':
		ret = get1(m, addr, (uchar*)buf, mach->szdouble);
		if (ret < 0)
			error("indir: %r");
		mach->ftoa64(buf, sizeof(buf), (void*) buf);
		convflt(r, buf);
		break;
	case '3':	/* little endian ieee 80 with hole in bytes 8&9 */
		ret = get1(m, addr, (uchar*)reg, 10);
		if (ret < 0)
			error("indir: %r");
		memmove(reg+10, reg+8, 2);	/* open hole */
		memset(reg+8, 0, 2);		/* fill it */
		leieeeftoa80(buf, sizeof(buf), reg);
		convflt(r, buf);
		break;
	case '8':	/* big-endian ieee 80 */
		ret = get1(m, addr, (uchar*)reg, 10);
		if (ret < 0)
			error("indir: %r");
		beieeeftoa80(buf, sizeof(buf), reg);
		convflt(r, buf);
		break;
	case 'G':
		ret = get1(m, addr, (uchar*)buf, mach->szdouble);
		if (ret < 0)
			error("indir: %r");
		mach->ftoa64(buf, sizeof(buf), (void*) buf);
		r->type = TSTRING;
		r->store.u.string = strnode(buf);
		break;
	}
}

void
indirreg(Regs *regs, char *name, char fmt, Node *r)
{
	u64int val;

	if(regs == 0)
		error("no register set for *%s=", name);

	r->op = OCONST;
	r->store.fmt = fmt;
	switch(fmt){
	default:
		error("bad pointer format '%c' for *%s", fmt, name);
	case 'c':
	case 'C':
	case 'b':
	case 'x':
	case 'd':
	case 'u':
	case 'o':
	case 'q':
	case 'r':
	case 'a':
	case 'A':
	case 'B':
	case 'X':
	case 'D':
	case 'U':
	case 'O':
	case 'Q':
	case 'V':
	case 'W':
	case 'Y':
	case 'Z':
		if(rget(regs, name, &val) < 0)
			error("reading %s: %r", name);
		r->type = TINT;
		r->store.u.ival = val;
		break;
	case 'f':
	case 'g':
	case 'F':
	case '3':
	case '8':
	case 'G':
		error("floating point registers not supported");
		break;
	}
}

void
windir(Map *m, Node aes, Node *rval, Node *r)
{
	uchar cval;
	ushort sval;
	Node res;
	int ret;

	if(m == 0)
		error("no map for */@=");

	if(aes.type != TINT)
		error("bad type lhs of */@=");

	expr(rval, &res);

	if(m != cormap && wtflag == 0)
		error("not in write mode");

	r->type = res.type;
	r->store.fmt = res.store.fmt;
	r->store = res.store;

	switch(res.store.fmt) {
	default:
		error("bad pointer format '%c' for */@=", res.store.fmt);
	case 'c':
	case 'C':
	case 'b':
		cval = res.store.u.ival;
		ret = put1(m, aes.store.u.ival, &cval, 1);
		break;
	case 'r':
	case 'x':
	case 'd':
	case 'u':
	case 'o':
		sval = res.store.u.ival;
		ret = put2(m, aes.store.u.ival, sval);
		r->store.u.ival = sval;
		break;
	case 'a':
	case 'A':
	case 'B':
	case 'X':
	case 'D':
	case 'U':
	case 'O':
		ret = put4(m, aes.store.u.ival, res.store.u.ival);
		break;
	case 'V':
	case 'W':
	case 'Y':
	case 'Z':
		ret = put8(m, aes.store.u.ival, res.store.u.ival);
		break;
	case 's':
	case 'R':
		ret = put1(m, aes.store.u.ival, (uchar*)res.store.u.string->string, res.store.u.string->len);
		break;
	}
	if (ret < 0)
		error("windir: %r");
}

void
windirreg(Regs *regs, char *name, Node *rval, Node *r)
{
	Node res;

	if(regs == 0)
		error("no register set for *%s=", name);

	expr(rval, &res);

	r->type = res.type;
	r->store.fmt = res.store.fmt;
	r->store = res.store;

	switch(res.store.fmt){
	default:
		error("bad format '%c' for *%s=", res.store.fmt, name);
	case 'c':
	case 'C':
	case 'b':
	case 'x':
	case 'd':
	case 'u':
	case 'o':
	case 'q':
	case 'r':
	case 'a':
	case 'A':
	case 'B':
	case 'X':
	case 'D':
	case 'U':
	case 'O':
	case 'Q':
	case 'V':
	case 'W':
	case 'Y':
	case 'Z':
		if(rput(regs, name, res.store.u.ival) < 0)
			error("writing %s: %r", name);
		break;
	case 'f':
	case 'g':
	case 'F':
	case '3':
	case '8':
	case 'G':
		error("floating point registers not supported");
		break;
	}
}

void
call(char *fn, Node *parameters, Node *local, Node *body, Node *retexp)
{
	int np, i;
	Rplace rlab;
	Node *n, res;
	Value *v, *f;
	Lsym *s, *next;
	Node *avp[Maxarg], *ava[Maxarg];

	rlab.local = 0;

	na = 0;
	flatten(avp, parameters);
	np = na;
	na = 0;
	flatten(ava, local);
	if(np != na) {
		if(np < na)
			error("%s: too few arguments", fn);
		error("%s: too many arguments", fn);
	}

	rlab.tail = &rlab.local;

	ret = &rlab;
	for(i = 0; i < np; i++) {
		n = ava[i];
		switch(n->op) {
		default:
			error("%s: %d formal not a name", fn, i);
		case ONAME:
			expr(avp[i], &res);
			s = n->sym;
			break;
		case OINDM:
			res.store.u.cc = avp[i];
			res.type = TCODE;
			res.store.comt = 0;
			if(n->left->op != ONAME)
				error("%s: %d formal not a name", fn, i);
			s = n->left->sym;
			break;
		}
		if(s->v->ret == ret)
			error("%s already declared at this scope", s->name);

		v = gmalloc(sizeof(Value));
		v->ret = ret;
		v->pop = s->v;
		s->v = v;
		v->scope = 0;
		*(rlab.tail) = s;
		rlab.tail = &v->scope;

		v->store = res.store;
		v->type = res.type;
		v->set = 1;
	}

	ret->val = retexp;
	if(setjmp(rlab.rlab) == 0)
		execute(body);

	for(s = rlab.local; s; s = next) {
		f = s->v;
		next = f->scope;
		s->v = f->pop;
		free(f);
	}
}
