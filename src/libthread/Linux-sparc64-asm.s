	.section	".text", #alloc, #execinstr
	.align		8
	.skip		16
	.global _tas
!	.type	_tas,2
_tas:
	or	%g0,1,%o1
	swap	[%o0],%o1	! o0 points to lock; key is first word
	retl
	mov	%o1, %o0

   	.size	_tas,(.-_tas)

