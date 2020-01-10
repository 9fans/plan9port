#define _FILE_OFFSET_BITS 64
#define _LARGEFILE64_SOURCE

#include <utf.h>
#include <fmt.h>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define OREAD		O_RDONLY
#define OWRITE	O_WRONLY

#define	OCEXEC 0
#define	ORCLOSE	0
#define	OTRUNC	0

#define nil ((void*)0)

typedef long long vlong;
typedef unsigned long long uvlong;

#define seek(fd, offset, whence) lseek(fd, offset, whence)
#define create(name, mode, perm) creat(name, perm)
