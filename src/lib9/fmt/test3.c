#include <u.h>
#include <libc.h>
#include <stdio.h>

void
test(char *fmt, ...)
{
	va_list arg;
	char fmtbuf[100], stdbuf[100];

	va_start(arg, fmt);
	vsnprint(fmtbuf, sizeof fmtbuf, fmt, arg);
	va_end(arg);

	va_start(arg, fmt);
	vsnprint(stdbuf, sizeof stdbuf, fmt, arg);
	va_end(arg);

	if(strcmp(fmtbuf, stdbuf) != 0)
		print("fmt %s: fmt=\"%s\" std=\"%s\"\n", fmt, fmtbuf, stdbuf);

	print("fmt %s: %s\n", fmt, fmtbuf);
}


int
main(int argc, char *argv[])
{
	test("%f", 3.14159);
	test("%f", 3.14159e10);
	test("%f", 3.14159e-10);

	test("%e", 3.14159);
	test("%e", 3.14159e10);
	test("%e", 3.14159e-10);

	test("%g", 3.14159);
	test("%g", 3.14159e10);
	test("%g", 3.14159e-10);

	test("%g", 2e25);
	test("%.18g", 2e25);

	test("%2.18g", 1.0);
	test("%2.18f", 1.0);
	test("%f", 3.1415927/4);

	test("%20.10d", 12345);
	test("%0.10d", 12345);

	return 0;
}
