/*
 *	mpvecdigmul(mpdigit *b, int n, mpdigit m, mpdigit *p)
 *
 *	p += b*m
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
 *		b+n = SI	- can't be BP
 *		p+n = DI	- can't be BP
 *		i-n = BP
 *		m = BX
 *		oldhi = CX
 *		
 */
.text

.p2align 2,0x90
.globl mpvecdigmuladd
	.type mpvecdigmuladd, @function
mpvecdigmuladd:
	/* Prelude */
	pushl %ebp
	movl %ebx, -4(%esp)		/* save on stack */
	movl %esi, -8(%esp)
	movl %edi, -12(%esp)

	movl	8(%esp), %esi		/* b */
	movl	12(%esp), %ecx		/* n */
	movl	16(%esp), %ebx		/* m */
	movl	20(%esp), %edi		/* p */
	movl	%ecx, %ebp
	negl	%ebp			/* BP = -n */
	shll	$2, %ecx
	addl	%ecx, %esi		/* SI = b + n */
	addl	%ecx, %edi		/* DI = p + n */
	xorl	%ecx, %ecx
_muladdloop:
	movl	(%esi, %ebp, 4), %eax	/* lo = b[i] */
	mull	%ebx			/* hi, lo = b[i] * m */
	addl	%ecx,%eax		/* lo += oldhi */
	jae	_muladdnocarry1
	incl	%edx			/* hi += carry */
_muladdnocarry1:
	addl	%eax, (%edi, %ebp, 4)	/* p[i] += lo */
	jae	_muladdnocarry2
	incl	%edx			/* hi += carry */
_muladdnocarry2:
	movl	%edx, %ecx		/* oldhi = hi */
	incl	%ebp			/* i++ */
	jnz	_muladdloop
	xorl	%eax, %eax
	addl	%ecx, (%edi, %ebp, 4)	/* p[n] + oldhi */
	adcl	%eax, %eax		/* return carry out of p[n] */

	/* Postlude */
	movl -4(%esp), %ebx		/* restore from stack */
	movl -8(%esp), %esi
	movl -12(%esp), %edi
	movl %esp, %ebp
	leave
	ret
