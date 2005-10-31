#include <u.h>
#include <libc.h>
#include <draw.h>
#include <plumb.h>
#include <regexp.h>
#include <bio.h>
#include <9pclient.h>
#include <thread.h>
#include "faces.h"

void
threadmain(int argc, char **argv)
{
	Face f;
	char *q;

	if(argc != 3){
		fprint(2, "usage: dblook name domain\n");
		threadexitsall("usage");
	}

	q = findfile(&f, argv[2], argv[1]);
	print("%s\n", q);
}

void
killall(char *s)
{
	USED(s);
}
