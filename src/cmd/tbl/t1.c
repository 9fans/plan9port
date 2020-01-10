/* t1.c: main control and input switching */
#
# include "t.h"

# define MACROS "/usr/lib/tmac.s"
# define PYMACS "/usr/lib/tmac.m"


# define ever (;;)

void
main(int argc, char *argv[])
{
	exits(tbl(argc, argv)? "error" : 0);
}


int
tbl(int argc, char *argv[])
{
	char	line[5120];
	/*int x;*/
	/*x=malloc((char *)0);	uncomment when allocation breaks*/
	Binit(&tabout, 1, OWRITE);
	setinp(argc, argv);
	while (gets1(line, sizeof(line))) {
		Bprint(&tabout, "%s\n", line);
		if (prefix(".TS", line))
			tableput();
	}
	Bterm(tabin);
	return(0);
}


int	sargc;
char	**sargv;

void
setinp(int argc, char **argv)
{
	sargc = argc;
	sargv = argv;
	sargc--;
	sargv++;
	if (sargc > 0)
		swapin();
	else {
		tabin = (Biobuf*)getcore(sizeof(Biobuf), 1);
		Binit(tabin, 0, OREAD);
	}
}


int
swapin(void)
{
	char	*name;
	while (sargc > 0 && **sargv == '-') {
		if (match("-ms", *sargv)) {
			*sargv = MACROS;
			break;
		}
		if (match("-mm", *sargv)) {
			*sargv = PYMACS;
			break;
		}
		if (match("-TX", *sargv))
			pr1403 = 1;
		if (match("-", *sargv))
			break;
		sargc--;
		sargv++;
	}
	if (sargc <= 0)
		return(0);
	/* file closing is done by GCOS troff preprocessor */
	if(tabin)
		Bterm(tabin);
	ifile = *sargv;
	name = ifile;
	if (match(ifile, "-")) {
		tabin = (Biobuf*)getcore(sizeof(Biobuf), 1);
		Binit(tabin, 0, OREAD);
	} else
		tabin = Bopen(ifile, OREAD);
	iline = 1;
	Bprint(&tabout, ".ds f. %s\n", ifile);
	Bprint(&tabout, ".lf %d %s\n", iline, name);
	if (tabin == 0)
		error("Can't open file");
	sargc--;
	sargv++;
	return(1);
}
