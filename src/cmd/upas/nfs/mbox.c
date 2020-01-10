#include "a.h"

Mailbox *hash[123];
Mailbox **box;
uint nbox;

static void
markboxes(int mark)
{
	Mailbox *b;

	for(i=0; i<nbox; i++)
		if(box[i])
			box[i]->mark = mark;
}

static void
sweepboxes(void)
{
	Mailbox *b;

	for(i=0; i<nbox; i++)
		if(box[i] && box[i]->mark){
			freembox(box[i]);
			box[i] = nil;
		}
}

static Mailbox*
mboxbyname(char *name)
{
	int i;

	for(i=0; i<nbox; i++)
		if(box[i] && strcmp(box[i]->name, name) == 0)
			return box[i];
	return nil;
}

static Mailbox*
mboxbyid(int id)
{
	if(id < 0 || id >= nbox)
		return nil;
	return box[id];
}

static Mailbox*
mboxcreate(char *name)
{
	Mailbox *b;

	b = emalloc(sizeof *b);
	b->name = estrdup(name);
	if(nbox%64 == 0)
		box = erealloc(box, (nbox+64)*sizeof box[0]);
	box[nbox++] = b;
	return b;
}

void
mboxupdate(void)
{
	markboxes();
	if(imapcmd("LIST \"\" *") < 0)
		return;
	sweepboxes();
}
