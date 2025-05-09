#include "rc.h"
#include "exec.h"
#include "fns.h"

var *gvar[NVAR];

int
hash(char *s, int n)
{
	int h = 0, i = 1;
	while(*s) h+=*s++*i++;
	h%=n;
	return h<0?h+n:h;
}
#define	NKW	30
struct kw{
	char *name;
	int type;
	struct kw *next;
}*kw[NKW];

void
kenter(int type, char *name)
{
	int h = hash(name, NKW);
	struct kw *p = new(struct kw);
	p->type = type;
	p->name = name;
	p->next = kw[h];
	kw[h] = p;
}

void
kinit(void)
{
	kenter(FOR, "for");
	kenter(IN, "in");
	kenter(WHILE, "while");
	kenter(IF, "if");
	kenter(NOT, "not");
	kenter(TWIDDLE, "~");
	kenter(BANG, "!");
	kenter(SUBSHELL, "@");
	kenter(SWITCH, "switch");
	kenter(FN, "fn");
}

tree*
klook(char *name)
{
	struct kw *p;
	tree *t = token(name, WORD);
	for(p = kw[hash(name, NKW)];p;p = p->next)
		if(strcmp(p->name, name)==0){
			t->type = p->type;
			t->iskw = 1;
			break;
		}
	return t;
}

var*
newvar(char *name, var *next)
{
	int n = strlen(name)+1;
	var *v = emalloc(sizeof(var)+n);
	memmove(v->name, name, n);
	v->next = next;
	v->val = 0;
	v->fn = 0;
	v->changed = 0;
	v->fnchanged = 0;
	return v;
}

var*
gvlook(char *name)
{
	int h = hash(name, NVAR);
	var *v;
	for(v = gvar[h];v;v = v->next) if(strcmp(v->name, name)==0) return v;
	return gvar[h] = newvar(name, gvar[h]);
}

var*
vlook(char *name)
{
	var *v;
	if(runq)
		for(v = runq->local;v;v = v->next)
			if(strcmp(v->name, name)==0) return v;
	return gvlook(name);
}

void
setvar(char *name, word *val)
{
	var *v = vlook(name);
	freewords(v->val);
	v->val = val;
	v->changed = 1;
}

void
freevar(var *v)
{
	freewords(v->val);
	free(v);
}
