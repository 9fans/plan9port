#include <stdarg.h>
#include <utf.h>
#include <fmt.h>

int
main(int argc, char **argv)
{
	print("%020.10d\n", 100);
}
