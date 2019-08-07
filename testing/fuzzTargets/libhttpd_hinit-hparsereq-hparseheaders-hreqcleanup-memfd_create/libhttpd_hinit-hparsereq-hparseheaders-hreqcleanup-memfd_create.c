#define _GNU_SOURCE
#include <u.h>
#include <libc.h>
#include <httpd.h>

#include "../.getReaderAndWriter_memfd_create.c"
#include "../.readerFildesFromBuf.c"
#include "../.libhttpd_hinit-hparsereq-hparseheaders-hreqcleanup.c"
