#define NOPLAN9DEFINES
#include <u.h>
#include <libc.h>
#include <pthread.h>

extern int __isthreaded;
int
ffork(int flags, void(*fn)(void*), void *arg)
{
	void *p;
	pthread_t tid;

	if(flags != (RFMEM|RFNOWAIT)){
		werrstr("ffork unsupported");
		return -1;
	}

	if(pthread_create(&tid, NULL, (void*(*)(void*))fn, arg) < 0)
		return -1;
	return (int)tid;
}

int
getfforkid(void)
{
	return (int)pthread_self();
}

