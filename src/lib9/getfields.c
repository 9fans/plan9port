#include <lib9.h>

int
getfields(char *str, char **args, int max, int mflag, char *set)
{
	Rune r;
	int nr, intok, narg;

	if(max <= 0)
		return 0;

	narg = 0;
	args[narg] = str;
	if(!mflag)
		narg++;
	intok = 0;
	for(;; str += nr) {
		nr = chartorune(&r, str);
		if(r == 0)
			break;
		if(utfrune(set, r)) {
			if(narg >= max)
				break;
			*str = 0;
			intok = 0;
			args[narg] = str + nr;
			if(!mflag)
				narg++;
		} else {
			if(!intok && mflag)
				narg++;
			intok = 1;
		}
	}
	return narg;
}
