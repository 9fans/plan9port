#include <pthread.h>
#include <utf.h>
#include <fmt.h>

pthread_key_t key;

void
pexit(void *v)
{
	int s;

	pthread_setspecific(key, (void*)1);
	switch(fork()){
	case -1:
		fprint(2, "fork: %r\n");
	case 0:
		_exit(0);
	default:
		wait(&s);
	}
	pthread_exit(0);
}

int
main(int argc, char *argv[])
{
	int i;
	pthread_t pid;

	pthread_key_create(&key, 0);
	for(i=0;; i++){
		print("%d\n", i);
		if(pthread_create(&pid, 0, pexit, 0) < 0){
			fprint(2, "pthread_create: %r\n");
			abort();
		}
	}
}
