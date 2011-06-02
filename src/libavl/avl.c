#include <u.h>
#include <libc.h>
#include <bio.h>
#include <avl.h>

/*
 * In-memory database stored as self-balancing AVL tree.
 * See Lewis & Denenberg, Data Structures and Their Algorithms.
 */

static void
singleleft(Avl **tp, Avl *p)
{
	int l, r2;
	Avl *a, *c;

	a = *tp;
	c = a->n[1];

	r2 = c->bal;
	l = (r2 > 0? r2: 0)+1 - a->bal;

	if((a->n[1] = c->n[0]) != nil)
		a->n[1]->p = a;

	if((c->n[0] = a) != nil)
		c->n[0]->p = c;

	if((*tp = c) != nil)
		(*tp)->p = p;

	a->bal = -l;
	c->bal = r2 - ((l > 0? l: 0)+1);

}

static void
singleright(Avl **tp, Avl *p)
{
	int l2, r;
	Avl *a, *c;

	a = *tp;
	c = a->n[0];
	l2 = - c->bal;
	r = a->bal + ((l2 > 0? l2: 0)+1);

	if((a->n[0] = c->n[1]) != nil)
		a->n[0]->p = a;

	if((c->n[1] = a) != nil)
		c->n[1]->p = c;

	if((*tp = c) != nil)
		(*tp)->p = p;

	a->bal = r;
	c->bal = ((r > 0? r: 0)+1) - l2;
}

static void
doublerightleft(Avl **tp, Avl *p)
{
	singleright(&(*tp)->n[1], *tp);
	singleleft(tp, p);
}

static void
doubleleftright(Avl **tp, Avl *p)
{
	singleleft(&(*tp)->n[0], *tp);
	singleright(tp, p);
}

static void
balance(Avl **tp, Avl *p)
{
	switch((*tp)->bal){
	case -2:
		if((*tp)->n[0]->bal <= 0)
			singleright(tp, p);
		else if((*tp)->n[0]->bal == 1)
			doubleleftright(tp, p);
		else
			assert(0);
		break;

	case 2:
		if((*tp)->n[1]->bal >= 0)
			singleleft(tp, p);
		else if((*tp)->n[1]->bal == -1)
			doublerightleft(tp, p);
		else
			assert(0);
		break;
	}
}

static int
canoncmp(int cmp)
{
	if(cmp < 0)
		return -1;
	else if(cmp > 0)
		return 1;
	return 0;
}

static int
_insertavl(Avl **tp, Avl *p, Avl *r, int (*cmp)(Avl*,Avl*), Avl **rfree)
{
	int i, ob;

	if(*tp == nil){
		r->bal = 0;
		r->n[0] = nil;
		r->n[1] = nil;
		r->p = p;
		*tp = r;
		return 1;
	}
	ob = (*tp)->bal;
	if((i = canoncmp(cmp(r, *tp))) != 0){
		(*tp)->bal += i * _insertavl(&(*tp)->n[(i+1)/2], *tp, r, cmp,
			rfree);
		balance(tp, p);
		return ob == 0 && (*tp)->bal != 0;
	}

	/* install new entry */
	*rfree = *tp;		/* save old node for freeing */
	*tp = r;		/* insert new node */
	**tp = **rfree;		/* copy old node's Avl contents */
	if(r->n[0])		/* fix node's children's parent pointers */
		r->n[0]->p = r;
	if(r->n[1])
		r->n[1]->p = r;

	return 0;
}

static int
successor(Avl **tp, Avl *p, Avl **r)
{
	int ob;

	if((*tp)->n[0] == nil){
		*r = *tp;
		*tp = (*r)->n[1];
		if(*tp)
			(*tp)->p = p;
		return -1;
	}
	ob = (*tp)->bal;
	(*tp)->bal -= successor(&(*tp)->n[0], *tp, r);
	balance(tp, p);
	return -(ob != 0 && (*tp)->bal == 0);
}

static int
_deleteavl(Avl **tp, Avl *p, Avl *rx, int(*cmp)(Avl*,Avl*), Avl **del,
	void (*predel)(Avl*, void*), void *arg)
{
	int i, ob;
	Avl *r, *or;

	if(*tp == nil)
		return 0;

	ob = (*tp)->bal;
	if((i=canoncmp(cmp(rx, *tp))) != 0){
		(*tp)->bal += i * _deleteavl(&(*tp)->n[(i+1)/2], *tp, rx, cmp,
			del, predel, arg);
		balance(tp, p);
		return -(ob != 0 && (*tp)->bal == 0);
	}

	if(predel)
		(*predel)(*tp, arg);

	or = *tp;
	if(or->n[i=0] == nil || or->n[i=1] == nil){
		*tp = or->n[1-i];
		if(*tp)
			(*tp)->p = p;
		*del = or;
		return -1;
	}

	/* deleting node with two kids, find successor */
	or->bal += successor(&or->n[1], or, &r);
	r->bal = or->bal;
	r->n[0] = or->n[0];
	r->n[1] = or->n[1];
	*tp = r;
	(*tp)->p = p;
	/* node has changed; fix children's parent pointers */
	if(r->n[0])
		r->n[0]->p = r;
	if(r->n[1])
		r->n[1]->p = r;
	*del = or;
	balance(tp, p);
	return -(ob != 0 && (*tp)->bal == 0);
}

