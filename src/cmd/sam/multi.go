package main

import "path"

// File menu management (multi.c). The C List of File* becomes s.file []*File.

func (s *Sam) newfile() *File {
	f := s.fileopen()
	s.file = append([]*File{f}, s.file...) // inslist(&file, 0, f)
	f.tag = s.tag
	s.tag++
	if s.downloaded {
		s.outTs(Hnewname, f.tag)
	}
	// already sorted; file name is ""
	return f
}

func (s *Sam) whichmenu(f *File) int {
	for i, g := range s.file {
		if g == f {
			return i
		}
	}
	// untested
	return -1
}

func (s *Sam) delfile(f *File) {
	w := s.whichmenu(f)
	if w < 0 { // e.g. x/./D
		// untested
		return
	}
	if s.downloaded {
		s.outTs(Hdelname, f.tag)
	}
	s.file = append(s.file[:w], s.file[w+1:]...)
	s.fileclose(f)
}

func (s *Sam) fullname(name *String) {
	if name.n() > 0 && name.s[0] != '/' && name.s[0] != 0 {
		Strinsert(name, &s.curwd, 0)
	}
}

func (s *Sam) fixname(name *String) {
	s.fullname(name)
	str := Strtoc(name)
	if len(str) > 0 {
		str = cleanname(str)
	}
	t := tmpcstr(str)
	Strduplstr(name, t)
	freetmpstr(t)

	if Strispre(&s.curwd, name) {
		Strdelete(name, 0, s.curwd.n())
	}
}

// cleanname is plan9's cleanname; path.Clean matches it for the paths sam sees.
func cleanname(p string) string {
	return path.Clean(p)
}

func (s *Sam) sortname(f *File) {
	w := s.whichmenu(f)
	dupwarned := false
	s.file = append(s.file[:w], s.file[w+1:]...) // dellist
	var i int
	if f == s.cmd {
		i = 0
	} else {
		for i = 0; i < len(s.file); i++ {
			cmp := Strcmp(&f.name, &s.file[i].name)
			if cmp == 0 && !dupwarned {
				dupwarned = true
				s.warn_S(Wdupname, &f.name)
			} else if cmp < 0 && (i > 0 || s.cmd == nil) {
				break
			}
		}
	}
	// inslist(&file, i, f)
	s.file = append(s.file, nil)
	copy(s.file[i+1:], s.file[i:])
	s.file[i] = f
	if s.downloaded {
		s.outTsS(Hmovname, f.tag, &f.name)
	}
}

func (s *Sam) state(f *File, cleandirty int) {
	if f == s.cmd {
		return
	}
	f.unread = false
	if s.downloaded && s.whichmenu(f) >= 0 { // else flist or menu
		if f.mod && cleandirty != Dirty {
			s.outTs(Hclean, f.tag)
		} else if !f.mod && cleandirty == Dirty {
			s.outTs(Hdirty, f.tag)
		}
	}
	if cleandirty == Clean {
		f.mod = false
	} else {
		f.mod = true
	}
}

func (s *Sam) lookfile(str *String) *File {
	for _, f := range s.file {
		if Strcmp(&f.name, str) == 0 {
			return f
		}
	}
	return nil
}
