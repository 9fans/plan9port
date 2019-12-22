.globl _tas
_tas:
	movl $0xCAFEBABE, %eax
	movl 4(%esp), %ecx
	xchgl %eax, 0(%ecx)
	ret

.globl getmcontext
getmcontext:
	movl	4(%esp), %eax

	movw	%fs, 8(%eax)
	movw	%es, 12(%eax)
	movw	%ds, 16(%eax)
	movw	%ss, 76(%eax)
	movl	%edi, 20(%eax)
	movl	%esi, 24(%eax)
	movl	%ebp, 28(%eax)
	movl	%ebx, 36(%eax)
	movl	%edx, 40(%eax)		 
	movl	%ecx, 44(%eax)

	movl	$1, 48(%eax)	/* %eax */
	movl	(%esp), %ecx	/* %eip */
	movl	%ecx, 60(%eax)
	leal	4(%esp), %ecx	/* %esp */
	movl	%ecx, 72(%eax)

	movl	44(%eax), %ecx	/* restore %ecx */
	movl	$0, %eax
	ret

.globl setmcontext
setmcontext:
	movl	4(%esp), %eax

	movw	8(%eax), %fs
	movw	12(%eax), %es
	movw	16(%eax), %ds
	movw	76(%eax), %ss
	movl	20(%eax), %edi
	movl	24(%eax), %esi
	movl	28(%eax), %ebp
	movl	36(%eax), %ebx
	movl	40(%eax), %edx
	movl	44(%eax), %ecx

	movl	72(%eax), %esp
	pushl	60(%eax)	/* new %eip */
	movl	48(%eax), %eax
	ret

