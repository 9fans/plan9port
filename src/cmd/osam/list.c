#include "sam.h"

/*
 * Check that list has room for one more element.
 */
static void
growlist(List *l, int esize)
{
	uchar *p;

	if(l->listptr == nil || l->nalloc == 0){
		l->nalloc = INCR;
		l->listptr = emalloc(INCR*esize);
		l->nused = 0;
	}
	else if(l->nused == l->nalloc){
		p = erealloc(l->listptr, (l->nalloc+INCR)*esize);
		l->listptr = p;
		memset(p+l->nalloc*esize, 0, INCR*esize);
		l->nalloc += INCR;
	}
}

/*
 * Remove the ith element from the list
 */
void
dellist(List *l, int i)
{
	Posn *pp;
	void **vpp;

	l->nused--;

	switch(l->type){
	case 'P':
		pp = l->posnptr+i;
		memmove(pp, pp+1, (l->nused-i)*sizeof(*pp));
		break;
	case 'p':
		vpp = l->voidpptr+i;
		memmove(vpp, vpp+1, (l->nused-i)*sizeof(*vpp));
		break;
	}
}

/*
 * Add a new element, whose position is i, to the list
 */
void
inslist(List *l, int i, ...)
{
	Posn *pp;
	void **vpp;
	va_list list;


	va_start(list, i);
	switch(l->type){
	case 'P':
		growlist(l, sizeof(*pp));
		pp = l->posnptr+i;
		memmove(pp+1, pp, (l->nused-i)*sizeof(*pp));
		*pp = va_arg(list, Posn);
		break;
	case 'p':
		growlist(l, sizeof(*vpp));
		vpp = l->voidpptr+i;
		memmove(vpp+1, vpp, (l->nused-i)*sizeof(*vpp));
		*vpp = va_arg(list, void*);
		break;
	}
	va_end(list);

	l->nused++;
}

void
listfree(List *l)
{
	free(l->listptr);
	free(l);
}

List*
listalloc(int type)
{
	List *l;

	l = emalloc(sizeof(List));
	l->type = type;
	l->nalloc = 0;
	l->nused = 0;

	return l;
}
