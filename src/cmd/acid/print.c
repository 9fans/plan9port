#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include <mach.h>
#define Extern extern
#include "acid.h"

static char *binop[NUMO];

static void
initbinop(void)
{
	binop[OMUL]=	"*";
	binop[ODIV]=	"/";
	binop[OMOD]=	"%";
	binop[OADD]=	"+";
	binop[OSUB]=	"-";
	binop[ORSH]=	">>";
	binop[OLSH]=	"<<";
	binop[OLT]=	"<";
	binop[OGT]=	">";
	binop[OLEQ]=	"<=";
	binop[OGEQ]=	">=";
	binop[OEQ]=	"==";
	binop[ONEQ]=	"!=";
	binop[OLAND]=	"&";
	binop[OXOR]=	"^";
	binop[OLOR]=	"|";
	binop[OCAND]=	"&&";
	binop[OCOR]=	"||";
	binop[OASGN]=	" = ";
}

static char *tabs = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
char *typenames[] = {
	"integer",
	"float",
	"string",
	"list",
	"code"
};

void
initprint(void)
{
	initbinop();
}

int
cmp(const void *va, const void *vb)
{
	char **a = (char**)va;
	char **b = (char**)vb;

	return strcmp(*a, *b);
}

void
fundefs(void)
{
	Lsym *l;
	char **vec;
	int i, j, n, max, col, f, g, s;

	max = 0;
	f = 0;
	g = 100;
	vec = malloc(sizeof(char*)*g);
	if(vec == 0)
		fatal("out of memory");

	for(i = 0; i < Hashsize; i++) {
		for(l = hash[i]; l; l = l->hash) {
			if(l->proc == 0 && l->builtin == 0)
				continue;
			n = strlen(l->name);
			if(n > max)
				max = n;
			if(f >= g) {
				g *= 2;
				vec = realloc(vec, sizeof(char*)*g);
				if(vec == 0)
					fatal("out of memory");
			}
			vec[f++] = l->name;
		}
	}
        qsort(vec, f, sizeof(char*), cmp);
	max++;
	col = 60/max;
	s = (f+col-1)/col;

	for(i = 0; i < s; i++) {
		for(j = i; j < f; j += s)
			Bprint(bout, "%-*s", max, vec[j]);
		Bprint(bout, "\n");
	}
}

void
whatis(Lsym *l)
{
	int t;
	int def;
	Type *ti;

	if(l == 0) {
		fundefs();
		return;
	}

	def = 0;
	if(l->v->set) {
		t = l->v->type;
		Bprint(bout, "%s variable", typenames[t]);
		if(t == TINT || t == TFLOAT)
			Bprint(bout, " format %c", l->v->store.fmt);
		if(l->v->store.comt)
			Bprint(bout, " complex %s",
						l->v->store.comt->base->name);
		Bputc(bout, '\n');
		def = 1;
	}
	if(l->lt) {
		Bprint(bout, "complex %s {\n", l->name);
		for(ti = l->lt; ti; ti = ti->next) {
			if(ti->type) {
				if(ti->fmt == 'a') {
					Bprint(bout, "\t%s %d %s;\n",
					ti->type->name, ti->offset,
					ti->tag->name);
				}
				else {
					Bprint(bout, "\t'%c' %s %d %s;\n",
					ti->fmt, ti->type->name, ti->offset,
					ti->tag->name);
				}
			}
			else
				Bprint(bout, "\t'%c' %d %s;\n",
				ti->fmt, ti->offset, ti->tag->name);
		}
		Bprint(bout, "};\n");
		def = 1;
	}
	if(l->proc) {
		Bprint(bout, "defn %s(", l->name);
		pexpr(l->proc->left);
		Bprint(bout, ") {\n");
		pcode(l->proc->right, 1);
		Bprint(bout, "}\n");
		def = 1;
	}
	if(l->builtin) {
		Bprint(bout, "builtin function\n");
		def = 1;
	}
	if(def == 0)
		Bprint(bout, "%s is undefined\n", l->name);
}

void
slist(Node *n, int d)
{
	if(n == 0)
		return;
	if(n->op == OLIST)
		Bprint(bout, "%.*s{\n", d-1, tabs);
	pcode(n, d);
	if(n->op == OLIST)
		Bprint(bout, "%.*s}\n", d-1, tabs);
}

void
pcode(Node *n, int d)
{
	Node *r, *l;

	if(n == 0)
		return;

	r = n->right;
	l = n->left;

	switch(n->op) {
	default:
		Bprint(bout, "%.*s", d, tabs);
		pexpr(n);
		Bprint(bout, ";\n");
		break;
	case OLIST:
		pcode(n->left, d);
		pcode(n->right, d);
		break;
	case OLOCAL:
		Bprint(bout, "%.*slocal", d, tabs);
		while(l) {
			Bprint(bout, " %s", l->sym->name);
			l = l->left;
			if(l == 0)
				Bprint(bout, ";\n");
			else
				Bprint(bout, ",");
		}
		break;
	case OCOMPLEX:
		Bprint(bout, "%.*scomplex %s %s;\n", d, tabs, n->sym->name, l->sym->name);
		break;
	case OIF:
		Bprint(bout, "%.*sif ", d, tabs);
		pexpr(l);
		d++;
		Bprint(bout, " then\n");
		if(r && r->op == OELSE) {
			slist(r->left, d);
			Bprint(bout, "%.*selse\n", d-1, tabs);
			slist(r->right, d);
		}
		else
			slist(r, d);
		break;
	case OWHILE:
		Bprint(bout, "%.*swhile ", d, tabs);
		pexpr(l);
		d++;
		Bprint(bout, " do\n");
		slist(r, d);
		break;
	case ORET:
		Bprint(bout, "%.*sreturn ", d, tabs);
		pexpr(l);
		Bprint(bout, ";\n");
		break;
	case ODO:
		Bprint(bout, "%.*sloop ", d, tabs);
		pexpr(l->left);
		Bprint(bout, ", ");
		pexpr(l->right);
		Bprint(bout, " do\n");
		slist(r, d+1);
	}
}

