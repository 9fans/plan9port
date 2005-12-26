#include <u.h>
#include <libc.h>
#include <ip.h>
#include <bio.h>
#include <ndb.h>
#include "dat.h"

char	*blog = "ipboot";

void
main(int argc, char **argv)
{
	fmtinstall('E', eipconv);
	fmtinstall('I', eipconv);

	if(argc < 2)
		exits(0);
	if(icmpecho(argv[1]))
		fprint(2, "%s live\n", argv[1]);
	else
		fprint(2, "%s doesn't answer\n", argv[1]);
}
