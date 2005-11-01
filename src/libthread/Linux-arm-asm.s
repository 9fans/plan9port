 
.globl _tas
_tas:
	mov	r3, #0xCA000000
	add	r3, r3, #0xFE0000
	add	r3, r3, #0xBA00
	add	r3, r3, #0xBE
	swp	r3, r3, [r0]
	mov	r0, r3
	mov	pc, lr

.globl getmcontext
getmcontext:
	/* r0 will be overwritten */
	str	r1, [r0,#4]!
	str	r2, [r0,#4]!
	str	r3, [r0,#4]!
	str	r4, [r0,#4]!
	str	r5, [r0,#4]!
	str	r6, [r0,#4]!
	str	r7, [r0,#4]!
	str	r8, [r0,#4]!
	str	r9, [r0,#4]!
	str	r10, [r0,#4]!
	str	r11, [r0,#4]!
	str	r12, [r0,#4]!
	str	r13, [r0,#4]!
	str	r14, [r0,#4]!
	/* r15 is pc */
	mov	r0, #0
	mov	pc, lr

.globl setmcontext
setmcontext:
	/* r0 will be overwritten */
	ldr	r1, [r0,#4]!
	ldr	r2, [r0,#4]!
	ldr	r3, [r0,#4]!
	ldr	r4, [r0,#4]!
	ldr	r5, [r0,#4]!
	ldr	r6, [r0,#4]!
	ldr	r7, [r0,#4]!
	ldr	r8, [r0,#4]!
	ldr	r9, [r0,#4]!
	ldr	r10, [r0,#4]!
	ldr	r11, [r0,#4]!
	ldr	r12, [r0,#4]!
	ldr	r13, [r0,#4]!
	ldr	r14, [r0,#4]!
	/* r15 is pc */
	mov	r0, #1
	mov	pc, lr

