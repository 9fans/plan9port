#ifdef __Linux26__
#include "rendez-pthread.c"
#else
#include "rendez-signal.c"
#endif
