#include "rc.h"
#include "exec.h"
#include "fns.h"
#include "io.h"
extern char *Signame[];
void dotrap(void){
	register int i;
	register struct var *trapreq;
	register struct word *starval;
	starval=vlook("*")->val;
	while(ntrap) for(i=0;i!=NSIG;i++) while(trap[i]){
		--trap[i];
		--ntrap;
		if(getpid()!=mypid) Exit(getstatus());
		trapreq=vlook(Signame[i]);
		if(trapreq->fn){
			start(trapreq->fn, trapreq->pc, (struct var *)0);
			runq->local=newvar(strdup("*"), runq->local);
			runq->local->val=copywords(starval, (struct word *)0);
			runq->local->changed=1;
			runq->redir=runq->startredir=0;
		}
		else if(i==SIGINT || i==SIGQUIT){
			/*
			 * run the stack down until we uncover the
			 * command reading loop.  Xreturn will exit
			 * if there is none (i.e. if this is not
			 * an interactive rc.)
			 */
			while(!runq->iflag) Xreturn();
		}
		else Exit(getstatus());
	}
}
