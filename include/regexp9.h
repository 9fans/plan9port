#ifndef _REGEXP9_H_
#define _REGEXP9_H_ 1
#if defined(__cplusplus)
extern "C" { 
#endif

#ifdef AUTOLIB
AUTOLIB(regexp9)
#endif

#include <utf.h>

typedef struct Resub		Resub;
typedef struct Reclass		Reclass;
typedef struct Reinst		Reinst;
typedef struct Reprog		Reprog;

/*
 *	Sub expression matches
 */
struct Resub{
	union
	{
		char *sp;
		Rune *rsp;
	}s;
	union
	{
		char *ep;
		Rune *rep;
	}e;
};

/*
 *	character class, each pair of rune's defines a range
 */
struct Reclass{
	Rune	*end;
	Rune	spans[64];
};

/*
 *	Machine instructions
 */
struct Reinst{
	int	type;
	union	{
		Reclass	*cp;		/* class pointer */
		Rune	r;		/* character */
		int	subid;		/* sub-expression id for RBRA and LBRA */
		Reinst	*right;		/* right child of OR */
	}u1;
	union {	/* regexp relies on these two being in the same union */
		Reinst *left;		/* left child of OR */
		Reinst *next;		/* next instruction for CAT & LBRA */
	}u2;
};

/*
 *	Reprogram definition
 */
struct Reprog{
	Reinst	*startinst;	/* start pc */
	Reclass	class[16];	/* .data */
	Reinst	firstinst[5];	/* .text */
};

extern Reprog	*regcomp9(char*);
extern Reprog	*regcomplit9(char*);
extern Reprog	*regcompnl9(char*);
extern void	regerror9(char*);
extern int	regexec9(Reprog*, char*, Resub*, int);
extern void	regsub9(char*, char*, int, Resub*, int);

extern int	rregexec9(Reprog*, Rune*, Resub*, int);
extern void	rregsub9(Rune*, Rune*, int, Resub*, int);

/*
 * Darwin simply cannot handle having routines that
 * override other library routines.
 */
#ifndef NOPLAN9DEFINES
#define regcomp regcomp9
#define regcomplit regcomplit9
#define regcompnl regcompnl9
#define regerror regerror9
#define regexec regexec9
#define regsub regsub9
#define rregexec rregexec9
#define rregsub rregsub9
#endif

#if defined(__cplusplus)
}
#endif
#endif
