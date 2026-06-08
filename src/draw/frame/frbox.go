package frame

import (
	"unicode/utf8"

	"9fans.net/go/draw"
)

type box struct {
	wid    int // in pixels
	nrune  int // <0 ==> negate and treat as break char
	bytes  []byte
	bc     rune // break char
	minwid int
}

func drawerror(d *draw.Display, err string) {
	panic(err)
}

// addbox adds new boxes f.box[bn+1:bn+1+n].
func (f *Frame) addbox(bn, n int) {
	if bn > len(f.box) {
		drawerror(f.Display, "addbox")
	}
	olen := len(f.box)
	for cap(f.box) < olen+n {
		f.box = append(f.box[:cap(f.box)], box{})
	}
	f.box = f.box[:olen+n]
	copy(f.box[bn+n:], f.box[bn:])
	for i := bn; i < bn+n; i++ {
		f.box[i] = box{}
	}
}

// delbox deletes boxes f.box[n0:n1+1].
func (f *Frame) delbox(n0, n1 int) {
	if n0 >= len(f.box) || n1 >= len(f.box) || n1 < n0 {
		drawerror(f.Display, "delbox")
	}
	n1++
	copy(f.box[n0:], f.box[n1:])
	for j := len(f.box) - (n1 - n0); j < len(f.box); j++ {
		f.box[j] = box{}
	}
	f.box = f.box[:len(f.box)-(n1-n0)]
}

// dupbox duplicates f.box[bn], creating f.box[bn+1].
func (f *Frame) dupbox(bn int) {
	if f.box[bn].nrune < 0 {
		drawerror(f.Display, "dupbox")
	}
	f.addbox(bn+1, 1)
	f.box[bn+1] = f.box[bn]
	if f.box[bn].nrune >= 0 {
		p := make([]byte, len(f.box[bn].bytes))
		copy(p, f.box[bn].bytes)
		f.box[bn+1].bytes = p
	}
}

func runeindex(p []byte, n int) int {
	off := 0
	for i := 0; i < n; i++ {
		_, size := utf8.DecodeRune(p[off:])
		off += size
	}
	return off
}

// truncatebox drops the last n characters from b.
func (f *Frame) truncatebox(b *box, n int) {
	if b.nrune < 0 || b.nrune < n {
		drawerror(f.Display, "chopbox")
	}
	b.nrune -= n
	b.bytes = b.bytes[:runeindex(b.bytes, b.nrune)]
	b.wid = f.Font.BytesWidth(b.bytes)
}

// chopbox chops the first n characters from b.
func (f *Frame) chopbox(b *box, n int) {
	if b.nrune < 0 || b.nrune < n {
		drawerror(f.Display, "chopbox")
	}
	i := runeindex(b.bytes, n)
	copy(b.bytes, b.bytes[i:])
	b.bytes = b.bytes[:len(b.bytes)-i]
	b.nrune -= n
	b.wid = f.Font.BytesWidth(b.bytes)
}

// splitbox splits box bn after n runes.
func (f *Frame) splitbox(bn, n int) {
	f.dupbox(bn)
	f.truncatebox(&f.box[bn], f.box[bn].nrune-n)
	f.chopbox(&f.box[bn+1], n)
}

// mergebox merges boxes bn and bn+1.
func (f *Frame) mergebox(bn int) {
	b0 := &f.box[bn]
	b1 := &f.box[bn+1]
	b0.bytes = append(b0.bytes, b1.bytes...)
	b0.wid += b1.wid
	b0.nrune += b1.nrune
	f.delbox(bn+1, bn+1)
}

// findbox returns the index of a box starting at rune offset q,
// assuming that box bn starts at rune offset p.
// If needed, findbox splits an existing box to make one that
// starts at rune offset q.
func (f *Frame) findbox(bn, p, q int) int {
	for bn < len(f.box) && p+f.box[bn].NRUNE() <= q {
		p += f.box[bn].NRUNE()
		bn++
	}
	if p != q {
		f.splitbox(bn, q-p)
		bn++
	}
	return bn
}
