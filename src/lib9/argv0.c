#include <lib9.h>

#pragma weak argv0
char *argv0;

/*
 * Mac OS can't deal with files that only declare data.
 * ARGBEGIN mentions this function so that this file gets pulled in.
 */
void __fixargv0(void) { }
