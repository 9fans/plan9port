#ifdef __Linux26__
#include "ffork-pthread.c"
#else
#include "ffork-Linux-clone.c"
#endif
