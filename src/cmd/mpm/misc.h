#include	<stdio.h>
#include	<stdlib.h>
#include	<math.h>
#include	<ctype.h>
#include	<string.h>

// XXX: Apparently necessary for g++
#define	typename tyname

extern char	errbuf[];
extern char	*progname;
extern int	linenum;
extern int	wantwarn;

// #define	ERROR	fflush(stdout), fprintf(stderr, "%s: ", progname), fprintf(stderr,
// #define	FATAL	), exit(1)
// #define	WARNING	)

#define	ERROR	fprintf(stdout, "\n#MESSAGE TO USER:  "), sprintf(errbuf,
#define	FATAL	), fputs(errbuf, stdout), \
		fprintf(stderr, "%s: ", progname), \
		fputs(errbuf, stderr), \
		fflush(stdout), \
		exit(1)
#define	WARNING	), fputs(errbuf, stdout), \
		wantwarn ? \
			fprintf(stderr, "%s: ", progname), \
			fputs(errbuf, stderr) : 0, \
		fflush(stdout)

#define	eq(s,t)	(strcmp(s,t) == 0)

inline int	max(int x, int y)	{ return x > y ? x : y; }
inline int	min(int x, int y)	{ return x > y ? y : x; }
inline int	abs(int x)		{ return (x >= 0) ? x : -x; }

extern int	dbg;

extern int	pn, userpn;		// actual and user-defined page numbers
extern int	pagetop, pagebot;	// printing margins
extern int	physbot;		// physical bottom of the page
