#ifdef __Linux26__
#include "lock-pthread.c"
#else
#include "lock-tas.c"
#endif
