#include <u.h>
#include <libc.h>

int
exec(char *prog, char *argv[])
{
	/* to mimic plan 9 should be just exec, but execvp is a better fit for unix */
	return execvp(prog, argv);
}
