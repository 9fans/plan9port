typedef struct Ureg Ureg;

struct Ureg
{
/*  0*/	ulong	cause;
/*  4*/	ulong	srr1;	/* aka status */
/*  8*/	ulong	pc;	/* SRR0 */
/* 12*/	ulong	pad;
/* 16*/	ulong	lr;
/* 20*/	ulong	cr;
/* 24*/	ulong	xer;
/* 28*/	ulong	ctr;
/* 32*/	ulong	r0;
/* 36*/	ulong	r1;	/* aka sp */
/* 40*/	ulong	r2;
/* 44*/	ulong	r3;
/* 48*/	ulong	r4;
/* 52*/	ulong	r5;
/* 56*/	ulong	r6;
/* 60*/	ulong	r7;
/* 64*/	ulong	r8;
/* 68*/	ulong	r9;
/* 72*/	ulong	r10;
/* 76*/	ulong	r11;
/* 80*/	ulong	r12;
/* 84*/	ulong	r13;
/* 88*/	ulong	r14;
/* 92*/	ulong	r15;
/* 96*/	ulong	r16;
/*100*/	ulong	r17;
/*104*/	ulong	r18;
/*108*/	ulong	r19;
/*112*/	ulong	r20;
/*116*/	ulong	r21;
/*120*/	ulong	r22;
/*124*/	ulong	r23;
/*128*/	ulong	r24;
/*132*/	ulong	r25;
/*136*/	ulong	r26;
/*140*/	ulong	r27;
/*144*/	ulong	r28;
/*148*/	ulong	r29;
/*152*/	ulong	r30;
/*156*/	ulong	r31;
/*160*/	ulong	dcmp;
/*164*/	ulong	icmp;
/*168*/	ulong	dmiss;
/*172*/	ulong	imiss;
/*176*/	ulong	hash1;
/*180*/	ulong	hash2;
/*184*/	ulong	dar;
/*188*/	ulong	dsisr;
/*192*/	ulong	vrsave;
};
