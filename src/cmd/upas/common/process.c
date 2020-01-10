#include "common.h"

/* make a stream to a child process */
extern stream *
instream(void)
{
	stream *rv;
	int pfd[2];

	if ((rv = (stream *)malloc(sizeof(stream))) == 0)
		return 0;
	memset(rv, 0, sizeof(stream));
	if (pipe(pfd) < 0)
		return 0;
	if(Binit(&rv->bb, pfd[1], OWRITE) < 0){
		close(pfd[0]);
		close(pfd[1]);
		return 0;
	}
	rv->fp = &rv->bb;
	rv->fd = pfd[0];
	return rv;
}

/* make a stream from a child process */
extern stream *
outstream(void)
{
	stream *rv;
	int pfd[2];

	if ((rv = (stream *)malloc(sizeof(stream))) == 0)
		return 0;
	memset(rv, 0, sizeof(stream));
	if (pipe(pfd) < 0)
		return 0;
	if (Binit(&rv->bb, pfd[0], OREAD) < 0){
		close(pfd[0]);
		close(pfd[1]);
		return 0;
	}
	rv->fp = &rv->bb;
	rv->fd = pfd[1];
	return rv;
}

extern void
stream_free(stream *sp)
{
	int fd;

	close(sp->fd);
	fd = Bfildes(sp->fp);
	Bterm(sp->fp);
	close(fd);
	free((char *)sp);
}

/* start a new process */
extern process *
noshell_proc_start(char **av, stream *inp, stream *outp, stream *errp, int newpg, char *who)
{
	process *pp;
	int i, n;

	if ((pp = (process *)malloc(sizeof(process))) == 0) {
		if (inp != 0)
			stream_free(inp);
		if (outp != 0)
			stream_free(outp);
		if (errp != 0)
			stream_free(errp);
		return 0;
	}
	pp->std[0] = inp;
	pp->std[1] = outp;
	pp->std[2] = errp;
	switch (pp->pid = fork()) {
	case -1:
		proc_free(pp);
		return 0;
	case 0:
		if(newpg)
			sysdetach();
		for (i=0; i<3; i++)
			if (pp->std[i] != 0){
				close(Bfildes(pp->std[i]->fp));
				while(pp->std[i]->fd < 3)
					pp->std[i]->fd = dup(pp->std[i]->fd, -1);
			}
		for (i=0; i<3; i++)
			if (pp->std[i] != 0)
				dup(pp->std[i]->fd, i);
		for (n = sysfiles(); i < n; i++)
			close(i);
		if(who)
			fprint(2, "warning: cannot run %s as %s\n", av[0], who);
		exec(av[0], av);
		perror("proc_start");
		exits("proc_start");
	default:
		for (i=0; i<3; i++)
			if (pp->std[i] != 0) {
				close(pp->std[i]->fd);
				pp->std[i]->fd = -1;
			}
		return pp;
	}
}

/* start a new process under a shell */
extern process *
proc_start(char *cmd, stream *inp, stream *outp, stream *errp, int newpg, char *who)
{
	char *av[4];

	upasconfig();
	av[0] = SHELL;
	av[1] = "-c";
	av[2] = cmd;
	av[3] = 0;
	return noshell_proc_start(av, inp, outp, errp, newpg, who);
}

/* wait for a process to stop */
extern int
proc_wait(process *pp)
{
	Waitmsg *status;
	char err[Errlen];

	for(;;){
		status = wait();
		if(status == nil){
			errstr(err, sizeof(err));
			if(strstr(err, "interrupt") == 0)
				break;
		}
		if (status->pid==pp->pid)
			break;
	}
	pp->pid = -1;
	if(status == nil)
		pp->status = -1;
	else
		pp->status = status->msg[0];
	pp->waitmsg = status;
	return pp->status;
}

/* free a process */
extern int
proc_free(process *pp)
{
	int i;

	if(pp->std[1] == pp->std[2])
		pp->std[2] = 0;		/* avoid freeing it twice */
	for (i = 0; i < 3; i++)
		if (pp->std[i])
			stream_free(pp->std[i]);
	if (pp->pid >= 0)
		proc_wait(pp);
	free(pp->waitmsg);
	free((char *)pp);
	return 0;
}

/* kill a process */
extern int
proc_kill(process *pp)
{
	return syskill(pp->pid);
}
