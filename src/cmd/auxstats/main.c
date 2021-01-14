#include <u.h>
#include <libc.h>
#include <bio.h>
#include "dat.h"

Biobuf bout;

void
usage(void)
{
	fprint(2, "usage: auxstats [system [executable]]\n");
	exits("usage");
}

int pid;

void
notifyf(void *v, char *msg)
{
	USED(v);

	if(strstr(msg, "child"))
		noted(NCONT);
	if(pid)
		postnote(PNPROC, pid, msg);
	exits(nil);
}

void
main(int argc, char **argv)
{
	char *sys, *exe;
	int i;
	Waitmsg *w;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	notify(notifyf);

	sys = nil;
	exe = nil;
	if(argc > 0)
		sys = argv[0];
	if(argc > 1)
		exe = argv[1];
	if(argc > 2)
		usage();

	if(sys){
		if(exe == nil)
			exe = "auxstats";
		for(;;){
			switch(pid = fork()){
			case -1:
				sysfatal("fork: %r");
			case 0:
				rfork(RFNOTEG);
				execlp("ssh", "ssh", "-nTC", sys, exe, nil);
				_exit(97);
			default:
				if((w = wait()) == nil)
					sysfatal("wait: %r");
				if(w->pid != pid)
					sysfatal("wait got wrong pid");
				if(atoi(w->msg) == 97)
					sysfatal("exec ssh failed");
				free(w);
				fprint(2, "stats: %s hung up; sleeping 60\n", sys);
				sleep(60*1000);
				fprint(2, "stats: redialing %s\n", sys);
				break;
			}
		}
	}

	for(i=0; statfn[i]; i++)
		statfn[i](1);

	Binit(&bout, 1, OWRITE);
	for(;;){
		Bflush(&bout);
		sleep(900);
		for(i=0; statfn[i]; i++)
			statfn[i](0);
	}
}

char buf[16384];
char *line[100];
char *tok[100];
int nline, ntok;

void
readfile(int fd)
{
	int n;

	if(fd == -1){
		nline = 0;
		return;
	}

	seek(fd, 0, 0);
	n = read(fd, buf, sizeof buf-1);
	if(n < 0)
		n = 0;
	buf[n] = 0;

	nline = getfields(buf, line, nelem(line), 0, "\n");
}

void
tokens(int i)
{
	if(i >= nline){
		ntok = 0;
		return;
	}
	ntok = tokenize(line[i], tok, nelem(tok));
}
