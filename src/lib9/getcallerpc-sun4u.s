.text
.globl getcallerpc
getcallerpc:
	retl
	or %o7, %r0, %o0
