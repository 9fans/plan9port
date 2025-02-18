#include "rc.h"
#include "exec.h"
#include "fns.h"
#include "io.h"

int ntrap;
int trap[NSIG];

void
dotrap(void)
{
	int i;
	var *trapreq;
	word *starval;
	starval = vlook("*")->val;
	while(ntrap) for(i = 0;i<NSIG;i++) while(trap[i]){
		--trap[i];
		--ntrap;
		if(getpid()!=mypid) Exit();
		trapreq = vlook(Signame[i]);
		if(trapreq->fn)
			startfunc(trapreq, copywords(starval, (word*)0), (var*)0, (redir*)0);
		else if(i==SIGINT || i==SIGQUIT){
			/*
			 * run the stack down until we uncover the
			 * command reading loop.  Xreturn will exit
			 * if there is none (i.e. if this is not
			 * an interactive rc.)
			 */
			while(!runq->iflag) Xreturn();
		}
		else Exit();
	}
}
