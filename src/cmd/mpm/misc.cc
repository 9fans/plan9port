#include	"misc.h"

char	errbuf[200];
char	*progname;
int	wantwarn = 0;

int	dbg	= 0;
// dbg = 1 : dump slugs
// dbg = 2 : dump ranges
// dbg = 4 : report function entry
// dbg = 8 : follow queue progress
// dbg = 16: follow page fill progress
