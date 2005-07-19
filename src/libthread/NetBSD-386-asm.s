.globl _tas
_tas:
	movl $0xCAFEBABE, %eax
	movl 4(%esp), %ecx
	xchgl %eax, 0(%ecx)
	ret

