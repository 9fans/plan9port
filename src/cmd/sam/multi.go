package main

import (
	"path"
	"slices"
)

func (s *Sam) newfile() *File {
	f := s.fileopen()
	s.file = slices.Insert(s.file, 0, f)
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
	s.file = slices.Delete(s.file, w, w+1)
	s.fileclose(f)
}

func (s *Sam) fullname(name []rune) []rune {
	if len(name) > 0 && name[0] != '/' {
		name = s.insertString(name, s.curwd, 0)
	}
	return name
}

func (s *Sam) fixname(name []rune) []rune {
	name = s.fullname(name)
	str := string(name)
	if len(str) > 0 {
		str = cleanname(str)
	}
	name = []rune(str)

	if runesHasPrefix(name, s.curwd) {
		name = slices.Delete(name, 0, len(s.curwd))
	}
	return name
}

// cleanname is plan9's cleanname; path.Clean matches it for the paths sam sees.
func cleanname(p string) string {
	return path.Clean(p)
}

func (s *Sam) sortname(f *File) {
	w := s.whichmenu(f)
	dupwarned := false
	s.file = slices.Delete(s.file, w, w+1)
	var i int
	if f == s.cmd {
		i = 0
	} else {
		for i = 0; i < len(s.file); i++ {
			cmp := runesCompare(f.name, s.file[i].name)
			if cmp == 0 && !dupwarned {
				dupwarned = true
				s.warn_S(Wdupname, f.name)
			} else if cmp < 0 && (i > 0 || s.cmd == nil) {
				break
			}
		}
	}
	s.file = slices.Insert(s.file, i, f)
	if s.downloaded {
		s.outTsS(Hmovname, f.tag, f.name)
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

func (s *Sam) lookfile(str []rune) *File {
	for _, f := range s.file {
		if runesCompare(f.name, str) == 0 {
			return f
		}
	}
	return nil
}
