#define __USE_UNIX98  // for pread/pwrite, supposedly
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

#include "utf.h"
#include "fmt.h"

#define nil 0
#define dup dup2
#define exec execv
#define seek lseek
#define getwd getcwd
#define USED(a)
#define SET(a)

enum {
	OREAD = 0,
	OWRITE = 1,
	ORDWR = 2,
	OCEXEC = 4,
	ORCLOSE = 8
};

enum {
	ERRMAX = 255
};

void exits(const char *);
void _exits(const char *);
int notify (void(*f)(void *, char *));
int create(char *, int, int);
int errstr(char *, int);
