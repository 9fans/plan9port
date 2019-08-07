#define NOPLAN9DEFINES // TODO: why is this needed?
#define _GNU_SOURCE
#include <u.h>
#include <libc.h>
#include <mach.h>

#include "../.getReaderAndWriter_memfd_create.c"
#include "../.readerFildesFromBuf.c"
#define CRACK crackmacho
#include "../.libmach_crack.c"
