/*
 * db - main command loop and error/interrupt handling
 */
#include "defs.h"
#include "fns.h"

int wtflag = OREAD;
BOOL kflag;

BOOL mkfault;
ADDR maxoff;

int xargc;		/* bullshit */

extern	BOOL	executing;
extern	int	infile;
int	exitflg;
extern	int	eof;

int	alldigs(char*);
void	fault(void*, char*);

extern	char	*Ipath;
jmp_buf env;
static char *errmsg;

void
usage(void)
{
	fprint(2, "usage: db [-kw] [-m machine] [-I dir] [symfile] [pid]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	int omode;
	volatile int quiet;
	char *s;
	char *name;

	quiet = 0;
	name = 0;
	outputinit();
	maxoff = MAXOFF;
	omode = OREAD;
	ARGBEGIN{
	default:
		usage();
	case 'A':
		abort();
	case 'k':
		kflag = 1;
		break;
	case 'w':
		omode = ORDWR;
		break;
	case 'I':
		s = ARGF();
		if(s == 0)
			dprint("missing -I argument\n");
		else
			Ipath = s;
		break;
	case 'm':
		name = ARGF();
		if(name == 0)
			dprint("missing -m argument\n");
		break;
	case 'q':
		quiet = 1;
		break;
	}ARGEND

	attachargs(argc, argv, omode, !quiet);

	dotmap = dumbmap(-1);

	/*
	 * show initial state and drop into the execution loop.
	 */
	notify(fault);
	setsym();
	if(setjmp(env) == 0){
		if (pid || corhdr)
			setcor();	/* could get error */
		if (correg && !quiet) {
			dprint("%s\n", mach->exc(cormap, correg));
			printpc();
		}
	}

	setjmp(env);
	if (executing)
		delbp();
	executing = FALSE;
	for (;;) {
		flushbuf();
		if (errmsg) {
			dprint(errmsg);
			printc('\n');
			errmsg = 0;
			exitflg = 0;
		}
		if (mkfault) {
			mkfault=0;
			printc('\n');
			prints(DBNAME);
		}
		clrinp();
		rdc();
		reread();
		if (eof) {
			if (infile == STDIN)
				done();
			iclose(-1, 0);
			eof = 0;
			longjmp(env, 1);
		}
		exitflg = 0;
		command(0, 0);
		reread();
		if (rdc() != '\n')
			error("newline expected");
	}
}

int
alldigs(char *s)
{
	while(*s){
		if(*s<'0' || '9'<*s)
			return 0;
		s++;
	}
	return 1;
}

void
done(void)
{
	if (pid)
		endpcs();
	exits(exitflg? "error": 0);
}

/*
 * An error occurred; save the message for later printing,
 * close open files, and reset to main command loop.
 */
void
error(char *n)
{
	errmsg = n;
	iclose(0, 1);
	oclose();
	flush();
	delbp();
	ending = 0;
	longjmp(env, 1);
}

void
errors(char *m, char *n)
{
	static char buf[128];

	sprint(buf, "%s: %s", m, n);
	error(buf);
}

/*
 * An interrupt occurred;
 * seek to the end of the current file
 * and remember that there was a fault.
 */
void
fault(void *a, char *s)
{
	USED(a);
	if(strncmp(s, "interrupt", 9) == 0){
		seek(infile, 0L, 2);
		mkfault++;
		noted(NCONT);
	}
	noted(NDFLT);
}
