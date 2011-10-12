/*
 * I am too ignorant to know if Cocoa and Libthread
 * can coexist: if I try to include thread.h, now
 * that Devdraw uses Cocoa's threads (and timers), it
 * crashes immediately; when Devdraw was using
 * proccreate(), it could run a little while before to
 * crash; the origin of those crashes is hard to
 * ascertain, because other programs using Libthread
 * (such as 9term, Acme, Plumber, and Sam) currently
 * don't run when compiled with Xcode 4.1.
 */
//#define TRY_LIBTHREAD

#ifdef TRY_LIBTHREAD
	#include <thread.h>
#else
	#define QLock DQLock
	#define qlock dqlock
	#define qunlock dqunlock
	#define threadexitsall exits
	#define threadmain main

	typedef struct QLock QLock;

	struct QLock
	{
		int init;
		pthread_mutex_t m;
	};

	void	qlock(QLock*);
	void	qunlock(QLock*);
#endif
