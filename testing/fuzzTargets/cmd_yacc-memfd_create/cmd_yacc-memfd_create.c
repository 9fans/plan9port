#define NOPLAN9DEFINES // TODO: why is this needed?
#define _GNU_SOURCE
#include <u.h>
#include <libc.h>
#include <bio.h>

#include "../.getReaderAndWriter_memfd_create.c"
#include "../.readerFildesFromBuf.c"
#include "../.cmd_yacc.c"
