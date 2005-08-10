/*	mpvecsub(mpdigit *a, int alen, mpdigit *b, int blen, mpdigit *diff) */
/*		diff[0:alen-1] = a[0:alen-1] - b[0:blen-1] */
/*	prereq: alen >= blen, diff has room for alen digits */
/* (very old gnu assembler doesn't allow multiline comments) */

.text

.p2align 2,0x90
.globl mpvecsub
	.type mpvecsub, @function
mpvecsub:
	/* Prelude */
	pushl %ebp
	movl %ebx, -4(%esp)		/* save on stack */
	movl %esi, -8(%esp)
	movl %edi, -12(%esp)

	movl	8(%esp), %esi		/* a */
	movl	16(%esp), %ebx		/* b */
	movl	12(%esp), %edx		/* alen */
	movl	20(%esp), %ecx		/* blen */
	movl	24(%esp), %edi		/* diff */
	subl	%ecx,%edx
	xorl	%ebp,%ebp			/* this also sets carry to 0 */

	/* skip subraction if b is zero */
	testl	%ecx,%ecx
	jz	_sub1

	/* diff[0:blen-1],borrow = a[0:blen-1] - b[0:blen-1] */
_subloop1:
	movl	(%esi, %ebp, 4), %eax
	sbbl	(%ebx, %ebp, 4), %eax
	movl	%eax, (%edi, %ebp, 4)
	incl	%ebp
	loop	_subloop1

_sub1:
	incl	%edx
	movl	%edx,%ecx
	loop	_subloop2
	jmp done

	/* diff[blen:alen-1] = a[blen:alen-1] - 0 */
_subloop2:
	movl	(%esi, %ebp, 4), %eax
	sbbl	$0, %eax
	movl	%eax, (%edi, %ebp, 4)
	INCL	%ebp
	LOOP	_subloop2

done:
	/* Postlude */
	movl -4(%esp), %ebx		/* restore from stack */
	movl -8(%esp), %esi
	movl -12(%esp), %edi
	movl %esp, %ebp
	leave
	ret

