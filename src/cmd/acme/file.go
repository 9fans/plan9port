package main

// File is the data plus undo/redo transcript shared by a window's body and
// any zerox copies (dat.h File). The C version (file.c) packs Undo structs
// into rune Buffers (delta/epsilon) read backwards; following the sam port we
// use a typed []undoRec consumed LIFO, each record carrying its own payload.
type File struct {
	b         Buffer    // the data
	delta     []undoRec // transcript of changes (undo)
	epsilon   []undoRec // inversion of delta (redo)
	elog      Elog      // current pending editor change
	elogbuf   []elogRec // log of pending editor changes
	name      []rune    // name of associated file
	qidpath   uint64    // of file when read
	mtime     int64     // of file when read
	dev       int       // of file when read
	sha1      [20]byte  // of file when read
	unread    bool      // file has not been read from disk
	editclean bool      // mark clean after edit command

	seq     int     // if seq==0, File acts like Buffer
	mod     bool    // modified since last read/write
	curtext *Text   // most recently used associated text
	text    []*Text // associated texts
	dumpid  int     // used in dumping zeroxed windows
}

func (f *File) nname() int { return len(f.name) }

// undoRec is one entry in delta/epsilon. data holds the runes to re-insert
// (Insert) or the old file name (Filename); it is nil for Delete.
type undoRec struct {
	typ  int  // Delete, Insert, Filename
	mod  bool // previous value of the modify bit
	seq  int  // sequence number
	p0   int  // location of change (unused for Filename)
	n    int  // # runes in string or file name
	data []rune
}

// fileaddtext associates text t with file f, allocating f if nil (fileaddtext).
func fileaddtext(f *File, t *Text) *File {
	if f == nil {
		f = new(File)
		f.unread = true
	}
	f.text = append(f.text, t)
	f.curtext = t
	return f
}

// filedeltext removes text t from f, closing f when its last text goes.
func (a *Acme) filedeltext(f *File, t *Text) {
	i := -1
	for j, x := range f.text {
		if x == t {
			i = j
			break
		}
	}
	if i < 0 {
		// unreachable
		bug("can't find text in filedeltext")
	}
	f.text = append(f.text[:i], f.text[i+1:]...)
	if len(f.text) == 0 {
		fileclose(f)
		return
	}
	if f.curtext == t {
		f.curtext = f.text[0]
	}
}

func (a *Acme) fileinsert(f *File, p0 int, s []rune) {
	if p0 > f.b.nc() {
		// unreachable
		bug("internal error: fileinsert")
	}
	if f.seq > 0 {
		fileuninsert(f, &f.delta, p0, len(s))
	}
	f.b.Insert(p0, s)
	if len(s) != 0 {
		f.mod = true
	}
}

// fileuninsert records, in delta, that inserting len ns runes at p0 is undone
// by deleting them.
func fileuninsert(f *File, delta *[]undoRec, p0, ns int) {
	*delta = append(*delta, undoRec{typ: Delete, mod: f.mod, seq: f.seq, p0: p0, n: ns})
}

func (a *Acme) filedelete(f *File, p0, p1 int) {
	if !(p0 <= p1 && p0 <= f.b.nc() && p1 <= f.b.nc()) {
		// unreachable
		bug("internal error: filedelete")
	}
	if f.seq > 0 {
		fileundelete(f, &f.delta, p0, p1)
	}
	f.b.Delete(p0, p1)
	if p1 > p0 {
		f.mod = true
	}
}

// fileundelete records, in delta, that deleting [p0,p1) is undone by
// re-inserting the runes there (which it copies into the record).
func fileundelete(f *File, delta *[]undoRec, p0, p1 int) {
	data := make([]rune, p1-p0)
	f.b.Read(p0, data)
	*delta = append(*delta, undoRec{typ: Insert, mod: f.mod, seq: f.seq, p0: p0, n: p1 - p0, data: data})
}

func (a *Acme) filesetname(f *File, name []rune) {
	if f.seq > 0 {
		fileunsetname(f, &f.delta)
	}
	f.name = append([]rune(nil), name...)
	f.unread = true
}

// fileunsetname records, in delta, that a name change is undone by restoring
// the old name (copied into the record).
func fileunsetname(f *File, delta *[]undoRec) {
	*delta = append(*delta, undoRec{typ: Filename, mod: f.mod, seq: f.seq, n: f.nname(),
		data: append([]rune(nil), f.name...)})
}

func fileredoseq(f *File) int {
	if len(f.epsilon) == 0 {
		return 0
	}
	return f.epsilon[len(f.epsilon)-1].seq
}

// fileundo applies one undo (isundo) or redo step, moving records from delta
// to epsilon and updating all associated texts; it reports the affected span
// in *q0p,*q1p (fileundo).
func (a *Acme) fileundo(f *File, isundo bool, q0p, q1p *int) {
	var delta, epsilon *[]undoRec
	var stop int
	if isundo {
		// undo; reverse delta onto epsilon, seq decreases
		delta = &f.delta
		epsilon = &f.epsilon
		stop = f.seq
	} else {
		// redo; reverse epsilon onto delta, seq increases
		delta = &f.epsilon
		epsilon = &f.delta
		stop = 0 // don't know yet
	}

	for len(*delta) > 0 {
		u := (*delta)[len(*delta)-1]
		if isundo {
			if u.seq < stop {
				f.seq = u.seq
				return
			}
		} else {
			if stop == 0 {
				stop = u.seq
			}
			if u.seq > stop {
				return
			}
		}
		*delta = (*delta)[:len(*delta)-1]
		switch u.typ {
		default:
			// unreachable
			bug("undo: bad type")
		case Delete:
			f.seq = u.seq
			fileundelete(f, epsilon, u.p0, u.p0+u.n)
			f.mod = u.mod
			f.b.Delete(u.p0, u.p0+u.n)
			for _, t := range f.text {
				a.textdelete(t, u.p0, u.p0+u.n, false)
			}
			*q0p = u.p0
			*q1p = u.p0
		case Insert:
			f.seq = u.seq
			fileuninsert(f, epsilon, u.p0, u.n)
			f.mod = u.mod
			f.b.Insert(u.p0, u.data)
			for _, t := range f.text {
				a.textinsert(t, u.p0, u.data, false)
			}
			*q0p = u.p0
			*q1p = u.p0 + u.n
		case Filename:
			f.seq = u.seq
			fileunsetname(f, epsilon)
			f.mod = u.mod
			f.name = append([]rune(nil), u.data...)
		}
	}
	if isundo {
		f.seq = 0
	}
}

func filereset(f *File) {
	f.delta = f.delta[:0]
	f.epsilon = f.epsilon[:0]
	f.seq = 0
}

func fileclose(f *File) {
	f.name = nil
	f.text = nil
	f.b.Close()
	f.delta = nil
	f.epsilon = nil
	elogclose(f)
}

func (a *Acme) filemark(f *File) {
	if len(f.epsilon) != 0 {
		f.epsilon = f.epsilon[:0]
	}
	f.seq = a.seq
}
