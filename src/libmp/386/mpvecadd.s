/*	mpvecadd(mpdigit *a, int alen, mpdigit *b, int blen, mpdigit *sum) */
/*		sum[0:alen] = a[0:alen-1] + b[0:blen-1] */
/*	prereq: alen >= blen, sum has room for alen+1 digits */
/* (very old gnu assembler doesn't allow multiline comments) */

.text

.p2align 2,0x90
.globl mpvecadd
mpvecadd:
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
	je	_add1

	/* sum[0:blen-1],carry = a[0:blen-1] + b[0:blen-1] */
_addloop1:
	movl	(%esi, %ebp, 4), %eax
	adcl	(%ebx, %ebp, 4), %eax
	movl	%eax, (%edi, %ebp, 4)
	incl	%ebp
	loop	_addloop1

_add1:
	/* jump if alen > blen */
	incl	%edx
	movl	%edx, %ecx
	loop	_addloop2

	/* sum[alen] = carry */
_addend:
	jb	_addcarry
	movl	$0, (%edi, %ebp, 4)
	jmp done

_addcarry:
	movl	$1, (%edi, %ebp, 4)
	jmp done

	/* sum[blen:alen-1],carry = a[blen:alen-1] + 0 */
_addloop2:
	movl	(%esi, %ebp, 4),%eax
	adcl	$0, %eax
	movl	%eax, (%edi, %ebp, 4)
	incl	%ebp
	loop	_addloop2
	jmp	_addend

done:
	/* Postlude */
	popl %edi
	popl %esi
	popl %ebx
	popl %ebp
	ret
