#include "mk.h"

static Shell *shells[] = {
	&rcshell,
	&shshell
};

Shell *shelldefault = &shshell;

Shell *shellt;
Word *shellcmd;

typedef struct Shellstack Shellstack;
struct Shellstack
{
	Shell *t;
	Word *w;
	Shellstack *next;
};

Shellstack *shellstack;

char*
setshell(Word *w)
{
	int i;

	if(w->s == nil)
		return "shell name not found on line";

	for(i=0; i<nelem(shells); i++)
		if(shells[i]->matchname(w->s))
			break;
	if(i == nelem(shells))
		return "cannot determine shell type";
	shellt = shells[i];
	shellcmd = w;
	return nil;
}

void
initshell(void)
{
	shellcmd = stow(shelldefault->name);
	shellt = shelldefault;
	setvar("MKSHELL", shellcmd);
}

void
pushshell(void)
{
	Shellstack *s;

	/* save */
	s = Malloc(sizeof *s);
	s->t = shellt;
	s->w = shellcmd;
	s->next = shellstack;
	shellstack = s;

	initshell();	/* reset to defaults */
}

void
popshell(void)
{
	Shellstack *s;

	if(shellstack == nil){
		fprint(2, "internal shellstack error\n");
		Exit();
	}

	s = shellstack;
	shellstack = s->next;
	shellt = s->t;
	shellcmd = s->w;
	setvar("MKSHELL", shellcmd);
	free(s);
}
