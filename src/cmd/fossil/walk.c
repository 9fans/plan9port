/*
 * Generic traversal routines.
 */

#include "stdinc.h"
#include "dat.h"
#include "fns.h"

static uint
etype(Entry *e)
{
	uint t;

	if(e->flags&_VtEntryDir)
		t = BtDir;
	else
		t = BtData;
	return t+e->depth;
}

void
initWalk(WalkPtr *w, Block *b, uint size)
{
	memset(w, 0, sizeof *w);
	switch(b->l.type){
	case BtData:
		return;

	case BtDir:
		w->data = b->data;
		w->m = size / VtEntrySize;
		w->isEntry = 1;
		return;

	default:
		w->data = b->data;
		w->m = size / VtScoreSize;
		w->type = b->l.type;
		w->tag = b->l.tag;
		return;
	}
}

int
nextWalk(WalkPtr *w, uchar score[VtScoreSize], uchar *type, u32int *tag, Entry **e)
{
	if(w->n >= w->m)
		return 0;

	if(w->isEntry){
		*e = &w->e;
		entryUnpack(&w->e, w->data, w->n);
		memmove(score, w->e.score, VtScoreSize);
		*type = etype(&w->e);
		*tag = w->e.tag;
	}else{
		*e = nil;
		memmove(score, w->data+w->n*VtScoreSize, VtScoreSize);
		*type = w->type-1;
		*tag = w->tag;
	}
	w->n++;
	return 1;
}
