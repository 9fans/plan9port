.globl	_setlabel
.type	_setlabel,@function

_setlabel:
	movl	4(%esp), %eax
	movl	0(%esp), %edx
	movl	%edx, 0(%eax)
	movl	%ebx, 4(%eax)
	movl	%esp, 8(%eax)
	movl	%ebp, 12(%eax)
	movl	%esi, 16(%eax)
	movl	%edi, 20(%eax)
	xorl	%eax, %eax
	ret

.globl	_gotolabel
.type	_gotolabel,@function

_gotolabel:
	pushl $1
	call _threadinswitch
	popl %eax
	movl	4(%esp), %edx
	movl	0(%edx), %ecx
	movl	4(%edx), %ebx
	movl	8(%edx), %esp
	movl	12(%edx), %ebp
	movl	16(%edx), %esi
	movl	20(%edx), %edi
	movl	%ecx, 0(%esp)
	pushl $0
	call _threadinswitch
	popl %eax
	xorl	%eax, %eax
	incl	%eax
	ret


# .globl	_xinc
# _xinc:
# 	movl 4(%esp), %eax
# 	lock incl 0(%eax)
# 	ret
# 
# .globl	_xdec
# _xdec:
# 	movl 4(%esp), %eax
# 	lock decl 0(%eax)
# 	jz iszero
# 	movl $1, %eax
# 	ret
# iszero:
# 	movl $0, %eax
# 	ret
# 
