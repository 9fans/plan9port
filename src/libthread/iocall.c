#include "threadimpl.h"

long
iocall(Ioproc *io, long (*op)(va_list*), ...)
{
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
		errstr(io->err, sizeof io->err);
	io->inuse = 0;

	/* release resources */
	while(send(io->creply, &io) == -1)
		;
	return ret;
}
