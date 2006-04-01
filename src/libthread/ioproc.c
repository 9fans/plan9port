#include <u.h>
#include <libc.h>
#include <thread.h>
#include "ioproc.h"

enum
{
	STACK = 32768
};

void
iointerrupt(Ioproc *io)
{
	if(!io->inuse)
		return;
	fprint(2, "bug: cannot iointerrupt %p yet\n", io);
}

static void
xioproc(void *a)
{
	Ioproc *io, *x;

	threadsetname("ioproc");
	io = a;
	/*
	 * first recvp acquires the ioproc.
	 * second tells us that the data is ready.
	 */
	for(;;){
		while(recv(io->c, &x) == -1)
			;
		if(x == 0)	/* our cue to leave */
			break;
		assert(x == io);

		/* caller is now committed -- even if interrupted he'll return */
		while(recv(io->creply, &x) == -1)
			;
		if(x == 0)	/* caller backed out */
			continue;
		assert(x == io);

		io->ret = io->op(&io->arg);
		if(io->ret < 0)
			rerrstr(io->err, sizeof io->err);
		while(send(io->creply, &io) == -1)
			;
		while(recv(io->creply, &x) == -1)
			;
	}
}

Ioproc*
ioproc(void)
{
	Ioproc *io;

	io = mallocz(sizeof(*io), 1);
	if(io == nil)
		sysfatal("ioproc malloc: %r");
	io->c = chancreate(sizeof(void*), 0);
	chansetname(io->c, "ioc%p", io->c);
	io->creply = chancreate(sizeof(void*), 0);
	chansetname(io->creply, "ior%p", io->c);
	io->tid = proccreate(xioproc, io, STACK);
	return io;
}

void
closeioproc(Ioproc *io)
{
	if(io == nil)
		return;
	iointerrupt(io);
	while(send(io->c, 0) == -1)
		;
	chanfree(io->c);
	chanfree(io->creply);
	free(io);
}

long
iocall(Ioproc *io, long (*op)(va_list*), ...)
{
	char e[ERRMAX];
	int ret, inted;
	Ioproc *msg;

	if(send(io->c, &io) == -1){
		werrstr("interrupted");
		return -1;
	}
	assert(!io->inuse);
	io->inuse = 1;
	io->op = op;
	va_start(io->arg, op);
	msg = io;
	inted = 0;
	while(send(io->creply, &msg) == -1){
		msg = nil;
		inted = 1;
	}
	if(inted){
		werrstr("interrupted");
		return -1;
	}

	/*
	 * If we get interrupted, we have stick around so that
	 * the IO proc has someone to talk to.  Send it an interrupt
	 * and try again.
	 */
	inted = 0;
	while(recv(io->creply, nil) == -1){
		inted = 1;
		iointerrupt(io);
	}
	USED(inted);
	va_end(io->arg);
	ret = io->ret;
	if(ret < 0)
		strecpy(e, e+sizeof e, io->err);
	io->inuse = 0;

	/* release resources */
	while(send(io->creply, &io) == -1)
		;
	if(ret < 0)
		errstr(e, sizeof e);
	return ret;
}
