package main

import (
	"fmt"
	"io"
	"unicode/utf8"
)

// bug reports a "can't happen" internal error. These paths are unreachable in
// correct operation (they correspond to C panic() calls guarding invariants).
func bug(msg string) {
	// unreachable
	panic("sam: " + msg)
}

// panicf is C's panic(): a fatal internal error. C prints the message, rescues
// any unsaved work, and aborts the process. Running in-process (as a library or
// under the test harness) we must not abort the host, so after the rescue we
// unwind to Run via exits — the same clean shutdown C's exits() uses, and the
// in-process analogue of C's abort()/process exit. Reachable when a terminal
// violates the protocol (e.g. an over-long Tsetsnarf leaves trailing bytes the
// receiver mis-frames as an oversized message).
func (s *Sam) panicf(msg string) {
	if s.panicking == 0 {
		s.panicking++
		s.downloaded = false
		s.dprint("sam: panic: %s\n", msg)
		s.rescue()
	}
	s.exits("panic")
}

// decodeRune decodes one rune from b, matching Plan 9 chartorune: invalid
// encodings yield RuneError with width 1.
func decodeRune(b []byte) (Rune, int) {
	r, w := utf8.DecodeRune(b)
	return r, w
}

func fullRune(b []byte) bool { return utf8.FullRune(b) }

// dprint formats and writes diagnostic/menu output. When not downloaded it
// goes to stderr; when downloaded it is inserted into the command window
// (error.c dprint -> termwrite).
func (s *Sam) dprint(format string, a ...any) {
	s.termwrite(fmt.Sprintf(format, a...))
}

// termwrite routes text either into the command file (downloaded) or to
// stderr (error.c termwrite).
func (s *Sam) termwrite(str string) {
	if s.downloaded {
		p := tmpcstr(str)
		if s.cmd != nil {
			s.loginsert(s.cmd, s.cmdpt, p.s, p.n())
		} else {
			Strinsert(&s.cmdstr, p, Posn(s.cmdstr.n()))
		}
		s.cmdptadv += Posn(p.n())
	} else {
		io.WriteString(s.err, str)
	}
}
