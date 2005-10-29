#include <u.h>
#include <libc.h>
#include <regexp.h>
#include <bio.h>

main(void)
{
	char *re;
	char *line;
	Reprog *prog;
	char *cp;
	Biobuf in;

	Binit(&in, 0, OREAD);
	print("re> ");
	while(re = Brdline(&in, '\n')){
		re[Blinelen(&in)-1] = 0;
		if(*re == 0)
			break;
		prog = regcomp(re);
		print("> ");
		while(line = Brdline(&in, '\n')){
			line[Blinelen(&in)-1] = 0;
			if(cp = strchr(line, '\n'))
				*cp = 0;
			if(*line == 0)
				break;
			if(regexec(prog, line, 0))
				print("yes\n");
			else
				print("no\n");
			print("> ");
		}
		print("re> ");
	}
}
