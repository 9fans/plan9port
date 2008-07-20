#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include "dat.h"
#include "fns.h"

char	Enonexist[] =	"file does not exist";
char	Eperm[] =	"permission denied";
char	Enofile[] =	"no file system specified";
char	Eauth[] =	"authentication failed";

char	*srvname = "9660";
char	*deffile = 0;

extern Xfsub	isosub;

Xfsub*	xsublist[] =
{
	&isosub,
	0
};
