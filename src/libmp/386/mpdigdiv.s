.text

.p2align 2,0x90
.globl mpdigdiv
	.type mpdigdiv, @function
mpdigdiv:
	/* Prelude */
	pushl %ebp
	movl %ebx, -4(%esp)		/* save on stack */

	movl	8(%esp), %ebx
	movl	(%ebx), %eax
	movl	4(%ebx), %edx

	movl	12(%esp), %ebx
	movl	16(%esp), %ebp
	xorl	%ecx, %ecx
	cmpl	%ebx, %edx		/* dividend >= 2^32 * divisor */
	jae	divovfl
	cmpl	%ecx, %ebx		/* divisor == 1 */
	je	divovfl
	divl	%ebx		/* AX = DX:AX/BX */
	movl	%eax, (%ebp)
	jmp done

	/* return all 1's */
divovfl:
	notl	%ecx
	movl	%ecx, (%ebp)

done:
	/* Postlude */
	movl -4(%esp), %ebx		/* restore from stack */
	movl %esp, %ebp
	leave
	ret

.endmpdigdiv:
	.size mpdigdiv,.endmpdigdiv-mpdigdiv
