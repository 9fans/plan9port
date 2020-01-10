#include <u.h>
#include <libc.h>
#include <mach.h>
#include "macho.h"
#include "uregpower.h"

enum
{
	ThreadState = 1,
	FloatState,
	ExceptionState,
	VectorState,
	ThreadState64,
	ExceptionState64,
	ThreadStateNone
};

typedef struct Lreg Lreg;
typedef struct Lflt Lflt;
typedef struct Lexc Lexc;

struct Lreg
{
	u32int srr0;
	u32int srr1;
	u32int r0;
	u32int r1;
	u32int r2;
	u32int r3;
	u32int r4;
	u32int r5;
	u32int r6;
	u32int r7;
	u32int r8;
	u32int r9;
	u32int r10;
	u32int r11;
	u32int r12;
	u32int r13;
	u32int r14;
	u32int r15;
	u32int r16;
	u32int r17;
	u32int r18;
	u32int r19;
	u32int r20;
	u32int r21;
	u32int r22;
	u32int r23;
	u32int r24;
	u32int r25;
	u32int r26;
	u32int r27;
	u32int r28;
	u32int r29;
	u32int r30;
	u32int r31;

	u32int cr;
	u32int xer;
	u32int lr;
	u32int ctr;
	u32int mq;

	u32int vrsave;
};

struct Lflt
{
	u32int fpregs[32*2];		/* 32 doubles */
	u32int fpscr[2];

};

struct Lexc
{
	u32int dar;
	u32int dsisr;
	u32int exception;
	u32int pad0;
	u32int pad1[4];
};

static void
lreg2ureg(Lreg *l, Ureg *u)
{
	u->pc = l->srr0;
	u->srr1 = l->srr1;
	u->lr = l->lr;
	u->cr = l->cr;
	u->xer = l->xer;
	u->ctr = l->ctr;
	u->vrsave = l->vrsave;
	memmove(&u->r0, &l->r0, 32*4);
}

static void
lexc2ureg(Lexc *l, Ureg *u)
{
	u->cause = l->exception;
	u->dar = l->dar;
	u->dsisr = l->dsisr;
}

static uchar*
load(int fd, ulong off, int size)
{
	uchar *a;

	a = malloc(size);
	if(a == nil)
		return nil;
	if(seek(fd, off, 0) < 0 || readn(fd, a, size) != size){
		free(a);
		return nil;
	}
	return a;
}

int
coreregsmachopower(Macho *m, uchar **up)
{
	int i, havereg, haveexc;
	uchar *a, *p, *nextp;
	Ureg *u;
	ulong flavor, count;
	MachoCmd *c;

	*up = nil;
	for(i=0; i<m->ncmd; i++)
		if(m->cmd[i].type == MachoCmdThread)
			break;
	if(i == m->ncmd){
		werrstr("no registers found");
		return -1;
	}

	c = &m->cmd[i];
	a = load(m->fd, c->off, c->size);
	if(a == nil)
		return -1;

	if((u = mallocz(sizeof(Ureg), 1)) == nil){
		free(a);
		return -1;
	}

	havereg = haveexc = 0;
	for(p=a+8; p<a+c->size; p=nextp){
		flavor = m->e4(p);
		count = m->e4(p+4);
		nextp = p+8+count*4;
		if(flavor == ThreadState && count*4 == sizeof(Lreg)){
			havereg = 1;
			lreg2ureg((Lreg*)(p+8), u);
		}
		if(flavor == ExceptionState && count*4 == sizeof(Lexc)){
			haveexc = 1;
			lexc2ureg((Lexc*)(p+8), u);
		}
	}
	free(a);
	if(!havereg){
		werrstr("no registers found");
		free(u);
		return -1;
	}
	if(!haveexc)
		fprint(2, "warning: no exception state in core file registers\n");
	*up = (uchar*)u;
	return sizeof(*u);
}
