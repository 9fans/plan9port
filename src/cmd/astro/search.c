#include "astro.h"

char*	solstr[] =
{
	"Fall equinox",
	"Winter solstice",
	"Spring equinox",
	"Summer solstice",
};

struct
{
	double	beta;
	int	rta;
	int	dec;
	char	*betstr;
} bettab[] =
{
	-1.3572, 231,	50,	"Quadrantid",
	 0.7620, 336,	0,	"Eta aquarid",
	 1.5497, 260,	-20,	"Ophiuchid",
	 2.1324, 315,	-15,	"Capricornid",
	 2.1991, 339,	-17,	"Delta aquarid",
	 2.2158, 340,	-30,	"Pisces australid",
	 2.4331, 46,	58,	"Perseid",
	-2.6578, 95,	15,	"Orionid",
	-1.8678, 15,	-55,	"Phoenicid",
	-1.7260, 113,	32,	"Geminid",
	0
};

void
search(void)
{
	Obj2 *p, *q;
	int i, j;
	double t;

	for(i=0; objlst[i]; i++) {
		p = objlst[i];
		if(p == &oshad)
			continue;
		t = rise(p, -.833);
		if(t >= 0.)
			event("%s rises at ", p->name, "", t,
				i==0? PTIME: PTIME|DARK);
		t = set(p, -.833);
		if(t >= 0.)
			event("%s sets at ", p->name, "", t,
				i==0? PTIME: PTIME|DARK);
		if(p == &osun) {
			for(j=0; j<4; j++) {
				t = solstice(j);
				if(t >= 0)
					event("%s at ", solstr[j], "", t,
						SIGNIF|PTIME);
			}
			for(j=0; bettab[j].beta!=0; j++) {
				t = betcross(bettab[j].beta);
				if(t >= 0)
					event("%s  meeteeor shouwer",
					bettab[j].betstr, "", t, SIGNIF);
			}
			t = rise(p, -18);
			if(t >= 0)
				event("Twilight starts at ", "", "", t, PTIME);
			t = set(p, -18);
			if(t >= 0)
				event("Twilight ends at ", "", "", t, PTIME);
		}
		if(p == &omoon)
		for(j=0; j<NPTS; j++) {
			if(p->point[j].mag > .75 && p->point[j+1].mag < .25)
				event("New moon", "", "", 0, 0);
			if(p->point[j].mag <= .25 && p->point[j+1].mag > .25)
				event("First quarter moon", "", "", 0, 0);
			if(p->point[j].mag <= .50 && p->point[j+1].mag > .50)
				event("Full moon", "", "", 0, 0);
			if(p->point[j].mag <= .75 && p->point[j+1].mag > .75)
				event("Last quarter moon", "", "", 0, 0);
		}
		if(p == &omerc || p == &ovenus) {
			t = melong(p);
			if(t >= 0) {
				t = rise(p, 0) - rise(&osun, 0);
				if(t < 0)
					t += NPTS;
				if(t > NPTS)
					t -= NPTS;
				if(t > NPTS/2)
				event("Morning elongation of %s", p->name,
					"", 0, SIGNIF);
				else
				event("Evening elongation of %s", p->name,
					"", 0, SIGNIF);
			}
		}
		for(j=i; objlst[j]; j++) {
			if(i == j)
				continue;
			q = objlst[j];
			if(p == &omoon || q == &omoon) {
				occult(p, q, 0);
				if(occ.t3 < 0)
					continue;
				if(p == &osun || q == &oshad) {
					if(occ.t1 >= 0)
					event("Partial eclipse of %s begins at ", p->name, "",
						occ.t1, SIGNIF|PTIME);
					if(occ.t2 >= 0)
					event("Total eclipse of %s begins at ", p->name, "",
						occ.t2, SIGNIF|PTIME);
					if(occ.t4 >= 0)
					event("Total eclipse of %s ends at ", p->name, "",
						occ.t4, SIGNIF|PTIME);
					if(occ.t5 >= 0)
					event("Partial eclipse of %s ends at ", p->name, "",
						occ.t5, SIGNIF|PTIME);
				} else {
					if(occ.t1 >= 0)
					event("Occultation of %s begins at ", q->name, "",
						occ.t1, SIGNIF|PTIME);
					if(occ.t5 >= 0)
					event("Occultation of %s ends at ", q->name, "",
						occ.t5, SIGNIF|PTIME);
				}
				continue;
			}
			if(p == &osun) {
				if(q != &omerc && q != &ovenus)
					continue;
				occult(p, q, -1);
				if(occ.t3 >= 0.) {
					if(occ.t1 >= 0)
					event("Transit of %s begins at ", q->name, "",
						occ.t1, SIGNIF|LIGHT|PTIME);
					if(occ.t5 >= 0)
					event("Transit of %s ends at ", q->name, "",
						occ.t5, SIGNIF|LIGHT|PTIME);
				}
				continue;
			}
			t = dist(&p->point[0], &q->point[0]);
			if(t > 5000)
				continue;
			event("%s is in the house of %s",
				p->name, q->name, 0, 0);
		}
	}
	if(flags['o'])
		stars();
	if(flags['a'])
		satels();
	evflush();
}
