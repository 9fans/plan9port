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

.globl _mpvecdigmuladd
_mpvecdigmuladd:
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
	movl	%ecx, %ebp
	negl	%ebp			/* BP = -n */
	shll	$2, %ecx
	addl	%ecx, %esi		/* SI = b + n */
	addl	%ecx, %edi		/* DI = p + n */
	xorl	%ecx, %ecx
1:
	movl	(%esi, %ebp, 4), %eax	/* lo = b[i] */
	mull	%ebx			/* hi, lo = b[i] * m */
	addl	%ecx,%eax		/* lo += oldhi */
	jae	2f
	incl	%edx			/* hi += carry */
2:
	addl	%eax, (%edi, %ebp, 4)	/* p[i] += lo */
	jae	3f
	incl	%edx			/* hi += carry */
3:
	movl	%edx, %ecx		/* oldhi = hi */
	incl	%ebp			/* i++ */
	jnz	1b
	xorl	%eax, %eax
	addl	%ecx, (%edi, %ebp, 4)	/* p[n] + oldhi */
	adcl	%eax, %eax		/* return carry out of p[n] */

	/* Postlude */
	popl %edi
	popl %esi
	popl %ebx
	popl %ebp
	ret

