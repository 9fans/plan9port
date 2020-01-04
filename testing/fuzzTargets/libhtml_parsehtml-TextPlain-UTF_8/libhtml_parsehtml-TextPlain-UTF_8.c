#include <u.h>
#include <libc.h>
#include <draw.h>
#include <html.h>

enum {
	mType = TextPlain,
	chSet = UTF_8
};

#include "../.libhtml_parsehtml.c"