/*
static void
checkparents(Avl *a, Avl *p)
{
	if(a == nil)
		return;
	if(a->p != p)
		print("bad parent\n");
	checkparents(a->n[0], a);
	checkparents(a->n[1], a);
}
*/

struct Avltree
{
	Avl	*root;
	int	(*cmp)(Avl*, Avl*);
	Avlwalk	*walks;
};
struct Avlwalk
{
	int	started;
	int	moved;
	Avlwalk	*next;
	Avltree	*tree;
	Avl	*node;
};

Avltree*
mkavltree(int (*cmp)(Avl*, Avl*))
{
	Avltree *t;

	t = malloc(sizeof *t);
	if(t == nil)
		return nil;
	memset(t, 0, sizeof *t);
	t->cmp = cmp;
	return t;
}

void
insertavl(Avltree *t, Avl *new, Avl **oldp)
{
	*oldp = nil;
	_insertavl(&t->root, nil, new, t->cmp, oldp);
}

static Avl*
findpredecessor(Avl *a)
{
	if(a == nil)
		return nil;

	if(a->n[0] != nil){
		/* predecessor is rightmost descendant of left child */
		for(a = a->n[0]; a->n[1]; a = a->n[1])
			;
		return a;
	}else{
		/* we're at a leaf, successor is a parent we enter from the right */
		while(a->p && a->p->n[0] == a)
			a = a->p;
		return a->p;
	}
}

static Avl*
findsuccessor(Avl *a)
{
	if(a == nil)
		return nil;

	if(a->n[1] != nil){
		/* successor is leftmost descendant of right child */
		for(a = a->n[1]; a->n[0]; a = a->n[0])
			;
		return a;
	}else{
		/* we're at a leaf, successor is a parent we enter from the left going up */
		while(a->p && a->p->n[1] == a)
			a = a->p;
		return a->p;
	}
}

static Avl*
_lookupavl(Avl *t, Avl *r, int (*cmp)(Avl*,Avl*), int neighbor)
{
	int i;
	Avl *p;

	p = nil;
	if(t == nil)
		return nil;
	do{
		assert(t->p == p);
		if((i = canoncmp(cmp(r, t))) == 0)
			return t;
		p = t;
		t = t->n[(i+1)/2];
	}while(t);
	if(neighbor == 0)
		return nil;
	if(neighbor < 0)
		return i > 0 ? p : findpredecessor(p);
	return i < 0 ? p : findsuccessor(p);
}

Avl*
searchavl(Avltree *t, Avl *key, int neighbor)
{
	return _lookupavl(t->root, key, t->cmp, neighbor);
}

Avl*
lookupavl(Avltree *t, Avl *key)
{
	return _lookupavl(t->root, key, t->cmp, 0);
}

static void
walkdel(Avl *a, void *v)
{
	Avl *p;
	Avlwalk *w;
	Avltree *t;

	if(a == nil)
		return;

	p = findpredecessor(a);
	t = v;
	for(w = t->walks; w; w = w->next){
		if(w->node == a){
			/* back pointer to predecessor; not perfect but adequate */
			w->moved = 1;
			w->node = p;
			if(p == nil)
				w->started = 0;
		}
	}
}

void
deleteavl(Avltree *t, Avl *key, Avl **oldp)
{
	*oldp = nil;
	_deleteavl(&t->root, nil, key, t->cmp, oldp, walkdel, t);
}

Avlwalk*
avlwalk(Avltree *t)
{
	Avlwalk *w;

	w = malloc(sizeof *w);
	if(w == nil)
		return nil;
	memset(w, 0, sizeof *w);
	w->tree = t;
	w->next = t->walks;
	t->walks = w;
	return w;
}

Avl*
avlnext(Avlwalk *w)
{
	Avl *a;

	if(w->started==0){
		for(a = w->tree->root; a && a->n[0]; a = a->n[0])
			;
		w->node = a;
		w->started = 1;
	}else{
		a = findsuccessor(w->node);
		if(a == w->node)
			abort();
		w->node = a;
	}
	return w->node;
}

Avl*
avlprev(Avlwalk *w)
{
	Avl *a;

	if(w->started == 0){
		for(a = w->tree->root; a && a->n[1]; a = a->n[1])
			;
		w->node = a;
		w->started = 1;
	}else if(w->moved){
		w->moved = 0;
		return w->node;
	}else{
		a = findpredecessor(w->node);
		if(a == w->node)
			abort();
		w->node = a;
	}
	return w->node;
}

void
endwalk(Avlwalk *w)
{
	Avltree *t;
	Avlwalk **l;

	t = w->tree;
	for(l = &t->walks; *l; l = &(*l)->next){
		if(*l == w){
			*l = w->next;
			break;
		}
	}
	free(w);
}

/*
static void
walkavl(Avl *t, void (*f)(Avl*, void*), void *v)
{
	if(t == nil)
		return;
	walkavl(t->n[0], f, v);
	f(t, v);
	walkavl(t->n[1], f, v);
}
*/
