#include <u.h>
#include <libc.h>
#include <bio.h>

#define	whsiz	(sizeof(wheel)/sizeof(wheel[0]))

double	wheel[] =
{
	 2,10, 2, 4, 2, 4, 6, 2, 6, 4,
	 2, 4, 6, 6, 2, 6, 4, 2, 6, 4,
	 6, 8, 4, 2, 4, 2, 4, 8, 6, 4,
	 6, 2, 4, 6, 2, 6, 6, 4, 2, 4,
	 6, 2, 6, 4, 2, 4, 2,10,
};

Biobuf	bin;

void	factor(double);

void
main(int argc, char *argv[])
{
	double n;
	int i;
	char *l;

	if(argc > 1) {
		for(i=1; i<argc; i++) {
			n = atof(argv[i]);
			factor(n);
		}
		exits(0);
	}

	Binit(&bin, 0, OREAD);
	for(;;) {
		l = Brdline(&bin, '\n');
		if(l ==  0)
			break;
		n = atof(l);
		if(n <= 0)
			break;
		factor(n);
	}
	exits(0);
}

void
factor(double n)
{
	double quot, d, s;
	int i;

	print("%.0f\n", n);
	if(n == 0)
		return;
	s = sqrt(n) + 1;
	while(modf(n/2, &quot) == 0) {
		print("     2\n");
		n = quot;
		s = sqrt(n) + 1;
	}
	while(modf(n/3, &quot) == 0) {
		print("     3\n");
		n = quot;
		s = sqrt(n) + 1;
	}
	while(modf(n/5, &quot) == 0) {
		print("     5\n");
		n = quot;
		s = sqrt(n) + 1;
	}
	while(modf(n/7, &quot) == 0) {
		print("     7\n");
		n = quot;
		s = sqrt(n) + 1;
	}
	d = 1;
	for(i=1;;) {
		d += wheel[i];
		while(modf(n/d, &quot) == 0) {
			print("     %.0f\n", d);
			n = quot;
			s = sqrt(n) + 1;
		}
		i++;
		if(i >= whsiz) {
			i = 0;
			if(d > s)
				break;
		}
	}
	if(n > 1)
		print("     %.0f\n",n);
	print("\n");
}
