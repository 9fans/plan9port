.text

.p2align 2,0x90
.globl _sha1block
	.type _sha1block, @function
_sha1block:

/* x = (wp[off-f] ^ wp[off-8] ^ wp[off-14] ^ wp[off-16]) <<< 1;
 * wp[off] = x;
 * x += A <<< 5;
 * E += 0xca62c1d6 + x;
 * x = FN(B,C,D);
 * E += x;
 * B >>> 2
 */
#define BSWAPDI	BYTE $0x0f; BYTE $0xcf;

#define BODY(off,FN,V,A,B,C,D,E)\
	movl (off-64)(%ebp), %edi;\
	xorl (off-56)(%ebp), %edi;\
	xorl (off-32)(%ebp), %edi;\
	xorl (off-12)(%ebp), %edi;\
	roll $1, %edi;\
	movl %edi, off(%ebp);\
	leal V(%edi, E, 1), E;\
	movl A, %edi;\
	roll $5, %edi;\
	addl %edi, E;\
	FN(B,C,D)\
	addl %edi, E;\
	rorl $2, B;\

#define BODY0(off,FN,V,A,B,C,D,E)\
	movl off(%ebx), %edi;\
	bswap %edi;\
	movl %edi, off(%ebp);\
	leal V(%edi,E,1), E;\
	movl A, %edi;\
	roll $5,%edi;\
	addl %edi,E;\
	FN(B,C,D)\
	addl %edi,E;\
	rorl $2,B;\

/*
 * fn1 = (((C^D)&B)^D);
 */
#define FN1(B,C,D)\
	movl C, %edi;\
	xorl D, %edi;\
	andl B, %edi;\
	xorl D, %edi;\

/*
 * fn24 = B ^ C ^ D
 */
#define FN24(B,C,D)\
	movl B, %edi;\
	xorl C, %edi;\
	xorl D, %edi;\

/*
 * fn3 = ((B ^ C) & (D ^= B)) ^ B
 * D ^= B to restore D
 */
#define FN3(B,C,D)\
	movl B, %edi;\
	xorl C, %edi;\
	xorl B, D;\
	andl D, %edi;\
	xorl B, %edi;\
	xorl B, D;\

/*
 * stack offsets
 * void sha1block(uchar *DATA, int LEN, ulong *STATE)
 */
#define	DATA	8
#define	LEN	12
#define	STATE	16

/*
 * stack offsets for locals
 * ulong w[80];
 * uchar *edata;
 * ulong *w15, *w40, *w60, *w80;
 * register local
 * ulong *wp = %ebp
 * ulong a = eax, b = ebx, c = ecx, d = edx, e = esi
 * ulong tmp = edi
 */
#define WARRAY	(-4-(80*4))
#define TMP1	(-8-(80*4))
#define TMP2	(-12-(80*4))
#define W15	(-16-(80*4))
#define W40	(-20-(80*4))
#define W60	(-24-(80*4))
#define W80	(-28-(80*4))
#define EDATA	(-32-(80*4))
#define OLDEBX	(-36-(80*4))
#define OLDESI	(-40-(80*4))
#define OLDEDI	(-44-(80*4))

	/* Prelude */
	pushl %ebp
	mov %ebx, OLDEBX(%esp)
	mov %esi, OLDESI(%esp)
	mov %edi, OLDEDI(%esp)

	movl DATA(%esp), %eax
	addl LEN(%esp), %eax
	movl %eax, EDATA(%esp)

	leal (WARRAY+15*4)(%esp), %edi	/* aw15 */
	movl %edi, W15(%esp)
	leal (WARRAY+40*4)(%esp), %edx	/* aw40 */
	movl %edx, W40(%esp)
	leal (WARRAY+60*4)(%esp), %ecx	/* aw60 */
	movl %ecx, W60(%esp)
	leal (WARRAY+80*4)(%esp), %edi	/* aw80 */
	movl %edi, W80(%esp)

mainloop:
	leal WARRAY(%esp), %ebp		/* warray */

	movl STATE(%esp), %edi		/* state */
	movl (%edi),%eax
	movl 4(%edi),%ebx
	movl %ebx, TMP1(%esp)		/* tmp1 */
	movl 8(%edi), %ecx
	movl 12(%edi), %edx
	movl 16(%edi), %esi

	movl DATA(%esp), %ebx		/* data */

