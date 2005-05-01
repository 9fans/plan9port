#include <sys/syscall.h>
#include <machine/asm.h>

ENTRY(_tas)
	li	%r0, 0
	mr	%r4, %r3
	lis	%r5, 0xcafe
	ori	%r5, %r5, 0xbabe
1:
	lwarx	%r3, %r0, %r4
	cmpwi	%r3, 0
	bne	2f
	stwcx.	%r5, %r0, %r4
	bne-	1b
2:
	sync
	blr

ENTRY(_getmcontext)				/* xxx: instruction scheduling */
	mflr	%r0
	mfcr	%r5
	mfctr	%r6
	mfxer	%r7
	stw	%r0, 0*4(%r3)
	stw	%r5, 1*4(%r3)
	stw	%r6, 2*4(%r3)
	stw	%r7, 3*4(%r3)

	stw	%r1, 4*4(%r3)
	stw	%r2, 5*4(%r3)
	li	%r5, 1			/* return value for setmcontext */
	stw	%r5, 6*4(%r3)

	stw	%r13, (0+7)*4(%r3)	/* callee-save GPRs */
	stw	%r14, (1+7)*4(%r3)	/* xxx: block move */
	stw	%r15, (2+7)*4(%r3)
	stw	%r16, (3+7)*4(%r3)
	stw	%r17, (4+7)*4(%r3)
	stw	%r18, (5+7)*4(%r3)
	stw	%r19, (6+7)*4(%r3)
	stw	%r20, (7+7)*4(%r3)
	stw	%r21, (8+7)*4(%r3)
	stw	%r22, (9+7)*4(%r3)
	stw	%r23, (10+7)*4(%r3)
	stw	%r24, (11+7)*4(%r3)
	stw	%r25, (12+7)*4(%r3)
	stw	%r26, (13+7)*4(%r3)
	stw	%r27, (14+7)*4(%r3)
	stw	%r28, (15+7)*4(%r3)
	stw	%r29, (16+7)*4(%r3)
	stw	%r30, (17+7)*4(%r3)
	stw	%r31, (18+7)*4(%r3)

	li	%r3, 0			/* return */
	blr

ENTRY(_setmcontext)
	lwz	%r13, (0+7)*4(%r3)	/* callee-save GPRs */
	lwz	%r14, (1+7)*4(%r3)	/* xxx: block move */
	lwz	%r15, (2+7)*4(%r3)
	lwz	%r16, (3+7)*4(%r3)
	lwz	%r17, (4+7)*4(%r3)
	lwz	%r18, (5+7)*4(%r3)
	lwz	%r19, (6+7)*4(%r3)
	lwz	%r20, (7+7)*4(%r3)
	lwz	%r21, (8+7)*4(%r3)
	lwz	%r22, (9+7)*4(%r3)
	lwz	%r23, (10+7)*4(%r3)
	lwz	%r24, (11+7)*4(%r3)
	lwz	%r25, (12+7)*4(%r3)
	lwz	%r26, (13+7)*4(%r3)
	lwz	%r27, (14+7)*4(%r3)
	lwz	%r28, (15+7)*4(%r3)
	lwz	%r29, (16+7)*4(%r3)
	lwz	%r30, (17+7)*4(%r3)
	lwz	%r31, (18+7)*4(%r3)

	lwz	%r1, 4*4(%r3)
	lwz	%r2, 5*4(%r3)

	lwz	%r0, 0*4(%r3)
	mtlr	%r0
	lwz	%r0, 1*4(%r3)
	mtcr	%r0			/* mtcrf 0xFF, %r0 */
	lwz	%r0, 2*4(%r3)
	mtctr	%r0
	lwz	%r0, 3*4(%r3)
	mtxer	%r0

	lwz	%r3,	6*4(%r3)
	blr

ENTRY(rfork_thread)
	/* sanity check */
	cmpwi	%r4, 0
	beq	1f
	cmpwi	%r5, 0
	beq	1f
	
	mr	%r7,%r4
	
	/* call rfork */
	li	%r0, SYS_rfork
	sc
	cmpwi	%r0, 0
	bne	2f
	
	/* check if we are parent or child */
	cmpwi	%r3, 0
	bnelr
	
	/* child */
	mtlr	%r5	/* fp */
	mr	%r3, %r6	/* arg */
	mr	%r1, %r7	/* new sp */
	blrl
	
	/* child returned, call _exit */
	li	%r0, SYS_exit
	sc
1:
	li	%r3, -1
2:
	b PIC_PLT(_C_LABEL(__cerror))

