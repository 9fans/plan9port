/*
 * db - common definitions
 * something of a grab-bag
 */

#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>

#include <mach.h>

typedef long WORD;
typedef u64int ADDR;

#define	HUGEINT	0x7fffffff	/* enormous WORD */

#define	MAXOFF	0x1000000
#define	INCDIR	"/usr/lib/adb"
#define	DBNAME	"db\n"
#define CMD_VERBS	"?/=>!$: \t"

typedef	int	BOOL;

#define MAXPOS	80
#define MAXLIN	128
#define	ARB	512
#define MAXCOM	64
#define MAXARG	32
#define LINSIZ	4096
#define	MAXSYM	255

#define EOR	'\n'
#define SPC	' '
#define TB	'\t'

#define	STDIN	0
#define	STDOUT	1

#define	TRUE	(-1)
#define	FALSE	0


/*
 * run modes
 */

#define	SINGLE	1
#define	CONTIN	2

/*
 * breakpoints
 */

#define	BKPTCLR	0	/* not a real breakpoint */
#define BKPTSET	1	/* real, ready to trap */
#define BKPTSKIP 2	/* real, skip over it next time */
#define	BKPTTMP	3	/* temporary; clear when it happens */

struct bkpt {
	ADDR	loc;
	uchar	save[4];
	int	count;
	int	initcnt;
	int	flag;
	char	comm[MAXCOM];
	struct bkpt *nxtbkpt;
};
typedef struct bkpt	BKPT;

#define	BADREG	(-1)

/*
 * common globals
 */

extern	WORD	adrval;
extern	vlong	expv;
extern	int	adrflg;
extern	WORD	cntval;
extern	int	cntflg;
extern	WORD	loopcnt;
extern	ADDR	maxoff;
extern	ADDR	localval;
extern	ADDR	maxfile;
extern	ADDR	maxstor;

extern	ADDR	dot;
extern	WORD	dotinc;

extern	int	xargc;

extern	BOOL	wtflag;
extern	char	*corfil, *symfil;
extern	BOOL	mkfault;
extern	BOOL	regdirty;

extern	int	pid;
extern	int	pcsactive;
#define	NNOTE 10
extern	int	nnote;
extern	char	note[NNOTE][ERRMAX];

extern	int	ending;
extern	Map	*dotmap;

extern	BKPT	*bkpthead;
extern	int	kflag;
extern	int	lastc, peekc;