loop1:
	BODY0(0,FN1,0x5a827999,%eax,TMP1(%esp),%ecx,%edx,%esi)
	movl %esi,TMP2(%esp)
	BODY0(4,FN1,0x5a827999,%esi,%eax,TMP1(%esp),%ecx,%edx)
	movl TMP1(%esp),%esi
	BODY0(8,FN1,0x5a827999,%edx,TMP2(%esp),%eax,%esi,%ecx)
	BODY0(12,FN1,0x5a827999,%ecx,%edx,TMP2(%esp),%eax,%esi)
	movl %esi,TMP1(%esp)
	BODY0(16,FN1,0x5a827999,%esi,%ecx,%edx,TMP2(%esp),%eax)
	movl TMP2(%esp),%esi

	addl $20, %ebx
	addl $20, %ebp
	cmpl W15(%esp), %ebp	/* w15 */
	jb loop1

	BODY0(0,FN1,0x5a827999,%eax,TMP1(%esp),%ecx,%edx,%esi)
	addl $4, %ebx
	MOVL %ebx, DATA(%esp)	/* data */
	MOVL TMP1(%esp),%ebx

	BODY(4,FN1,0x5a827999,%esi,%eax,%ebx,%ecx,%edx)
	BODY(8,FN1,0x5a827999,%edx,%esi,%eax,%ebx,%ecx)
	BODY(12,FN1,0x5a827999,%ecx,%edx,%esi,%eax,%ebx)
	BODY(16,FN1,0x5a827999,%ebx,%ecx,%edx,%esi,%eax)

	addl $20, %ebp

loop2:
	BODY(0,FN24,0x6ed9eba1,%eax,%ebx,%ecx,%edx,%esi)
	BODY(4,FN24,0x6ed9eba1,%esi,%eax,%ebx,%ecx,%edx)
	BODY(8,FN24,0x6ed9eba1,%edx,%esi,%eax,%ebx,%ecx)
	BODY(12,FN24,0x6ed9eba1,%ecx,%edx,%esi,%eax,%ebx)
	BODY(16,FN24,0x6ed9eba1,%ebx,%ecx,%edx,%esi,%eax)

	addl $20,%ebp
	cmpl W40(%esp), %ebp
	jb loop2

loop3:
	BODY(0,FN3,0x8f1bbcdc,%eax,%ebx,%ecx,%edx,%esi)
	BODY(4,FN3,0x8f1bbcdc,%esi,%eax,%ebx,%ecx,%edx)
	BODY(8,FN3,0x8f1bbcdc,%edx,%esi,%eax,%ebx,%ecx)
	BODY(12,FN3,0x8f1bbcdc,%ecx,%edx,%esi,%eax,%ebx)
	BODY(16,FN3,0x8f1bbcdc,%ebx,%ecx,%edx,%esi,%eax)

	addl $20, %ebp
	cmpl W60(%esp), %ebp 	/* w60 */
	jb loop3

loop4:
	BODY(0,FN24,0xca62c1d6,%eax,%ebx,%ecx,%edx,%esi)
	BODY(4,FN24,0xca62c1d6,%esi,%eax,%ebx,%ecx,%edx)
	BODY(8,FN24,0xca62c1d6,%edx,%esi,%eax,%ebx,%ecx)
	BODY(12,FN24,0xca62c1d6,%ecx,%edx,%esi,%eax,%ebx)
	BODY(16,FN24,0xca62c1d6,%ebx,%ecx,%edx,%esi,%eax)

	addl $20, %ebp
	cmpl W80(%esp), %ebp 	/* w80 */
	jb loop4

	movl STATE(%esp), %edi	/* state */
	addl %eax, 0(%edi)
	addl %ebx, 4(%edi)
	addl %ecx, 8(%edi)
	addl %edx, 12(%edi)
	addl %esi, 16(%edi)

	movl EDATA(%esp), %edi	/* edata */
	cmpl %edi, DATA(%esp)	/* data */
	jb mainloop

	/* Postlude */
	mov OLDEBX(%esp), %ebx
	mov OLDESI(%esp), %esi
	mov OLDEDI(%esp), %edi
	movl %esp, %ebp
	leave
	ret
