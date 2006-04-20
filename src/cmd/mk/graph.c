#include	"mk.h"

static Node *applyrules(char *, char *);
static void togo(Node *);
static int vacuous(Node *);
static Node *newnode(char *);
static void trace(char *, Arc *);
static void cyclechk(Node *);
static void ambiguous(Node *);
static void attribute(Node *);

Node *
graph(char *target)
{
	Node *node;
	char *cnt;

	cnt = rulecnt();
	node = applyrules(target, cnt);
	free(cnt);
	cyclechk(node);
	node->flags |= PROBABLE;	/* make sure it doesn't get deleted */
	vacuous(node);
	ambiguous(node);
	attribute(node);
	return(node);
}

static Node *
applyrules(char *target, char *cnt)
{
	Symtab *sym;
	Node *node;
	Rule *r;
	Arc head, *a = &head;
	Word *w;
	char stem[NAMEBLOCK], buf[NAMEBLOCK];
	Resub rmatch[NREGEXP];

/*	print("applyrules(%lux='%s')\n", target, target); */
	sym = symlook(target, S_NODE, 0);
	if(sym)
		return sym->u.ptr;
	target = strdup(target);
	node = newnode(target);
	head.n = 0;
	head.next = 0;
	sym = symlook(target, S_TARGET, 0);
	memset((char*)rmatch, 0, sizeof(rmatch));
	for(r = sym? sym->u.ptr:0; r; r = r->chain){
		if(r->attr&META) continue;
		if(strcmp(target, r->target)) continue;
		if((!r->recipe || !*r->recipe) && (!r->tail || !r->tail->s || !*r->tail->s)) continue;	/* no effect; ignore */
		if(cnt[r->rule] >= nreps) continue;
		cnt[r->rule]++;
		node->flags |= PROBABLE;

/*		if(r->attr&VIR)
 *			node->flags |= VIRTUAL;
 *		if(r->attr&NOREC)
 *			node->flags |= NORECIPE;
 *		if(r->attr&DEL)
 *			node->flags |= DELETE;
 */
		if(!r->tail || !r->tail->s || !*r->tail->s) {
			a->next = newarc((Node *)0, r, "", rmatch);
			a = a->next;
		} else
			for(w = r->tail; w; w = w->next){
				a->next = newarc(applyrules(w->s, cnt), r, "", rmatch);
				a = a->next;
		}
		cnt[r->rule]--;
		head.n = node;
	}
	for(r = metarules; r; r = r->next){
		if((!r->recipe || !*r->recipe) && (!r->tail || !r->tail->s || !*r->tail->s)) continue;	/* no effect; ignore */
		if ((r->attr&NOVIRT) && a != &head && (a->r->attr&VIR))
			continue;
		if(r->attr&REGEXP){
			stem[0] = 0;
			patrule = r;
			memset((char*)rmatch, 0, sizeof(rmatch));
			if(regexec(r->pat, node->name, rmatch, NREGEXP) == 0)
				continue;
		} else {
			if(!match(node->name, r->target, stem, r->shellt)) continue;
		}
		if(cnt[r->rule] >= nreps) continue;
		cnt[r->rule]++;

/*		if(r->attr&VIR)
 *			node->flags |= VIRTUAL;
 *		if(r->attr&NOREC)
 *			node->flags |= NORECIPE;
 *		if(r->attr&DEL)
 *			node->flags |= DELETE;
 */

		if(!r->tail || !r->tail->s || !*r->tail->s) {
			a->next = newarc((Node *)0, r, stem, rmatch);
			a = a->next;
		} else
			for(w = r->tail; w; w = w->next){
				if(r->attr&REGEXP)
					regsub(w->s, buf, sizeof buf, rmatch, NREGEXP);
				else
					subst(stem, w->s, buf);
				a->next = newarc(applyrules(buf, cnt), r, stem, rmatch);
				a = a->next;
			}
		cnt[r->rule]--;
	}
	a->next = node->prereqs;
	node->prereqs = head.next;
	return(node);
}

