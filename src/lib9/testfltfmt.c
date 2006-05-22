#include <u.h>
#include <libc.h>
#include <stdio.h>

/*
 * try all combination of flags and float conversions
 * with some different widths & precisions
 */

#define Njust 2
#define Nplus 3
#define Nalt 2
#define Nzero 2
#define Nspec 5
#define Nwidth 5
#define Nprec 5

static double fmtvals[] = {
	3.1415925535897932e15,
	3.1415925535897932e14,
	3.1415925535897932e13,
	3.1415925535897932e12,
	3.1415925535897932e11,
	3.1415925535897932e10,
	3.1415925535897932e9,
	3.1415925535897932e8,
	3.1415925535897932e7,
	3.1415925535897932e6,
	3.1415925535897932e5,
	3.1415925535897932e4,
	3.1415925535897932e3,
	3.1415925535897932e2,
	3.1415925535897932e1,
	3.1415925535897932e0,
	3.1415925535897932e-1,
	3.1415925535897932e-2,
	3.1415925535897932e-3,
	3.1415925535897932e-4,
	3.1415925535897932e-5,
	3.1415925535897932e-6,
	3.1415925535897932e-7,
	3.1415925535897932e-8,
	3.1415925535897932e-9,
	3.1415925535897932e-10,
	3.1415925535897932e-11,
	3.1415925535897932e-12,
	3.1415925535897932e-13,
	3.1415925535897932e-14,
	3.1415925535897932e-15,
};

/*
 * are the numbers close?
 * used to compare long numbers where the last few digits are garbage
 * due to precision problems
 */
static int
numclose(char *num1, char *num2)
{
	int ndig;
	double d1, d2;
	enum { MAXDIG = 15 };

	d1 = fmtstrtod(num1, 0);
	d2 = fmtstrtod(num2, 0);
	if(d1 != d2)
		return 0;

	ndig = 0;
	while (*num1) {
		if (*num1 >= '0' && *num1 <= '9') {
			ndig++;
			if (ndig > MAXDIG) {
				if (!(*num2 >= '0' && *num2 <= '9')) {
					return 0;
				}
			} else if (*num1 != *num2) {
				return 0;
			}
		} else if (*num1 != *num2) {
			return 0;
		} else if (*num1 == 'e' || *num1 == 'E') {
			ndig = 0;
		}
		num1++;
		num2++;
	}
	if (*num1 || !num2)
		return 0;
	return 1;
}

static void
doit(int just, int plus, int alt, int zero, int width, int prec, int spec)
{
	char format[256];
	char *p;
	const char *s;
	int i;

	p = format;
	*p++ = '%';
	if (just > 0)
		*p++ = "-"[just - 1];
	if (plus > 0)
		*p++ = "+ "[plus - 1];
	if (alt > 0)
		*p++ = "#"[alt - 1];
	if (zero > 0)
		*p++ = "0"[zero - 1];

	s = "";
	switch (width) {
	case 1: s = "1"; break;
	case 2: s = "5"; break;
	case 3: s = "10"; break;
	case 4: s = "15"; break;
	}
	strcpy(p, s);

	s = "";
	switch (prec) {
	case 1: s = ".0"; break;
	case 2: s = ".2"; break;
	case 3: s = ".5"; break;
	case 4: s = ".15"; break;
	}
	strcat(p, s);

	p = strchr(p, '\0');
	*p++ = "efgEG"[spec];
	*p = '\0';

	for (i = 0; i < sizeof(fmtvals) / sizeof(fmtvals[0]); i++) {
		char ref[1024], buf[1024];
		Rune rbuf[1024];
		double d1, d2;

		sprintf(ref, format, fmtvals[i]);
		snprint(buf, sizeof(buf), format, fmtvals[i]);
		if (strcmp(ref, buf) != 0
		&& !numclose(ref, buf)) {
			d1 = fmtstrtod(ref, 0);
			d2 = fmtstrtod(buf, 0);
			fprintf(stderr, "%s: ref='%s'%s fmt='%s'%s\n", 
				format, 
				ref, d1==fmtvals[i] ? "" : " (ref is inexact!)", 
				buf, d2==fmtvals[i] ? "" : " (fmt is inexact!)");
		//	exits("oops");
		}

		/* Check again with output to rune string */
		runesnprint(rbuf, 1024, format, fmtvals[i]);
		snprint(buf, sizeof(buf), "%S", rbuf);
		if (strcmp(ref, buf) != 0
		&& !numclose(ref, buf)) {
			d1 = fmtstrtod(ref, 0);
			d2 = fmtstrtod(buf, 0);
			fprintf(stderr, "%s: ref='%s'%s fmt='%s'%s\n", 
				format, 
				ref, d1==fmtvals[i] ? "" : " (ref is inexact!)", 
				buf, d2==fmtvals[i] ? "" : " (fmt is inexact!)");
		//	exits("oops");
		}
	}
}

void
main(int argc, char **argv)
{
	int just, plus, alt, zero, width, prec, spec;

	for (just = 0; just < Njust; just++)
	for (plus = 0; plus < Nplus; plus++)
	for (alt = 0; alt < Nalt; alt++)
	for (zero = 0; zero < Nzero; zero++)
	for (width = 0; width < Nwidth; width++)
	for (prec = 0; prec < Nprec; prec++)
	for (spec = 0; spec < Nspec; spec++)
		doit(just, plus, alt, zero, width, prec, spec);

	exits(0);
}
