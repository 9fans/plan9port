#include <u.h>
#include <libc.h>

int
equivip(uchar *a, uchar *b)
{
	int i;

	for(i = 0; i < 4; i++)
		if(a[i] != b[i])
			return 0;
	return 1;
}
