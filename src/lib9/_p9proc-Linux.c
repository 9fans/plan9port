#ifdef __Linux26__
#include "_p9proc-pthread.c"
#else
#include "_p9proc-getpid.c"
#endif
