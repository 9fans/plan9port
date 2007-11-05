/*	mpvecadd(mpdigit *a, int alen, mpdigit *b, int blen, mpdigit *sum) */
/*		sum[0:alen] = a[0:alen-1] + b[0:blen-1] */
/*	prereq: alen >= blen, sum has room for alen+1 digits */
/* (very old gnu assembler doesn't allow multiline comments) */

.text

.p2align 2,0x90
.globl _mpvecadd
_mpvecadd:
	/* Prelude */
	pushl %ebp		/* save on stack */
	pushl %ebx
	pushl %esi
	pushl %edi

	leal 20(%esp), %ebp		/* %ebp = FP for now */

	movl	4(%ebp), %edx		/* alen */
	movl	12(%ebp), %ecx		/* blen */
	movl	0(%ebp), %esi		/* a */
	movl	8(%ebp), %ebx		/* b */
	subl	%ecx, %edx
	movl	16(%ebp), %edi		/* sum */
	xorl	%ebp, %ebp		/* this also sets carry to 0 */

	/* skip addition if b is zero */
	testl	%ecx,%ecx
	je	2f

	/* sum[0:blen-1],carry = a[0:blen-1] + b[0:blen-1] */
1:
	movl	(%esi, %ebp, 4), %eax
	adcl	(%ebx, %ebp, 4), %eax
	movl	%eax, (%edi, %ebp, 4)
	incl	%ebp
	loop	1b

2:
	/* jump if alen > blen */
	incl	%edx
	movl	%edx, %ecx
	loop	5f

	/* sum[alen] = carry */
3:
	jb	4f
	movl	$0, (%edi, %ebp, 4)
	jmp 6f

4:
	movl	$1, (%edi, %ebp, 4)
	jmp 6f

	/* sum[blen:alen-1],carry = a[blen:alen-1] + 0 */
5:
	movl	(%esi, %ebp, 4),%eax
	adcl	$0, %eax
	movl	%eax, (%edi, %ebp, 4)
	incl	%ebp
	loop	5b
	jmp	3b

6:
	/* Postlude */
	popl %edi
	popl %esi
	popl %ebx
	popl %ebp
	ret
