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

Fhdr *symhdr, *corhdr;

void
usage(void)
{
	fprint(2, "usage: db [-kw] [-m machine] [-I dir] [symfile] [pid]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	int i, omode;
	char *s;
	char *name;
	Fhdr *hdr;

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
	}ARGEND

	/*
	 * Unix and Plan 9 differ on what the right order of pid, text, and core is.
	 * I never remember anyway.  Let's just accept them in any order.
	 */
	for(i=0; i<argc; i++){
		if(alldigs(argv[i])){
			if(pid){
				dprint("already have pid %d; ignoring pid %d\n", pid, argv[i]);
				continue;
			}
			if(corhdr){
				dprint("already have core %s; ignoring pid %d\n", corfil, pid);
				continue;
			}
			pid = atoi(argv[i]);
			continue;
		}
		if((hdr = crackhdr(argv[i], omode)) == nil){
			dprint("crackhdr %s: %r\n", argv[i]);
			continue;
		}
		dprint("%s: %s %s %s\n", argv[i], hdr->aname, hdr->mname, hdr->fname);
		if(hdr->ftype == FCORE){
			if(pid){
				dprint("already have pid %d; ignoring core %s\n", pid, argv[i]);
				uncrackhdr(hdr);
				continue;
			}
			if(corhdr){
				dprint("already have core %s; ignoring core %s\n", corfil, argv[i]);
				uncrackhdr(hdr);
				continue;
			}
			corhdr = hdr;
			corfil = argv[i];
		}else{
			if(symhdr){
				dprint("already have text %s; ignoring text %s\n", symfil, argv[i]);
				uncrackhdr(hdr);
				continue;
			}
			symhdr = hdr;
			symfil = argv[i];
		}
	}

	if(symhdr==nil){
		symfil = "a.out";
		if(pid){
			if((s = proctextfile(pid)) != nil){
				dprint("pid %d: text %s\n", pid, s);
				symfil = s;
			}
		}
		/* XXX pull command from core */

		if((symhdr = crackhdr(symfil, omode)) == nil){
			dprint("crackhdr %s: %r\n", symfil);
			symfil = nil;
		}
	}

	if(!mach)
		mach = machcpu;

	/*
	 * Set up maps.
	 */
	symmap = allocmap();
	cormap = allocmap();
	if(symmap == nil || cormap == nil)
		sysfatal("allocating maps: %r");

	if(symhdr){
		if(mapfile(symhdr, 0, symmap, nil) < 0)
			dprint("mapping %s: %r\n", symfil);
		mapfile(symhdr, 0, cormap, nil);
	}

	dotmap = dumbmap(-1);

	/*
	 * show initial state and drop into the execution loop.
	 */
	notify(fault);
	setsym();
	if(setjmp(env) == 0){
		if (pid || corhdr)
			setcor();	/* could get error */
		if (correg) {
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
