.globl _setlabel
.type  _setlabel,#function

_setlabel:
	ta	3
	st	%i0, [%o0]
	st	%i1, [%o0+4]
	st	%i2, [%o0+8]
	st	%i3, [%o0+12]
	st	%i4, [%o0+16]
	st	%i5, [%o0+20]
	st	%i6, [%o0+24]
	st	%i7, [%o0+28]
	st	%l0, [%o0+32]
	st	%l1, [%o0+36]
	st	%l2, [%o0+40]
	st	%l3, [%o0+44]
	st	%l4, [%o0+48]
	st	%l5, [%o0+52]
	st	%l6, [%o0+56]
	st	%l7, [%o0+60]
	st	%sp, [%o0+64]
	st	%o7, [%o0+68]
	jmpl	%o7+8, %g0
	or	%g0, %g0, %o0

.globl _gotolabel
.type	_gotolabel,#function

_gotolabel:
	ta	3
	ld	[%o0], %i0
	ld	[%o0+4], %i1
	ld	[%o0+8], %i2
	ld	[%o0+12], %i3
	ld	[%o0+16], %i4
	ld	[%o0+20], %i5
	ld	[%o0+24], %i6
	ld	[%o0+28], %i7
	ld	[%o0+32], %l0
	ld	[%o0+36], %l1
	ld	[%o0+40], %l2
	ld	[%o0+44], %l3
	ld	[%o0+48], %l4
	ld	[%o0+52], %l5
	ld	[%o0+56], %l6
	ld	[%o0+60], %l7
	ld	[%o0+64], %sp
	ld	[%o0+68], %o7
	jmpl	%o7+8, %g0
	or	%g0, 1, %o0
