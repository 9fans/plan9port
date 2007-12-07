#include "lib9.h"
#include "regexp9.h"
#include "regcomp.h"


/*
 *  save a new match in mp
 */
extern void
_renewmatch(Resub *mp, int ms, Resublist *sp)
{
	int i;

	if(mp==0 || ms<=0)
		return;
	if(mp[0].s.sp==0 || sp->m[0].s.sp<mp[0].s.sp ||
	   (sp->m[0].s.sp==mp[0].s.sp && sp->m[0].e.ep>mp[0].e.ep)){
		for(i=0; i<ms && i<NSUBEXP; i++)
			mp[i] = sp->m[i];
		for(; i<ms; i++)
			mp[i].s.sp = mp[i].e.ep = 0;
	}
}

/*
 * Add ip to the list [lp, elp], but only if it is not there already.
 * These work lists are stored and processed in increasing
 * order of sp[0], so if the ip is there already, the one that's
 * there already is a more left match and takes priority.
 */
static Relist*
_renewthread1(Relist *lp,	/* Relist to add to */
	Relist *elp,		/* limit pointer for Relist */
	Reinst *ip,		/* instruction to add */
	int ms,
	Resublist *sep)		/* pointers to subexpressions */
{
	Relist *p;

	for(p=lp; p->inst; p++)
		if(p->inst == ip)
			return 0;
	
	if(p == elp)	/* refuse to overflow buffer */
		return elp;

	p->inst = ip;
	if(ms > 1)
		p->se = *sep;
	else
		p->se.m[0] = sep->m[0];
	(p+1)->inst = 0;
	return p;
}

extern int
_renewthread(Relist *lp, Relist *elp, Reinst *ip, int ms, Resublist *sep)
{
	Relist *ap;

	ap = _renewthread1(lp, elp, ip, ms, sep);
	if(ap == 0)
		return 0;
	if(ap == elp)
		return -1;

	/*
	 * Added ip to list at ap.  
	 * Expand any ORs right now, so that entire
	 * work list ends up being sorted by increasing m[0].sp.
	 */
	for(; ap->inst; ap++){
		if(ap->inst->type == OR){
			if(_renewthread1(lp, elp, ap->inst->u1.right, ms, &ap->se) == elp)
				return -1;
			if(_renewthread1(lp, elp, ap->inst->u2.next, ms, &ap->se) == elp)
				return -1;
		}
	}
	return 0;
}

/*
 * same as renewthread, but called with
 * initial empty start pointer.
 */
extern int
_renewemptythread(Relist *lp,	/* _relist to add to */
	Relist *elp,
	Reinst *ip,		/* instruction to add */
	int ms,
	char *sp)		/* pointers to subexpressions */
{
	Resublist sep;
	
	if(ms > 1)
		memset(&sep, 0, sizeof sep);
	sep.m[0].s.sp = sp;
	sep.m[0].e.ep = 0;
	return _renewthread(lp, elp, ip, ms, &sep);
}

extern int
_rrenewemptythread(Relist *lp,	/* _relist to add to */
	Relist *elp,
	Reinst *ip,		/* instruction to add */
	int ms,
	Rune *rsp)		/* pointers to subexpressions */
{
	return _renewemptythread(lp, elp, ip, ms, (char*)rsp);
}
