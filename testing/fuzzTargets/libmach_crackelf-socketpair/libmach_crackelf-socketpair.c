#define NOPLAN9DEFINES // TODO: why is this needed?
#define _GNU_SOURCE
#include <u.h>
#include <libc.h>
#include <mach.h>

#include "../.getReaderAndWriter_socketpair.c"
#include "../.readerFildesFromBuf.c"
#define CRACK crackelf
#include "../.libmach_crack.c"
