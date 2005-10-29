#include "common.h"
#include "send.h"

/* pipe an address through a command to translate it */
extern dest *
translate(dest *dp)
{
	process *pp;
	String *line;
	dest *rv;
	char *cp;
	int n;

	pp = proc_start(s_to_c(dp->repl1), (stream *)0, outstream(), outstream(), 1, 0);
	if (pp == 0) {
		dp->status = d_resource;
		return 0;
	}
	line = s_new();
	for(;;) {
		cp = Brdline(pp->std[1]->fp, '\n');
		if(cp == 0)
			break;
		if(strncmp(cp, "_nosummary_", 11) == 0){
			nosummary = 1;
			continue;
		}
		n = Blinelen(pp->std[1]->fp);
		cp[n-1] = ' ';
		s_nappend(line, cp, n);
	}
	rv = s_to_dest(s_restart(line), dp);
	s_restart(line);
	while(s_read_line(pp->std[2]->fp, line))
		;
	if ((dp->pstat = proc_wait(pp)) != 0) {
		dp->repl2 = line;
		rv = 0;
	} else
		s_free(line);
	proc_free(pp);
	return rv;
}
