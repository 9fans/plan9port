#include "e.h"

	/* YOU MAY WANT TO CHANGE THIS */
char	*typesetter = "post";	/* type of typesetter today */
int	ttype	= DEVPOST;
int	minsize	= 4;		/* min size it can handle */


int	dbg;		/* debugging print if non-zero */
int	lp[200];	/* stack for things like piles and matrices */
int	ct;		/* pointer to lp */
int	used[100];	/* available registers */
int	ps;		/* default init point size */
int	deltaps	= 3;	/* default change in ps */
int	dps_set = 0;	/* 1 => -p option used */
int	gsize	= 10;	/* default initial point size */
int	ft	= '2';
Font	ftstack[10] = { '2', "2" };	/* bottom is global font */
Font	*ftp	= ftstack;
int	szstack[10];	/* non-zero if absolute size set at this level */
int	nszstack = 0;
int	display	= 0;	/* 1=>display, 0=>.EQ/.EN */

int	synerr;		/* 1 if syntax err in this eqn */
double	eht[100];	/* height in ems at gsize */
double	ebase[100];	/* base: where one enters above bottom */
int	lfont[100];	/* leftmost and rightmost font associated with this thing */
int	rfont[100];
int	lclass[100];	/* leftmost and rightmost class associated with this thing */
int	rclass[100];
int	eqnreg;		/* register where final string appears */
double	eqnht;		/* final height of equation */
int	lefteq	= '\0';	/* left in-line delimiter */
int	righteq	= '\0';	/* right in-line delimiter */
int	markline = 0;	/* 1 if this EQ/EN contains mark; 2 if lineup */
