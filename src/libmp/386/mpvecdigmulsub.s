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

/* XXX: had to use "-4(%esp)" kludge to get around inability to
 *      push/pop without first adjusting %esp.  This may not be
 *      as fast as using push/pop (and accessing pushed element
 *      with "(%esp)".)
 */

.p2align 2,0x90
.globl mpvecdigmulsub
	.type mpvecdigmulsub, @function
mpvecdigmulsub:
	/* Prelude */
	pushl %ebp
	movl %ebx, -8(%esp)		/* save on stack */
	movl %esi, -12(%esp)
	movl %edi, -16(%esp)

	movl	8(%esp), %esi		/* b */
	movl	12(%esp), %ecx		/* n */
	movl	16(%esp), %ebx		/* m */
	movl	20(%esp), %edi		/* p */
	xorl	%ebp, %ebp
	movl	%ebp, -4(%esp)
_mulsubloop:
	movl	(%esi, %ebp, 4),%eax	/* lo = b[i] */
	mull	%ebx			/* hi, lo = b[i] * m */
	addl	-4(%esp), %eax		/* lo += oldhi */
	jae	_mulsubnocarry1
	incl	%edx			/* hi += carry */
_mulsubnocarry1:
	subl	%eax, (%edi, %ebp, 4)
	jae	_mulsubnocarry2
	incl	%edx			/* hi += carry */
_mulsubnocarry2:
	movl	%edx, -4(%esp)
	incl	%ebp
	loop	_mulsubloop
	movl	-4(%esp), %eax
	subl	%eax, (%edi, %ebp, 4)
	jae	_mulsubnocarry3
	movl	$-1, %eax
	jmp done

_mulsubnocarry3:
	movl	$1, %eax

done:
	/* Postlude */
	movl -8(%esp), %ebx		/* restore from stack */
	movl -12(%esp), %esi
	movl -16(%esp), %edi
	movl %esp, %ebp
	leave
	ret
