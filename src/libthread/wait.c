#include <u.h>
#include <libc.h>
#include <thread.h>

typedef struct Waiter Waiter;

struct {
	QLock lk;
	Waitmsg **msg;
	int nmsg;
	int muxer;
	Waiter *head;
} waiting;

struct Waiter
{
	Rendez r;
	Waitmsg *msg;
	int pid;
	Waiter *next;
	Waiter *prev;
};

/* see src/libmux/mux.c */
Waitmsg*
procwait(int pid)
{
	Waiter *w;
	Waiter me;
	Waitmsg *msg;
	int i;

	memset(&me, 0, sizeof me);
	me.pid = pid;
	me.r.l = &waiting.lk;

	qlock(&waiting.lk);
	for(i=0; i<waiting.nmsg; i++){
		if(waiting.msg[i]->pid == pid){
			msg = waiting.msg[i];
			waiting.msg[i] = waiting.msg[--waiting.nmsg];
			qunlock(&waiting.lk);
			return msg;
		}
	}
	me.next = waiting.head;
	me.prev = nil;
	if(me.next)
		me.next->prev = &me;
	waiting.head = &me;
	while(waiting.muxer && me.msg==nil)
		rsleep(&me.r);

	if(!me.msg){
		if(waiting.muxer)
			abort();
		waiting.muxer = 1;
		while(!me.msg){
			qunlock(&waiting.lk);
			msg = recvp(threadwaitchan());
			qlock(&waiting.lk);
			if(msg == nil)	/* shouldn't happen */
				break;
			for(w=waiting.head; w; w=w->next)
				if(w->pid == msg->pid)
					break;
			if(w){
				if(w->prev)
					w->prev->next = w->next;
				else
					waiting.head = w->next;
				if(w->next)
					w->next->prev = w->prev;
				me.msg = msg;
				rwakeup(&w->r);
			}else{
				waiting.msg = realloc(waiting.msg, (waiting.nmsg+1)*sizeof waiting.msg[0]);
				if(waiting.msg == nil)
					sysfatal("out of memory");
				waiting.msg[waiting.nmsg++] = msg;
			}
		}
		waiting.muxer = 0;
		if(waiting.head)
			rwakeup(&waiting.head->r);
	}
	qunlock(&waiting.lk);
	if (me.msg->pid < 0) {
		free(me.msg);
		me.msg = 0;
	}
	return me.msg;
}
