 
.globl _tas
_tas:
	mov	r3, #0xCA000000
	add	r3, r3, #0xFE0000
	add	r3, r3, #0xBA00
	add	r3, r3, #0xBE
	swp	r3, r3, [r0]
	mov	r0, r3
	mov	pc, lr
