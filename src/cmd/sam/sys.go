package main

import "io"

func (s *Sam) resetsys() { s.inerror = false }

func (s *Sam) syserror(a string, e error) {
	if !s.inerror {
		s.inerror = true
		s.dprint("%s: ", a)
		s.error_s(Eio, s.oserr(e))
	}
}

// Write writes p to w, mirroring sam.c's Write(): a short write (or error) is
// routed through syserror("write"), which on the first failure unwinds via
// s.error_s like C's error_s(Eio) longjmp. It returns the number of bytes written.
func (s *Sam) Write(w io.Writer, p []byte) int {
	m, err := w.Write(p)
	if m != len(p) {
		s.syserror("write", err)
	}
	return m
}

// Close closes c, mirroring sam.c's Close(): a failure is routed through
// syserror("close").
func (s *Sam) Close(c io.Closer) {
	if err := c.Close(); err != nil {
		s.syserror("close", err)
	}
}
