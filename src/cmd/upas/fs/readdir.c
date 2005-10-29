#include <u.h>
#include <libc.h>

void
main(void)
{
	Dir d;
	int fd, n;

	fd = open("/mail/fs", OREAD);
	while((n = dirread(fd, &d, sizeof(d))) > 0){
		print("%s\n", d.name);
	}
	print("n = %d\n", n);
}
