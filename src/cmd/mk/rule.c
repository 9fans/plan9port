#include	"mk.h"

static Rule *lr, *lmr;
static int rcmp(Rule *r, char *target, Word *tail);
static int nrules = 0;

void
addrule(char *head, Word *tail, char *body, Word *ahead, int attr, int hline, char *prog)
{
	Rule *r;
	Rule *rr;
	Symtab *sym;
	int reuse;

	r = 0;
	reuse = 0;
	if(sym = symlook(head, S_TARGET, 0)){
		for(r = (Rule *)sym->value; r; r = r->chain)
			if(rcmp(r, head, tail) == 0){
				reuse = 1;
				break;
			}
	}
	if(r == 0)
		r = (Rule *)Malloc(sizeof(Rule));
	r->target = head;
	r->tail = tail;
	r->recipe = body;
	r->line = hline;
	r->file = infile;
	r->attr = attr;
	r->alltargets = ahead;
	r->prog = prog;
	r->rule = nrules++;
	if(!reuse){
		rr = (Rule *)symlook(head, S_TARGET, (void *)r)->value;
		if(rr != r){
			r->chain = rr->chain;
			rr->chain = r;
		} else
			r->chain = 0;
	}
	if(!reuse)
		r->next = 0;
	if((attr&REGEXP) || charin(head, "%&")){
		r->attr |= META;
		if(reuse)
			return;
		if(attr&REGEXP){
			patrule = r;
			r->pat = regcomp(head);
		}
		if(metarules == 0)
			metarules = lmr = r;
		else {
			lmr->next = r;
			lmr = r;
		}
	} else {
		if(reuse)
			return;
		r->pat = 0;
		if(rules == 0)
			rules = lr = r;
		else {
			lr->next = r;
			lr = r;
		}
	}
}

void
dumpr(char *s, Rule *r)
{
	Bprint(&bout, "%s: start=%ld\n", s, r);
	for(; r; r = r->next){
		Bprint(&bout, "\tRule %ld: %s[%d] attr=%x next=%ld chain=%ld alltarget='%s'",
			r, r->file, r->line, r->attr, r->next, r->chain, wtos(r->alltargets, ' '));
		if(r->prog)
			Bprint(&bout, " prog='%s'", r->prog);
		Bprint(&bout, "\n\ttarget=%s: %s\n", r->target, wtos(r->tail, ' '));
		Bprint(&bout, "\trecipe@%ld='%s'\n", r->recipe, r->recipe);
	}
}

static int
rcmp(Rule *r, char *target, Word *tail)
{
	Word *w;

	if(strcmp(r->target, target))
		return 1;
	for(w = r->tail; w && tail; w = w->next, tail = tail->next)
		if(strcmp(w->s, tail->s))
			return 1;
	return(w || tail);
}

char *
rulecnt(void)
{
	char *s;

	s = Malloc(nrules);
	memset(s, 0, nrules);
	return(s);
}
