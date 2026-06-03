package main

import (
	"fmt"
)

// Err enumerates sam's error conditions.
type Err int

const (
	// error_s
	Eopen Err = iota
	Ecreate
	Emenu
	Emodified
	Eio
	Ewseq
	// error_c
	Eunk
	Emissop
	Edelim
	// error
	Efork
	Eintr
	Eaddress
	Esearch
	Epattern
	Enewline
	Eblank
	Enopattern
	EnestXY
	Enolbrace
	Enoaddr
	Eoverlap
	Enosub
	Elongrhs
	Ebadrhs
	Erange
	Esequence
	Eorder
	Enoname
	Eleftpar
	Erightpar
	Ebadclass
	Ebadregexp
	Eoverflow
	Enocmd
	Epipe
	Enofile
	Etoolong
	Echanges
	Eempty
	Efsearch
	Emanyfiles
	Elongtag
	Esubexp
	Etmpovfl
	Eappend
	Ecantplumb
	Ebufload
)

var emsg = []string{
	// error_s
	Eopen:     "can't open",
	Ecreate:   "can't create",
	Emenu:     "not in menu:",
	Emodified: "changes to",
	Eio:       "I/O error:",
	Ewseq:     "can't write while changing:",
	// error_c
	Eunk:    "unknown command",
	Emissop: "no operand for",
	Edelim:  "bad delimiter",
	// error
	Efork:      "can't fork",
	Eintr:      "interrupt",
	Eaddress:   "address",
	Esearch:    "search",
	Epattern:   "pattern",
	Enewline:   "newline expected",
	Eblank:     "blank expected",
	Enopattern: "pattern expected",
	EnestXY:    "can't nest X or Y",
	Enolbrace:  "unmatched `}'",
	Enoaddr:    "command takes no address",
	Eoverlap:   "addresses overlap",
	Enosub:     "substitution",
	Elongrhs:   "& match too long",
	Ebadrhs:    "bad \\ in rhs",
	Erange:     "address range",
	Esequence:  "changes not in sequence",
	Eorder:     "addresses out of order",
	Enoname:    "no file name",
	Eleftpar:   "unmatched `('",
	Erightpar:  "unmatched `)'",
	Ebadclass:  "malformed `[]'",
	Ebadregexp: "malformed regexp",
	Eoverflow:  "reg. exp. list overflow",
	Enocmd:     "plan 9 command",
	Epipe:      "can't pipe",
	Enofile:    "no current file",
	Etoolong:   "string too long",
	Echanges:   "changed files",
	Eempty:     "empty string",
	Efsearch:   "file search",
	Emanyfiles: "non-unique match for \"\"",
	Elongtag:   "tag match too long",
	Esubexp:    "too many subexpressions",
	Etmpovfl:   "temporary file too large",
	Eappend:    "file is append-only",
	Ecantplumb: "no destination for plumb message",
	Ebufload:   "internal read error in buffer load",
}

// Warn enumerates sam's warnings (errors.h enum Warn).
type Warn int

const (
	Wdupname Warn = iota
	Wfile
	Wdate
	Wdupfile
	Wnulls
	Wpwd
	Wnotnewline
	Wbadstatus
)

var wmsg = []string{
	// warn_S
	Wdupname: "duplicate file name",
	Wfile:    "no such file",
	Wdate:    "write might change good version of",
	// warn_SS
	Wdupfile: "files might be aliased",
	// warn
	Wnulls:      "null characters elided",
	Wpwd:        "can't run pwd",
	Wnotnewline: "last char not newline",
	Wbadstatus:  "exit status not 0",
}

// mainloopJump is panicked by hiccough to unwind to the command-loop boundary
// after it has already rolled the failed command back in place. It is C's
// longjmp(mainloop, 1).
type mainloopJump struct{}

// error and its variants build sam's "?..."-prefixed message and call hiccough,
// exactly as error.c's error()/error_s()/error_c()/error_r(): hiccough rolls the
// command back in place, then unwinds to the command loop.
func (s *Sam) error(e Err)             { s.hiccough("?" + emsg[e]) }
func (s *Sam) error_s(e Err, a string) { s.hiccough("?" + emsg[e] + ` "` + a + `"`) }
func (s *Sam) error_c(e Err, c rune)   { s.hiccough("?" + emsg[e] + " `" + string(c) + "'") }

// error_r appends a rendered OS error (C's %r) to error_s's message.
func (s *Sam) error_r(e Err, a, r string) {
	s.hiccough("?" + emsg[e] + " \"" + a + "\": " + r)
}

func (s *Sam) warn(w Warn) {
	s.dprint("?warning: %s\n", wmsg[w])
}

func (s *Sam) warn_S(w Warn, a []rune) {
	s.dprint("?warning: %s `%s'\n", wmsg[w], string(a))
}

func (s *Sam) warn_SS(w Warn, a, b []rune) {
	s.dprint("?warning: %s: `%s' and `%s'\n", wmsg[w], string(a), string(b))
}

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
		r := []rune(str)
		if s.cmd != nil {
			s.loginsert(s.cmd, s.cmdpt, r)
		} else {
			s.cmdstr = s.insertString(s.cmdstr, r, Posn(len(s.cmdstr)))
		}
		s.cmdptadv += Posn(len(r))
	} else {
		s.Write(s.err, []byte(str))
	}
}
