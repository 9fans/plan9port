package main

import "testing"

//FIX: change to script tests for comparison against C

// loadStr returns the present runes of [p0,p1) as a string.
func (t *Term) loadStr(r *Rasp, p0, p1 int) string {
	return string(t.rload(r, p0, p1))
}

// sects returns a compact description of the rasp's sections for assertions:
// present sections show their text in quotes, holes show "-N".
func sects(r *Rasp) string {
	s := ""
	for sec := r.sect; sec != nil; sec = sec.next {
		if s != "" {
			s += " "
		}
		if sec.text == nil {
			s += "-" + itoa(sec.nrunes)
		} else {
			s += "\"" + string(sec.text) + "\""
		}
	}
	return s
}

func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	neg := n < 0
	if neg {
		n = -n
	}
	var b []byte
	for n > 0 {
		b = append([]byte{byte('0' + n%10)}, b...)
		n /= 10
	}
	if neg {
		b = append([]byte{'-'}, b...)
	}
	return string(b)
}

func TestRaspBasic(t *testing.T) {
	tm := &Term{}
	r := new(Rasp)
	r.init()

	// Open a 10-rune hole.
	r.resize(0, 0, 10)
	if r.nrunes != 10 {
		t.Fatalf("nrunes = %d, want 10", r.nrunes)
	}
	if got := r.missing(0, 10); got != 10 {
		t.Fatalf("missing = %d, want 10", got)
	}
	if got := r.contig(0, 10, false); got != 10 {
		t.Fatalf("contig(absent) = %d, want 10", got)
	}
	if got := r.contig(0, 10, true); got != 0 {
		t.Fatalf("contig(present) = %d, want 0", got)
	}

	// Fill the first half.
	r.data(0, 5, []rune("hello"))
	if got := sects(r); got != `"hello" -5` {
		t.Fatalf("sects = %q, want %q", got, `"hello" -5`)
	}
	if got := r.missing(0, 10); got != 5 {
		t.Fatalf("missing = %d, want 5", got)
	}
	if got := r.contig(0, 10, true); got != 5 {
		t.Fatalf("contig(present) = %d, want 5", got)
	}
	if got := tm.loadStr(r, 0, 10); got != "hello" {
		t.Fatalf("load = %q, want %q (hole skipped)", got, "hello")
	}

	// Fill the second half, then coalesce.
	r.data(5, 10, []rune("world"))
	r.clean()
	if got := sects(r); got != `"helloworld"` {
		t.Fatalf("after clean sects = %q, want %q", got, `"helloworld"`)
	}
	if got := r.missing(0, 10); got != 0 {
		t.Fatalf("missing = %d, want 0", got)
	}
	if got := tm.loadStr(r, 0, 10); got != "helloworld" {
		t.Fatalf("load = %q, want %q", got, "helloworld")
	}
	if got := tm.loadStr(r, 3, 8); got != "lowor" {
		t.Fatalf("load(3,8) = %q, want %q", got, "lowor")
	}
}

func TestRaspResizeHole(t *testing.T) {
	tm := &Term{}
	r := new(Rasp)
	r.init()
	r.resize(0, 0, 10)
	r.data(0, 10, []rune("helloworld"))
	r.clean()

	// Open a 3-rune hole in the middle: "hello" | -3 | "world".
	r.resize(5, 0, 3)
	if r.nrunes != 13 {
		t.Fatalf("nrunes = %d, want 13", r.nrunes)
	}
	if got := sects(r); got != `"hello" -3 "world"` {
		t.Fatalf("sects = %q, want %q", got, `"hello" -3 "world"`)
	}
	if got := r.missing(0, 13); got != 3 {
		t.Fatalf("missing = %d, want 3", got)
	}
	// load skips the hole, returning the 10 present runes.
	if got := tm.loadStr(r, 0, 13); got != "helloworld" {
		t.Fatalf("load = %q, want %q", got, "helloworld")
	}
	if got := r.contig(0, 13, true); got != 5 {
		t.Fatalf("contig(present) from 0 = %d, want 5", got)
	}
	if got := r.contig(5, 13, false); got != 3 {
		t.Fatalf("contig(absent) from 5 = %d, want 3", got)
	}

	// Fill the hole and re-coalesce.
	r.data(5, 8, []rune("BIG"))
	r.clean()
	if got := sects(r); got != `"helloBIGworld"` {
		t.Fatalf("sects = %q, want %q", got, `"helloBIGworld"`)
	}
	if got := tm.loadStr(r, 0, 13); got != "helloBIGworld" {
		t.Fatalf("load = %q, want %q", got, "helloBIGworld")
	}
}

func TestRaspDelete(t *testing.T) {
	r := new(Rasp)
	r.init()
	r.resize(0, 0, 10)
	r.data(0, 10, []rune("0123456789"))
	r.clean()

	// Delete the middle 4 runes (offsets 3..7).
	r.resize(3, 4, 0)
	r.clean()
	if r.nrunes != 6 {
		t.Fatalf("nrunes = %d, want 6", r.nrunes)
	}
	tm := &Term{}
	if got := tm.loadStr(r, 0, 6); got != "012789" {
		t.Fatalf("load = %q, want %q", got, "012789")
	}
}

// TestRaspCleanCap checks clean does not coalesce present sections past
// TBLOCKSIZE.
func TestRaspCleanCap(t *testing.T) {
	r := new(Rasp)
	r.init()
	big := TBLOCKSIZE - 100
	r.resize(0, 0, big+200)
	r.data(0, big, make([]rune, big))
	r.data(big, big+200, make([]rune, 200))
	r.clean()
	// big + 200 > TBLOCKSIZE, so the two present sections stay separate.
	n := 0
	for s := r.sect; s != nil; s = s.next {
		n++
	}
	if n != 2 {
		t.Fatalf("section count = %d, want 2 (no coalesce past TBLOCKSIZE)", n)
	}
}
