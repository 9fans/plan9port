#include	"mk.h"

Job *
newjob(Rule *r, Node *nlist, char *stem, char **match, Word *pre, Word *npre, Word *tar, Word *atar)
{
	register Job *j;

	j = (Job *)Malloc(sizeof(Job));
	j->r = r;
	j->n = nlist;
	j->stem = stem;
	j->match = match;
	j->p = pre;
	j->np = npre;
	j->t = tar;
	j->at = atar;
	j->nproc = -1;
	j->next = 0;
	return(j);
}

void
dumpj(char *s, Job *j, int all)
{
	Bprint(&bout, "%s\n", s);
	while(j){
		Bprint(&bout, "job@%ld: r=%ld n=%ld stem='%s' nproc=%d\n",
			j, j->r, j->n, j->stem, j->nproc);
		Bprint(&bout, "\ttarget='%s' alltarget='%s' prereq='%s' nprereq='%s'\n",
			wtos(j->t, ' '), wtos(j->at, ' '), wtos(j->p, ' '), wtos(j->np, ' '));
		j = all? j->next : 0;
	}
}