static void
togo(Node *node)
{
	Arc *la, *a;

	/* delete them now */
	la = 0;
	for(a = node->prereqs; a; la = a, a = a->next)
		if(a->flag&TOGO){
			if(a == node->prereqs)
				node->prereqs = a->next;
			else
				la->next = a->next, a = la;
		}
}

static int
vacuous(Node *node)
{
	Arc *la, *a;
	int vac = !(node->flags&PROBABLE);

	if(node->flags&READY)
		return(node->flags&VACUOUS);
	node->flags |= READY;
	for(a = node->prereqs; a; a = a->next)
		if(a->n && vacuous(a->n) && (a->r->attr&META))
			a->flag |= TOGO;
		else
			vac = 0;
	/* if a rule generated arcs that DON'T go; no others from that rule go */
	for(a = node->prereqs; a; a = a->next)
		if((a->flag&TOGO) == 0)
			for(la = node->prereqs; la; la = la->next)
				if((la->flag&TOGO) && (la->r == a->r)){
					la->flag &= ~TOGO;
				}
	togo(node);
	if(vac)
		node->flags |= VACUOUS;
	return(vac);
}

static Node *
newnode(char *name)
{
	register Node *node;

	node = (Node *)Malloc(sizeof(Node));
	symlook(name, S_NODE, (void *)node);
	node->name = name;
	node->time = timeof(name, 0);
	node->prereqs = 0;
	node->flags = node->time? PROBABLE : 0;
	node->next = 0;
	return(node);
}

void
dumpn(char *s, Node *n)
{
	char buf[1024];
	Arc *a;

	snprint(buf, sizeof buf, "%s   ", (*s == ' ')? s:"");
	Bprint(&bout, "%s%s@%ld: time=%ld flags=0x%x next=%ld\n",
		s, n->name, n, n->time, n->flags, n->next);
	for(a = n->prereqs; a; a = a->next)
		dumpa(buf, a);
}

static void
trace(char *s, Arc *a)
{
	fprint(2, "\t%s", s);
	while(a){
		fprint(2, " <-(%s:%d)- %s", a->r->file, a->r->line,
			a->n? a->n->name:"");
		if(a->n){
			for(a = a->n->prereqs; a; a = a->next)
				if(*a->r->recipe) break;
		} else
			a = 0;
	}
	fprint(2, "\n");
}

static void
cyclechk(Node *n)
{
	Arc *a;

	if((n->flags&CYCLE) && n->prereqs){
		fprint(2, "mk: cycle in graph detected at target %s\n", n->name);
		Exit();
	}
	n->flags |= CYCLE;
	for(a = n->prereqs; a; a = a->next)
		if(a->n)
			cyclechk(a->n);
	n->flags &= ~CYCLE;
}

static void
ambiguous(Node *n)
{
	Arc *a;
	Rule *r = 0;
	Arc *la;
	int bad = 0;

	la = 0;
	for(a = n->prereqs; a; a = a->next){
		if(a->n)
			ambiguous(a->n);
		if(*a->r->recipe == 0) continue;
		if(r == 0)
			r = a->r, la = a;
		else{
			if(r->recipe != a->r->recipe){
				if((r->attr&META) && !(a->r->attr&META)){
					la->flag |= TOGO;
					r = a->r, la = a;
				} else if(!(r->attr&META) && (a->r->attr&META)){
					a->flag |= TOGO;
					continue;
				}
			}
			if(r->recipe != a->r->recipe){
				if(bad == 0){
					fprint(2, "mk: ambiguous recipes for %s:\n", n->name);
					bad = 1;
					trace(n->name, la);
				}
				trace(n->name, a);
			}
		}
	}
	if(bad)
		Exit();
	togo(n);
}

static void
attribute(Node *n)
{
	register Arc *a;

	for(a = n->prereqs; a; a = a->next){
		if(a->r->attr&VIR)
			n->flags |= VIRTUAL;
		if(a->r->attr&NOREC)
			n->flags |= NORECIPE;
		if(a->r->attr&DEL)
			n->flags |= DELETE;
		if(a->n)
			attribute(a->n);
	}
	if(n->flags&VIRTUAL)
		n->time = 0;
}
