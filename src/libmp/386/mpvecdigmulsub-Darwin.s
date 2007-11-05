/*
 *	mpvecdigmulsub(mpdigit *b, int n, mpdigit m, mpdigit *p)
 *
 *	p -= b*m
 *
 *	each step look like:
 *		hi,lo = m*b[i]
 *		lo += oldhi + carry
 *		hi += carry
 *		p[i] += lo
 *		oldhi = hi
 *
 *	the registers are:
 *		hi = DX		- constrained by hardware
 *		lo = AX		- constrained by hardware
 *		b = SI		- can't be BP
 *		p = DI		- can't be BP
 *		i = BP
 *		n = CX		- constrained by LOOP instr
 *		m = BX
 *		oldhi = EX
 *		
 */
.text

.globl _mpvecdigmulsub
_mpvecdigmulsub:
	/* Prelude */
	pushl %ebp		/* save on stack */
	pushl %ebx
	pushl %esi
	pushl %edi

	leal 20(%esp), %ebp		/* %ebp = FP for now */
	movl	0(%ebp), %esi		/* b */
	movl	4(%ebp), %ecx		/* n */
	movl	8(%ebp), %ebx		/* m */
	movl	12(%ebp), %edi		/* p */
	xorl	%ebp, %ebp
	pushl %ebp
1:
	movl	(%esi, %ebp, 4),%eax	/* lo = b[i] */
	mull	%ebx			/* hi, lo = b[i] * m */
	addl	0(%esp), %eax		/* lo += oldhi */
	jae	2f
	incl	%edx			/* hi += carry */
2:
	subl	%eax, (%edi, %ebp, 4)
	jae	3f
	incl	%edx			/* hi += carry */
3:
	movl	%edx, 0(%esp)
	incl	%ebp
	loop	1b
	popl %eax
	subl	%eax, (%edi, %ebp, 4)
	jae	4f
	movl	$-1, %eax
	jmp 5f
4:
	movl	$1, %eax
5:
	/* Postlude */
	popl %edi
	popl %esi
	popl %ebx
	popl %ebp
	ret

