/*
 * functions for running the debugged process
 */

#include "defs.h"
#include "fns.h"


int child;
int msgfd = -1;
int notefd = -1;
int pcspid = -1;
int pcsactive = 0;

void
setpcs(void)
{
	char buf[128];

	if(pid && pid != pcspid){
		if(msgfd >= 0){
			close(msgfd);
			msgfd = -1;
		}
		if(notefd >= 0){
			close(notefd);
			notefd = -1;
		}
		pcspid = -1;
		sprint(buf, "/proc/%d/ctl", pid);
		msgfd = open(buf, OWRITE);
		if(msgfd < 0)
			error("can't open control file");
		sprint(buf, "/proc/%d/note", pid);
		notefd = open(buf, ORDWR);
		if(notefd < 0)
			error("can't open note file");
		pcspid = pid;
	}
}

void
msgpcs(char *msg)
{
	char err[ERRMAX];

	setpcs();
	if(write(msgfd, msg, strlen(msg)) < 0 && !ending){
		errstr(err, sizeof err);
		if(strcmp(err, "interrupted") != 0)
			endpcs();
		errors("can't write control file", err);
	}
}

/*
 * empty the note buffer and toss pending breakpoint notes
 */
void
unloadnote(void)
{
	char err[ERRMAX];

	setpcs();
	for(; nnote<NNOTE; nnote++){
		switch(read(notefd, note[nnote], sizeof note[nnote])){
		case -1:
			errstr(err, sizeof err);
			if(strcmp(err, "interrupted") != 0)
				endpcs();
			errors("can't read note file", err);
		case 0:
			return;
		}
		note[nnote][ERRMAX-1] = '\0';
		if(strncmp(note[nnote], "sys: breakpoint", 15) == 0)
			--nnote;
	}
}

/*
 * reload the note buffer
 */
void
loadnote(void)
{
	int i;
	char err[ERRMAX];

	setpcs();
	for(i=0; i<nnote; i++){
		if(write(notefd, note[i], strlen(note[i])) < 0){
			errstr(err, sizeof err);
			if(strcmp(err, "interrupted") != 0)
				endpcs();
			errors("can't write note file", err);
		}
	}
	nnote = 0;
}

void
notes(void)
{
	int n;

	if(nnote == 0)
		return;
	dprint("notes:\n");
	for(n=0; n<nnote; n++)
		dprint("%d:\t%s\n", n, note[n]);
}

void
killpcs(void)
{
	msgpcs("kill");
}

void
grab(void)
{
	flush();
	msgpcs("stop");
	bpwait();
}

void
ungrab(void)
{
	msgpcs("start");
}

void
doexec(void)
{
	char *argl[MAXARG];
	char args[LINSIZ];
	char *p;
	char **ap;
	char *thisarg;

	ap = argl;
	p = args;
	*ap++ = symfil;
	for (rdc(); lastc != EOR;) {
		thisarg = p;
		if (lastc == '<' || lastc == '>') {
			*p++ = lastc;
			rdc();
		}
		while (lastc != EOR && lastc != SPC && lastc != TB) {
			*p++ = lastc;
			readchar();
		}
		if (lastc == SPC || lastc == TB)
			rdc();
		*p++ = 0;
		if (*thisarg == '<') {
			close(0);
			if (open(&thisarg[1], OREAD) < 0) {
				print("%s: cannot open\n", &thisarg[1]);
				_exits(0);
			}
		}
		else if (*thisarg == '>') {
			close(1);
			if (create(&thisarg[1], OWRITE, 0666) < 0) {
				print("%s: cannot create\n", &thisarg[1]);
				_exits(0);
			}
		}
		else
			*ap++ = thisarg;
	}
	*ap = 0;
	exec(symfil, argl);
	perror(symfil);
}

char	procname[100];

void
startpcs(void)
{
	if ((pid = fork()) == 0) {
		pid = getpid();
		msgpcs("hang");
		doexec();
		exits(0);
	}

	if (pid == -1)
		error("can't fork");
	child++;
	sprint(procname, "/proc/%d/mem", pid);
	corfil = procname;
	msgpcs("waitstop");
	bpwait();
	if (adrflg)
		rput(correg, mach->pc, adrval);
	while (rdc() != EOR)
		;
	reread();
}

void
runstep(ulong loc, int keepnote)
{
	int nfoll;
	ADDR foll[3];
	BKPT bkpt[3];
	int i;

	if(mach->foll == 0){
		dprint("stepping unimplemented; assuming not a branch\n");
		nfoll = 1;
		foll[0] = loc+mach->pcquant;
	}else {
		nfoll = mach->foll(cormap, correg, loc, foll);
		if (nfoll < 0)
			error("%r");
	}
	memset(bkpt, 0, sizeof bkpt);
	for(i=0; i<nfoll; i++){
		if(foll[i] == loc)
			error("can't single step: next instruction is dot");
		bkpt[i].loc = foll[i];
		bkput(&bkpt[i], 1);
	}
	runrun(keepnote);
	for(i=0; i<nfoll; i++)
		bkput(&bkpt[i], 0);
}

void
bpwait(void)
{
	setcor();
	unloadnote();
}

void
runrun(int keepnote)
{
	int on;

	on = nnote;
	unloadnote();
	if(on != nnote){
		notes();
		error("not running: new notes pending");
	}
	if(keepnote)
		loadnote();
	else
		nnote = 0;
	flush();
	msgpcs("startstop");
	bpwait();
}

void
bkput(BKPT *bp, int install)
{
	char buf[256];
	ulong loc;
	int ret;

	errstr(buf, sizeof buf);
/*
	if(mach->bpfix)
		loc = (*mach->bpfix)(bp->loc);
	else
*/
	loc = bp->loc;
	if(install){
		ret = get1(cormap, loc, bp->save, mach->bpsize);
		if (ret > 0)
			ret = put1(cormap, loc, mach->bpinst, mach->bpsize);
	}else
		ret = put1(cormap, loc, bp->save, mach->bpsize);
	if(ret < 0){
		sprint(buf, "can't set breakpoint at %#llux: %r", bp->loc);
		print(buf);
		read(0, buf, 100);
	}
}
