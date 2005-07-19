.globl _tas
_tas:
	movl $0xCAFEBABE, %eax
	movl 4(%esp), %ecx
	xchgl %eax, 0(%ecx)
	ret

.globl setmcontext
setmcontext:
	movl 4(%esp), %edx
	movl	8(%edx), %fs
	movl	12(%edx), %es
	movl	16(%edx), %ds
	movl	76(%edx), %ss
	movl	20(%edx), %edi
	movl	24(%edx), %esi
	movl	28(%edx), %ebp
	movl	%esp, %ecx		 
	movl	72(%edx), %esp		 
	pushl	60(%edx)	/* eip */	 
	pushl	44(%edx)	/* ecx */	 
	pushl	48(%edx)	/* eax */	 
	movl	36(%edx), %ebx		 
	movl	40(%edx), %edx
	movl	12(%ecx), %eax		 
	popl	%eax			 
	popl	%ecx
	ret

.globl getmcontext
getmcontext:
	pushl	%edx
	movl	8(%esp), %edx
	movl	%fs, 8(%edx)
	movl	%es, 12(%edx)
	movl	%ds, 16(%edx)
	movl	%ss, 76(%edx)
	movl	%edi, 20(%edx)
	movl	%esi, 24(%edx)
	movl	%ebp, 28(%edx)
	movl	%ebx, 36(%edx)
	movl	$1, 48(%edx)
	popl	%eax
	movl	%eax, 40(%edx)		 
	movl	%ecx, 44(%edx)
	movl	(%esp), %eax	/* eip */		 
	movl	%eax, 60(%edx)		 
	movl	%esp, %eax		 
	addl	$4, %eax	/* setmcontext will re-push the eip */	 
	movl	%eax, 72(%edx)
	movl	40(%edx), %edx		 
	xorl	%eax, %eax		 
	ret

