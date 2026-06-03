//go:build unix

package main

// Unix-specific file metadata and error rendering. Split out (per io.c being
// system glue) so Windows or Plan 9 can supply their own versions later.

import (
	"errors"
	"io"
	"os"
	"syscall"
)

func statfile(name string) (dev uint32, qid uint64, mtime uint, length int64, ok bool) {
	fi, err := os.Stat(name)
	if err != nil {
		return 0, 0, 0, 0, false
	}
	return statFromInfo(fi)
}

func statfd(r io.Reader) (dev uint32, qid uint64, mtime uint, length int64, ok bool) {
	f, isFile := r.(*os.File)
	if !isFile {
		// unreachable: s.ioR is always an *os.File (os.Open or exec StdoutPipe)
		return 0, 0, 0, 0, false
	}
	fi, err := f.Stat()
	if err != nil {
		// unreachable: Stat of an already-open fd does not fail
		return 0, 0, 0, 0, false
	}
	return statFromInfo(fi)
}

func statFromInfo(fi os.FileInfo) (dev uint32, qid uint64, mtime uint, length int64, ok bool) {
	st, isStat := fi.Sys().(*syscall.Stat_t)
	if !isStat {
		// unreachable: on Unix os.FileInfo.Sys() is always *syscall.Stat_t
		return 0, 0, uint(fi.ModTime().Unix()), fi.Size(), true
	}
	return uint32(st.Dev), uint64(st.Ino), uint(fi.ModTime().Unix()), fi.Size(), true
}

// oserr renders err for an error message. Go's syscall.Errno strings are
// libc's strerror with a lowercased first letter; in compat mode we restore
// the capital so the output matches the C sam (Plan 9 %r) byte-for-byte, but
// in ordinary use we keep Go's nicer lowercase form.
func (s *Sam) oserr(err error) string {
	var errno syscall.Errno
	if errors.As(err, &errno) {
		m := errno.Error()
		if s.compat && len(m) > 0 && 'a' <= m[0] && m[0] <= 'z' {
			return string(m[0]-'a'+'A') + m[1:]
		}
		return m
	}
	if pe, ok := err.(*os.PathError); ok {
		return pe.Err.Error()
	}
	return err.Error()
}
