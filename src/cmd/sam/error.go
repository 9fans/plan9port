package main

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

// samErr is the panic value used to model C's error()->hiccough()->longjmp.
// The message (already prefixed with '?') is printed by hiccough during the
// recover at the command-loop boundary; quiet means print nothing (the C
// hiccough(nil) case from whichfile).
type samErr struct {
	msg   string
	quiet bool
}

// fail* build the "?..."-prefixed message and panic, unwinding to the nearest
// recover (the mainloop boundary), exactly as C's error*() longjmp to mainloop.
func fail(e Err)             { panic(samErr{msg: "?" + emsg[e]}) }
func fail_s(e Err, a string) { panic(samErr{msg: "?" + emsg[e] + ` "` + a + `"`}) }
func fail_c(e Err, c int)    { panic(samErr{msg: "?" + emsg[e] + " `" + string(rune(c)) + "'"}) }
func fail_r(e Err, a, r string) {
	panic(samErr{msg: "?" + emsg[e] + " \"" + a + "\": " + r})
}

// quietHiccough is C's hiccough((char*)0): roll back without a message.
func quietHiccough() { panic(samErr{quiet: true}) }

func (s *Sam) warn(w Warn) {
	s.dprint("?warning: %s\n", wmsg[w])
}

// (C's warn_s is omitted: it has no caller in the ported engine.)

func (s *Sam) warn_S(w Warn, a *String) {
	s.dprint("?warning: %s `%s'\n", wmsg[w], runesToStr(a))
}

func (s *Sam) warn_SS(w Warn, a, b *String) {
	s.dprint("?warning: %s: `%s' and `%s'\n", wmsg[w], runesToStr(a), runesToStr(b))
}

// runesToStr renders all n runes of a String (C %.*S), unlike Strtoc which
// stops at the first NUL.
func runesToStr(a *String) string {
	return string(a.s)
}
