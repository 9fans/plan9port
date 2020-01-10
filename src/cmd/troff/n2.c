/*
 * n2.c
 *
 * output, cleanup
 */

#define _BSD_SOURCE 1	/* popen */
#define _DEFAULT_SOURCE 1
#include "tdef.h"
#include "fns.h"
#include "ext.h"
#include <setjmp.h>

#ifdef STRICT
	/* not in ANSI or POSIX */
FILE*	popen(char*, char*);
#endif


extern	jmp_buf	sjbuf;
int	toolate;
int	error;

char	obuf[2*BUFSIZ];
char	*obufp = obuf;

	/* pipe command structure; allows redicously long commends for .pi */
struct Pipe {
	char	*buf;
	int	tick;
	int	cnt;
} Pipe;


int	xon	= 0;	/* records if in middle of \X */

int pchar(Tchar i)
{
	int j;
	static int hx = 0;	/* records if have seen HX */

	if (hx) {
		hx = 0;
		j = absmot(i);
		if (isnmot(i)) {
			if (j > dip->blss)
				dip->blss = j;
		} else {
			if (j > dip->alss)
				dip->alss = j;
			ralss = dip->alss;
		}
		return 0;
	}
	if (ismot(i)) {
		pchar1(i);
		return 0;
	}
	switch (j = cbits(i)) {
	case 0:
	case IMP:
	case RIGHT:
	case LEFT:
		return 0;
	case HX:
		hx = 1;
		return 0;
	case XON:
		xon++;
		break;
	case XOFF:
		xon--;
		break;
	case PRESC:
		if (!xon && !tflg && dip == &d[0])
			j = eschar;	/* fall through */
	default:
		setcbits(i, trtab[j]);
	}
	if (NROFF & xon)	/* rob fix for man2html */
		return 0;
	pchar1(i);
	return 0;
}


void pchar1(Tchar i)
{
	int j;

	j = cbits(i);
	if (dip != &d[0]) {
		wbf(i);
		dip->op = offset;
		return;
	}
	if (!tflg && !print) {
		if (j == '\n')
			dip->alss = dip->blss = 0;
		return;
	}
	if (j == FILLER && !xon)
		return;
	if (tflg) {	/* transparent mode, undiverted */
		if (print)			/* assumes that it's ok to print */
			/* OUT "%c", j PUT;	/* i.e., is ascii */
			outascii(i);
		return;
	}
	if (TROFF && ascii)
		outascii(i);
	else
		ptout(i);
}


void outweird(int k)	/* like ptchname() but ascii */
{
	char *chn = chname(k);

	switch (chn[0]) {
	case MBchar:
		OUT "%s", chn+1 PUT;	/* \n not needed? */
		break;
	case Number:
		OUT "\\N'%s'", chn+1 PUT;
		break;
	case Troffchar:
		if (strlen(chn+1) == 2)
			OUT "\\(%s", chn+1 PUT;
		else
			OUT "\\C'%s'", chn+1 PUT;
		break;
	default:
		OUT " %s? ", chn PUT;
		break;
	}
}

void outascii(Tchar i)	/* print i in best-guess ascii */
{
	int j = cbits(i);

/* is this ever called with NROFF set? probably doesn't work at all. */

	if (ismot(i))
		oput(' ');
	else if (j < ALPHABET && j >= ' ' || j == '\n' || j == '\t')
		oput(j);
	else if (j == DRAWFCN)
		oputs("\\D");
	else if (j == HYPHEN)
		oput('-');
	else if (j == MINUS)	/* special pleading for strange encodings */
		oputs("\\-");
	else if (j == PRESC)
		oputs("\\e");
	else if (j == FILLER)
		oputs("\\&");
	else if (j == UNPAD)
		oputs("\\ ");
	else if (j == OHC)	/* this will never occur;  stripped out earlier */
		oputs("\\%");
	else if (j == XON)
		oputs("\\X");
	else if (j == XOFF)
		oputs(" ");
	else if (j == LIG_FI)
		oputs("fi");
	else if (j == LIG_FL)
		oputs("fl");
	else if (j == LIG_FF)
		oputs("ff");
	else if (j == LIG_FFI)
		oputs("ffi");
	else if (j == LIG_FFL)
		oputs("ffl");
	else if (j == WORDSP) {		/* nothing at all */
		if (xon)		/* except in \X */
			oput(' ');

	} else
		outweird(j);
}

int flusho(void)
{
	if (NROFF && !toolate && t.twinit)
			fwrite(t.twinit, strlen(t.twinit), 1, ptid);

	if (obufp > obuf) {
		if (pipeflg && !toolate) {
			/* fprintf(stderr, "Pipe to <%s>\n", Pipe.buf); */
			if (!Pipe.buf[0] || (ptid = popen(Pipe.buf, "w")) == NULL)
				ERROR "pipe %s not created.", Pipe.buf WARN;
			if (Pipe.buf)
				free(Pipe.buf);
		}
		if (!toolate)
			toolate++;
		*obufp = 0;
		fputs(obuf, ptid);
		fflush(ptid);
		obufp = obuf;
	}
	return 1;
}


void caseex(void)
{
	done(0);
}


void done(int x)
{
	int i;

	error |= x;
	app = ds = lgf = 0;
	if (i = em) {
		donef = -1;
		eschar = '\\';
		em = 0;
		if (control(i, 0))
			longjmp(sjbuf, 1);
	}
	if (!nfo)
		done3(0);
	mflg = 0;
	dip = &d[0];
	if (woff)	/* BUG!!! This isn't set anywhere */
		wbf((Tchar)0);
	if (pendw)
		getword(1);
	pendnf = 0;
	if (donef == 1)
		done1(0);
	donef = 1;
	ip = 0;
	frame = stk;
	nxf = frame + 1;
	if (!ejf)
		tbreak();
	nflush++;
	eject((Stack *)0);
	longjmp(sjbuf, 1);
}


void done1(int x)
{
	error |= x;
	if (numtabp[NL].val) {
		trap = 0;
		eject((Stack *)0);
		longjmp(sjbuf, 1);
	}
	if (!ascii)
		pttrailer();
	done2(0);
}


void done2(int x)
{
	ptlead();
	if (TROFF && !ascii)
		ptstop();
	flusho();
	done3(x);
}

void done3(int x)
{
	error |= x;
	flusho();
	if (NROFF)
		twdone();
	if (pipeflg)
		pclose(ptid);
	exit(error);
}


void edone(int x)
{
	frame = stk;
	nxf = frame + 1;
	ip = 0;
	done(x);
}


void casepi(void)
{
	int j;
	char buf[NTM];

	if (Pipe.buf == NULL) {
		if ((Pipe.buf = (char *)calloc(NTM, sizeof(char))) == NULL) {
			ERROR "No buf space for pipe cmd" WARN;
			return;
		}
		Pipe.tick = 1;
	} else
		Pipe.buf[Pipe.cnt++] = '|';

	getline(buf, NTM);
	j = strlen(buf);
	if (toolate) {
		ERROR "Cannot create pipe to %s", buf WARN;
		return;
	}
	Pipe.cnt += j;
	if (j >= NTM +1) {
		Pipe.tick++;
		if ((Pipe.buf = (char *)realloc(Pipe.buf, Pipe.tick * NTM * sizeof(char))) == NULL) {
			ERROR "No more buf space for pipe cmd" WARN;
			return;
		}
	}
	strcat(Pipe.buf, buf);
	pipeflg++;
}