void
pexpr(Node *n)
{
	Node *r, *l;

	if(n == 0)
		return;

	r = n->right;
	l = n->left;

	switch(n->op) {
	case ONAME:
		Bprint(bout, "%s", n->sym->name);
		break;
	case OCONST:
		switch(n->type) {
		case TINT:
			Bprint(bout, "%d", (int)n->store.u.ival);
			break;
		case TFLOAT:
			Bprint(bout, "%g", n->store.u.fval);
			break;
		case TSTRING:
			pstr(n->store.u.string);
			break;
		case TLIST:
			break;
		}
		break;
	case OMUL:
	case ODIV:
	case OMOD:
	case OADD:
	case OSUB:
	case ORSH:
	case OLSH:
	case OLT:
	case OGT:
	case OLEQ:
	case OGEQ:
	case OEQ:
	case ONEQ:
	case OLAND:
	case OXOR:
	case OLOR:
	case OCAND:
	case OCOR:
		Bputc(bout, '(');
		pexpr(l);
		Bprint(bout, binop[(uchar)n->op]);
		pexpr(r);
		Bputc(bout, ')');
		break;
	case OASGN:
		pexpr(l);
		Bprint(bout, binop[(uchar)n->op]);
		pexpr(r);
		break;
	case OINDM:
		Bprint(bout, "*");
		pexpr(l);
		break;
	case OEDEC:
		Bprint(bout, "--");
		pexpr(l);
		break;
	case OEINC:
		Bprint(bout, "++");
		pexpr(l);
		break;
	case OPINC:
		pexpr(l);
		Bprint(bout, "++");
		break;
	case OPDEC:
		pexpr(l);
		Bprint(bout, "--");
		break;
	case ONOT:
		Bprint(bout, "!");
		pexpr(l);
		break;
	case OLIST:
		pexpr(l);
		if(r) {
			Bprint(bout, ",");
			pexpr(r);
		}
		break;
	case OCALL:
		pexpr(l);
		Bprint(bout, "(");
		pexpr(r);
		Bprint(bout, ")");
		break;
	case OCTRUCT:
		Bprint(bout, "{");
		pexpr(l);
		Bprint(bout, "}");
		break;
	case OHEAD:
		Bprint(bout, "head ");
		pexpr(l);
		break;
	case OTAIL:
		Bprint(bout, "tail ");
		pexpr(l);
		break;
	case OAPPEND:
		Bprint(bout, "append ");
		pexpr(l);
		Bprint(bout, ",");
		pexpr(r);
		break;
	case ODELETE:
		Bprint(bout, "delete ");
		pexpr(l);
		Bprint(bout, ",");
		pexpr(r);
		break;
	case ORET:
		Bprint(bout, "return ");
		pexpr(l);
		break;
	case OINDEX:
		pexpr(l);
		Bprint(bout, "[");
		pexpr(r);
		Bprint(bout, "]");
		break;
	case OINDC:
		Bprint(bout, "@");
		pexpr(l);
		break;
	case ODOT:
		pexpr(l);
		Bprint(bout, ".%s", n->sym->name);
		break;
	case OFRAME:
		Bprint(bout, "%s:%s", n->sym->name, l->sym->name);
		break;
	case OCAST:
		Bprint(bout, "(%s)", n->sym->name);
		pexpr(l);
		break;
	case OFMT:
		pexpr(l);
		Bprint(bout, "\\%c", (int)r->store.u.ival);
		break;
	case OEVAL:
		Bprint(bout, "eval ");
		pexpr(l);
		break;
	case OWHAT:
		Bprint(bout, "whatis");
		if(n->sym)
			Bprint(bout, " %s", n->sym->name);
		break;
	case OUPLUS:
		Bprint(bout, "+");
		pexpr(l);
		break;
	}
}

void
pstr(String *s)
{
	int i, c;

	Bputc(bout, '"');
	for(i = 0; i < s->len; i++) {
		c = s->string[i];
		switch(c) {
		case '\0':
			c = '0';
			break;
		case '\n':
			c = 'n';
			break;
		case '\r':
			c = 'r';
			break;
		case '\t':
			c = 't';
			break;
		case '\b':
			c = 'b';
			break;
		case '\f':
			c = 'f';
			break;
		case '\a':
			c = 'a';
			break;
		case '\v':
			c = 'v';
			break;
		case '\\':
			c = '\\';
			break;
		case '"':
			c = '"';
			break;
		default:
			Bputc(bout, c);
			continue;
		}
		Bputc(bout, '\\');
		Bputc(bout, c);
	}
	Bputc(bout, '"');
}
