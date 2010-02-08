#ifndef _FLATE_H_
#define _FLATE_H_ 1
#if defined(__cplusplus)
extern "C" { 
#endif

AUTOLIB(flate)
/*
#pragma	lib	"libflate.a"
#pragma	src	"/sys/src/libflate"
*/

/*
 * errors from deflate, deflateinit, deflateblock,
 * inflate, inflateinit, inflateblock.
 * convertable to a string by flateerr
 */
enum
{
	FlateOk			= 0,
	FlateNoMem		= -1,
	FlateInputFail		= -2,
	FlateOutputFail		= -3,
	FlateCorrupted		= -4,
	FlateInternal		= -5
};

int	deflateinit(void);
int	deflate(void *wr, int (*w)(void*, void*, int), void *rr, int (*r)(void*, void*, int), int level, int debug);

int	inflateinit(void);
int	inflate(void *wr, int (*w)(void*, void*, int), void *getr, int (*get)(void*));

int	inflateblock(uchar *dst, int dsize, uchar *src, int ssize);
int	deflateblock(uchar *dst, int dsize, uchar *src, int ssize, int level, int debug);

int	deflatezlib(void *wr, int (*w)(void*, void*, int), void *rr, int (*r)(void*, void*, int), int level, int debug);
int	inflatezlib(void *wr, int (*w)(void*, void*, int), void *getr, int (*get)(void*));

int	inflatezlibblock(uchar *dst, int dsize, uchar *src, int ssize);
int	deflatezlibblock(uchar *dst, int dsize, uchar *src, int ssize, int level, int debug);

char	*flateerr(int err);

uint32	*mkcrctab(uint32);
uint32	blockcrc(uint32 *tab, uint32 crc, void *buf, int n);

uint32	adler32(uint32 adler, void *buf, int n);
#if defined(__cplusplus)
}
#endif
#endif
