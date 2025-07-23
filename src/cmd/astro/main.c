#include "astro.h"

double	converge;

char	flags[128];
int	nperiods;
double	wlong, awlong, nlat, elev;
double	obliq, phi, eps, tobliq;
double	dphi, deps;
double	day, deld, per;
double	eday, capt, capt2, capt3, gst;
double	pi, pipi, radian, radsec, deltat;
double	erad, glat;
double	xms, yms, zms;
double	xdot, ydot, zdot;

double	ecc, incl, node, argp, mrad, anom, motion;

double	lambda, beta, rad, mag, semi;
double	alpha, delta, rp, hp;
double	ra, decl, semi2;
double	lha, decl2, lmb2;
double	az, el;

double	meday, seday, mhp, salph, sdelt, srad;

double*	cafp;
char*	cacp;

double	rah, ram, ras, dday, dmin, dsec;
long	sao;
double	da, dd, px, epoch;
char	line[100];
Obj2	osun;
Obj2	omoon;
Obj2	oshad;
Obj2	omerc;
Obj2	ovenus;
Obj2	omars;
Obj2	osat;
Obj2	ouran;
Obj2	onept;
Obj2	oplut;
Obj2	ojup;
Obj2	ostar;
Obj2	ocomet;
Obj3	occ;
Obj2*	eobj1;
Obj2*	eobj2;

char*	herefile;

int
main(int argc, char *argv[])
{
	int i, j;
	double d;

	pi = atan(1.0)*4;
	pipi = pi*2;
	radian = pi/180;
	radsec = radian/3600;
	converge = 1.0e-14;

	startab = unsharp("#9/sky/estartab");
	herefile = unsharp("#9/sky/here");

	fmtinstall('R', Rconv);
	fmtinstall('D', Dconv);

	per = PER;
	deld = PER/NPTS;
	init();
	args(argc, argv);
	init();

loop:
	d = day;
	pdate(d);
	if(flags['p'] || flags['e']) {
		print(" ");
		ptime(d);
		pstime(d);
	}
	print("\n");
	for(i=0; i<=NPTS+1; i++) {
		setime(d);

		for(j=0; objlst[j]; j++) {
			(*objlst[j]->obj)();
			setobj(&objlst[j]->point[i]);
			if(flags['p']) {
				if(flags['m'])
					if(strcmp(objlst[j]->name, "Comet"))
						continue;
				output(objlst[j]->name, &objlst[j]->point[i]);
			}
		}
		if(flags['e']) {
			d = dist(&eobj1->point[i], &eobj2->point[i]);
			print("dist %s to %s = %.4f\n", eobj1->name, eobj2->name, d);
		}
/*		if(flags['p']) { */
/*			pdate(d); */
/*			print(" "); */
/*			ptime(d); */
/*			print("\n"); */
/*		} */
		if(flags['p'] || flags['e'])
			break;
		d += deld;
	}
	if(!(flags['p'] || flags['e']))
		search();
	day += per;
	nperiods -= 1;
	if(nperiods > 0)
		goto loop;
	exits(0);
	return 0;		/* gcc */
}

void
args(int argc, char *argv[])
{
	char *p;
	long t;
	int f, i;
	Obj2 *q;

	memset(flags, 0, sizeof(flags));
	ARGBEGIN {
	default:
		fprint(2, "astro [-adeklmopst] [-c nperiod] [-C tperiod]\n");
		exits(0);

	case 'c':
		nperiods = 1;
		p = ARGF();
		if(p)
			nperiods = atol(p);
		flags['c']++;
		break;
	case 'C':
		p = ARGF();
		if(p)
			per = atof(p);
		break;
	case 'e':
		eobj1 = nil;
		eobj2 = nil;
		p = ARGF();
		if(p) {
			for(i=0; q=objlst[i]; i++) {
				if(strcmp(q->name, p) == 0)
					eobj1 = q;
				if(strcmp(q->name1, p) == 0)
					eobj1 = q;
			}
			p = ARGF();
			if(p) {
				for(i=0; q=objlst[i]; i++) {
					if(strcmp(q->name, p) == 0)
						eobj2 = q;
					if(strcmp(q->name1, p) == 0)
						eobj2 = q;
				}
			}
		}
		if(eobj1 && eobj2) {
			flags['e']++;
			break;
		}
		fprint(2, "cant recognize eclipse objects\n");
		exits("eflag");

	case 'a':
	case 'd':
	case 'j':
	case 'k':
	case 'l':
	case 'm':
	case 'o':
	case 'p':
	case 's':
	case 't':
		flags[ARGC()]++;
		break;
	} ARGEND
	if(*argv){
		fprint(2, "usage: astro [-dlpsatokm] [-c nday] [-e obj1 obj2]\n");
		exits("usage");
	}

	t = time(0);
	day = t/86400. + 25567.5;
	if(flags['d'])
		day = readate();
	if(flags['j'])
		print("jday = %.4f\n", day);
	deltat = day * .001704;
	if(deltat > 32.184)		/* assume date is utc1 */
		deltat = 32.184;	/* correct by leap sec */
	if(flags['t'])
		deltat = readdt();

	if(flags['l']) {
		fprint(2, "nlat wlong elev\n");
		readlat(0);
	} else {
		f = open(herefile, OREAD);
		if(f < 0) {
			fprint(2, "%s?\n", herefile);
			/* btl mh */
			nlat = (40 + 41.06/60)*radian;
			awlong = (74 + 23.98/60)*radian;
			elev = 150 * 3.28084;
		} else {
			readlat(f);
			close(f);
		}
	}
}

double
readate(void)
{
	int i;
	Tim t;

	fprint(2, "year mo da hr min\n");
	rline(0);
	for(i=0; i<5; i++)
		t.ifa[i] = atof(skip(i));
	return convdate(&t);
}

double
readdt(void)
{

	fprint(2, "ΔT (sec) (%.3f)\n", deltat);
	rline(0);
	return atof(skip(0));
}

double
etdate(long year, int mo, double day)
{
	Tim t;

	t.ifa[0] = year;
	t.ifa[1] = mo;
	t.ifa[2] = day;
	t.ifa[3] = 0;
	t.ifa[4] = 0;
	return convdate(&t) + 2415020;
}

void
readlat(int f)
{

	rline(f);
	nlat = atof(skip(0)) * radian;
	awlong = atof(skip(1)) * radian;
	elev = atof(skip(2)) * 3.28084;
}

double
fmod(double a, double b)
{
	return a - floor(a/b)*b;
}
