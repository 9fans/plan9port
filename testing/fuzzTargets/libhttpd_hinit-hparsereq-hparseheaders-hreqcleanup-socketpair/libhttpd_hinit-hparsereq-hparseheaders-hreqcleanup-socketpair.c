#define NOPLAN9DEFINES // TODO: why is this needed?
#define _GNU_SOURCE
#include <u.h>
#include <libc.h>
#include <httpd.h>

#include "../.getReaderAndWriter_socketpair.c"
#include "../.readerFildesFromBuf.c"
#include "../.libhttpd_hinit-hparsereq-hparseheaders-hreqcleanup.c"
