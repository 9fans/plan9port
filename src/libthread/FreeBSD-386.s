
.globl	_xinc
_xinc:
	movl 4(%esp), %eax
	lock incl 0(%eax)
	ret

.globl	_xdec
_xdec:
	movl 4(%esp), %eax
	lock decl 0(%eax)
	jz iszero
	movl %eax, 1
	ret
iszero:
	movl %eax, 0
	ret

