#include "threadimpl.h"
extern int getfforkid(void);
int
_threadgetpid(void)
{
	return getfforkid();
}
