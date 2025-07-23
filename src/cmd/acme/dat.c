#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <plumb.h>
#include <libsec.h>
#include "dat.h"

uint		globalincref;
uint		seq;
uint		maxtab;	/* size of a tab, in units of the '0' character */

Mouse		*mouse;
Mousectl		*mousectl;
Keyboardctl	*keyboardctl;
Reffont		reffont;
Image		*modbutton;
Image		*colbutton;
Image		*button;
Image		*but2col;
Image		*but3col;
Row			row;
int			timerpid;
Disk			*disk;
Text			*seltext;
Text			*argtext;
Text			*mousetext;	/* global because Text.close needs to clear it */
Text			*typetext;		/* global because Text.close needs to clear it */
Text			*barttext;		/* shared between mousetask and keyboardthread */
int			bartflag;
Window		*activewin;
Column		*activecol;
Rectangle		nullrect;
int			fsyspid;
char			*cputype;
char			*objtype;
char			*acmeshell;
//char			*fontnames[2];
extern char		wdir[]; /* must use extern because no dimension given */
int			globalautoindent;
int			dodollarsigns;

Channel	*cplumb;		/* chan(Plumbmsg*) */
Channel	*cwait;		/* chan(Waitmsg) */
Channel	*ccommand;	/* chan(Command*) */
Channel	*ckill;		/* chan(Rune*) */
Channel	*cxfidalloc;	/* chan(Xfid*) */
Channel	*cxfidfree;	/* chan(Xfid*) */
Channel	*cnewwindow;	/* chan(Channel*) */
Channel	*mouseexit0;	/* chan(int) */
Channel	*mouseexit1;	/* chan(int) */
Channel	*cexit;		/* chan(int) */
Channel	*cerr;		/* chan(char*) */
Channel	*cedit;		/* chan(int) */
Channel	*cwarn;		/* chan(void*)[1] (really chan(unit)[1]) */

QLock	editoutlk;
