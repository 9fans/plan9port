#ifndef _UTFH_
#define _UTFH_ 1

typedef unsigned short Rune;	/* 16 bits */

enum
{
	UTFmax		= 3,		/* maximum bytes per rune */
	Runesync	= 0x80,		/* cannot represent part of a UTF sequence (<) */
	Runeself	= 0x80,		/* rune and UTF sequences are the same (<) */
	Runeerror	= 0x80,		/* decoding error in UTF */
};

/*
 * rune routines
 */
extern	int	runetochar(char*, Rune*);
extern	int	chartorune(Rune*, char*);
extern	int	runelen(long);
extern	int	runenlen(Rune*, int);
extern	int	fullrune(char*, int);
extern	int	utflen(char*);
extern	int	utfnlen(char*, long);
extern	char*	utfrune(char*, long);
extern	char*	utfrrune(char*, long);
extern	char*	utfutf(char*, char*);
extern	char*	utfecpy(char*, char*, char*);

extern	Rune*	runestrcat(Rune*, Rune*);
extern	Rune*	runestrchr(Rune*, Rune);
extern	int	runestrcmp(Rune*, Rune*);
extern	Rune*	runestrcpy(Rune*, Rune*);
extern	Rune*	runestrncpy(Rune*, Rune*, long);
extern	Rune*	runestrecpy(Rune*, Rune*, Rune*);
extern	Rune*	runestrdup(Rune*);
extern	Rune*	runestrncat(Rune*, Rune*, long);
extern	int	runestrncmp(Rune*, Rune*, long);
extern	Rune*	runestrrchr(Rune*, Rune);
extern	long	runestrlen(Rune*);
extern	Rune*	runestrstr(Rune*, Rune*);

extern	Rune	tolowerrune(Rune);
extern	Rune	totitlerune(Rune);
extern	Rune	toupperrune(Rune);
extern	int	isalpharune(Rune);
extern	int	islowerrune(Rune);
extern	int	isspacerune(Rune);
extern	int	istitlerune(Rune);
extern	int	isupperrune(Rune);

#endif
