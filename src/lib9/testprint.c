#include <u.h>
#include <libc.h>

void
main(int argc, char **argv)
{
	char c;

	c = argv[1][strlen(argv[1])-1];
	if(c == 'f' || c == 'e' || c == 'g' || c == 'F' || c == 'E' || c == 'G')
		print(argv[1], atof(argv[2]));
	else if(c == 'x' || c == 'u' || c == 'd' || c == 'c' || c == 'C' || c == 'X')
		print(argv[1], atoi(argv[2]));
}
