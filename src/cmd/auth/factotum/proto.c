#include "std.h"
#include "dat.h"

extern Proto	apop;		/* apop.c */
extern Proto	chap;		/* chap.c */
extern Proto	cram;		/* apop.c */
extern Proto	dsa;			/* dsa.c */
extern Proto	httpdigest;	/* httpdigest.c */
extern Proto	mschap;		/* chap.c */
extern Proto	p9any;		/* p9any.c */
extern Proto	p9sk1;		/* p9sk1.c */
extern Proto	p9sk2;		/* p9sk2.c */
extern Proto	p9cr;
extern Proto	pass;			/* pass.c */
extern Proto	rsa;			/* rsa.c */

Proto *prototab[] = {
	&apop,
	&cram,
	&dsa,
	&httpdigest,
	&p9any,
	&p9cr,
	&p9sk1,
	&p9sk2,
	&pass,
	&rsa,
	nil
};

Proto*
protolookup(char *name)
{
	int i;

	for(i=0; prototab[i]; i++)
		if(strcmp(prototab[i]->name, name) == 0)
			return prototab[i];
	return nil;
}
