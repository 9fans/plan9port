#ifndef _BIN_H_
#define _BIN_H_ 1
#if defined(__cplusplus)
extern "C" { 
#endif

AUTOLIB(bin)

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

#if defined(__cplusplus)
}
#endif
#endif
