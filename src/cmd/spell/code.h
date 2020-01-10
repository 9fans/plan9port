
/*
 * affix codes
 */

#define	ED		(1<<0)	/* +ed, +ing */
#define	ADJ		(1<<1)	/* (nce)-t_ce, +ize,+al, +ness, -t+cy, +ity, +ly */
#define	NOUN		(1<<2)	/* +s (+es), +make, +hood, +ship +less  */
#define PROP_COLLECT	(1<<3)	/* +'s,  +an, +ship(for -manship) +less */
#define ACTOR		(1<<4)	/* +er  */
#define	EST		(1<<5)
#define COMP		(EST|ACTOR)	/* +er,+est */
#define	DONT_TOUCH	(1<<6)
#define	ION		(1<<7)	/* +ion, +or */
#define	N_AFFIX		(1<<8) 	/* +ic, +ive, +ize, +like, +al, +ful, +ism, +ist, -t+cy, +c (maniac) */
#define	V_AFFIX		(1<<9)	/* +able, +ive, +ity((bility), +ment */
#define	V_IRREG		(1<<10)	/* +ing +es +s*/
#define	VERB		(V_IRREG|ED)
#define MAN		(1<<11)	/* +man, +men, +women, +woman */
#define	ADV		(1<<12)	/* +hood, +ness */
#define STOP		(1<<14)	/* stop list */
#define	NOPREF		(1<<13)	/* no prefix */

#define MONO		(1<<15)	/* double final consonant as in fib->fibbing */
#define IN		(1<<16) /* in- im- ir, not un- */
#define _Y		(1<<17)	/* +y */

#define ALL		(~(NOPREF|STOP|DONT_TOUCH|MONO|IN))    /*anything goes (no stop or nopref)*/
