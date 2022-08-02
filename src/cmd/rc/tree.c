#include "rc.h"
#include "io.h"
#include "fns.h"

/*
 * create and clear a new tree node, and add it
 * to the node list.
 */
static tree *treefree, *treenodes;

tree*
newtree(void)
{
	tree *t;

	t = treefree;
	if(t==0)
		t = new(tree);
	else
		treefree = t->next;
	t->quoted = 0;
	t->glob = 0;
	t->iskw = 0;
	t->str = 0;
	t->child[0] = t->child[1] = t->child[2] = 0;
	t->line = lex->line;
	t->next = treenodes;
	treenodes = t;
	return t;
}

void
freenodes(void)
{
	tree *t;

	t = treenodes;
	while(t){
		if(t->str){
			free(t->str);
			t->str = 0;
		}
		t->child[0] = t->child[1] = t->child[2] = 0;
		if(t->next==0){
			t->next = treefree;
			treefree = treenodes;
			break;
		}
		t = t->next;
	}
	treenodes = 0;
}

tree*
tree1(int type, tree *c0)
{
	return tree3(type, c0, (tree *)0, (tree *)0);
}

tree*
tree2(int type, tree *c0, tree *c1)
{
	return tree3(type, c0, c1, (tree *)0);
}

tree*
tree3(int type, tree *c0, tree *c1, tree *c2)
{
	tree *t;
	if(type==';'){
		if(c0==0)
			return c1;
		if(c1==0)
			return c0;
	}
	t = newtree();
	t->type = type;
	t->child[0] = c0;
	t->child[1] = c1;
	t->child[2] = c2;

	if(c0)
		t->line = c0->line;
	else if(c1)
		t->line = c1->line;
	else if(c2)
		t->line = c2->line;
	return t;
}

tree*
mung1(tree *t, tree *c0)
{
	t->child[0] = c0;
	return t;
}

tree*
mung2(tree *t, tree *c0, tree *c1)
{
	t->child[0] = c0;
	t->child[1] = c1;
	return t;
}

tree*
mung3(tree *t, tree *c0, tree *c1, tree *c2)
{
	t->child[0] = c0;
	t->child[1] = c1;
	t->child[2] = c2;
	return t;
}

tree*
epimung(tree *comp, tree *epi)
{
	tree *p;
	if(epi==0)
		return comp;
	for(p = epi;p->child[1];p = p->child[1]);
	p->child[1] = comp;
	return epi;
}

/*
 * Add a SIMPLE node at the root of t and percolate all the redirections
 * up to the root.
 */
tree*
simplemung(tree *t)
{
	tree *u;

	t = tree1(SIMPLE, t);
	t->str = fnstr(t);
	for(u = t->child[0];u->type==ARGLIST;u = u->child[0]){
		if(u->child[1]->type==DUP
		|| u->child[1]->type==REDIR){
			u->child[1]->child[1] = t;
			t = u->child[1];
			u->child[1] = 0;
		}
	}
	return t;
}

char*
fnstr(tree *t)
{
	io *f = openiostr();
	pfmt(f, "%t", t);
	return closeiostr(f);
}

tree*
globprop(tree *t)
{
	tree *c0 = t->child[0];
	tree *c1 = t->child[1];
	if(c1==0){
		while(c0 && c0->type==WORDS){
			c1 = c0->child[1];
			if(c1 && c1->glob){
				c1->glob=2;
				t->glob=1;
			}
			c0 = c0->child[0];
		}
	} else {
		if(c0->glob){
			c0->glob=2;
			t->glob=1;
		}
		if(c1->glob){
			c1->glob=2;
			t->glob=1;
		}
	}
	return t;
}

tree*
token(char *str, int type)
{
	tree *t = newtree();
	t->str = estrdup(str);
	t->type = type;
	return t;
}
