	.globl	_tas
_tas:
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

