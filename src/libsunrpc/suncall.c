#include <u.h>
#include <libc.h>
#include <thread.h>
#include <sunrpc.h>

void
suncallsetup(SunCall *c, SunProg *prog, uint proc)
{
	c->rpc.prog = prog->prog;
	c->rpc.vers = prog->vers;
	c->rpc.proc = proc>>1;
	c->rpc.iscall = !(proc&1);
	c->type = proc;
}
