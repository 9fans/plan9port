/*
#pragma	lib	"libbin.a"
#pragma	src	"/sys/src/libbin"
*/

#ifndef _HAVE_BIN
typedef struct Bin		Bin;
#define _HAVE_BIN
#endif

void	*binalloc(Bin **, ulong size, int zero);
void	*bingrow(Bin **, void *op, ulong osize, ulong size, int zero);
void	binfree(Bin **);
