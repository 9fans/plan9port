.text
.align 8

.globl	_libthread_getmcontext
_libthread_getmcontext:
	movq	$1, 0*8(%rdi)  // rax
	movq	%rbx, 1*8(%rdi)
	movq	%rcx, 2*8(%rdi)
	movq	%rdx, 3*8(%rdi)
	movq	%rsi, 4*8(%rdi)
	movq	%rdi, 5*8(%rdi)
	movq	%rbp, 6*8(%rdi)
	movq	%rsp, 7*8(%rdi)
	movq	%r8, 8*8(%rdi)
	movq	%r9, 9*8(%rdi)
	movq	%r10, 10*8(%rdi)
	movq	%r11, 11*8(%rdi)
	movq	%r12, 12*8(%rdi)
	movq	%r13, 13*8(%rdi)
	movq	%r14, 14*8(%rdi)
	movq	%r15, 15*8(%rdi)
	movq	$0, %rax
	ret

.globl	_libthread_setmcontext
_libthread_setmcontext:
	movq	0*8(%rdi), %rax
	movq	1*8(%rdi), %rbx
	movq	2*8(%rdi), %rcx
	movq	3*8(%rdi), %rdx
	movq	4*8(%rdi), %rsi
	// %rdi later
	movq	6*8(%rdi), %rbp
	movq	7*8(%rdi), %rsp
	movq	8*8(%rdi), %r8
	movq	9*8(%rdi), %r9
	movq	10*8(%rdi), %r10
	movq	11*8(%rdi), %r11
	movq	12*8(%rdi), %r12
	movq	13*8(%rdi), %r13
	movq	14*8(%rdi), %r14
	movq	15*8(%rdi), %r15
	movq	5*8(%rdi), %rdi
	ret
