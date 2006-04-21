.text
.p2align 2,0x90
.globl mpdigdiv
mpdigdiv:
	/* Prelude */
	pushl %ebp		/* save on stack */
	pushl %ebx
	
	leal 12(%esp), %ebp	/* %ebp = FP for now */
	movl 0(%ebp), %ebx	/* dividend */
	movl 0(%ebx), %eax
	movl 4(%ebx), %edx
	movl 4(%ebp), %ebx	/* divisor */
	movl 8(%ebp), %ebp	/* quotient */

	xorl	%ecx, %ecx
	cmpl	%ebx, %edx		/* dividend >= 2^32 * divisor */
	jae	divovfl
	cmpl	%ecx, %ebx		/* divisor == 1 */
	je	divovfl
	divl	%ebx		/* AX = DX:AX/BX */
	movl	%eax, (%ebp)
done:
	/* Postlude */
	popl %ebx
	popl %ebp
	ret

	/* return all 1's */
divovfl:
	notl	%ecx
	movl	%ecx, (%ebp)
	jmp done
