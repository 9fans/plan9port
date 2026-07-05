// Command gen writes the acme -D differential test scenarios as
// testdata/debug/*.txtar files (one scenario per file, a "script" member plus,
// for load tests, a "file" member). It is a one-shot generator: run it, then
// `go test ./cmd/acme -update` to fill in the "stdout" goldens from the C
// oracle. It re-creates the script-building helpers (editScript, mfScript, ...)
// that the old inline Go tests used so the generated scripts are byte-identical.
//
//	go run ./cmd/acme/gen
package main

import (
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"

	"9fans.net/plan9port/internal/testar"
	"golang.org/x/tools/txtar"
)

const outDir = "cmd/acme/testdata/debug"

// ---- script-building helpers (copied from the old *_test.go) ----

func itoa(n int) string { return fmt.Sprintf("%d", n) }

func escapeForScript(s string) string {
	r := strings.NewReplacer(`\`, `\\`, "\n", `\n`, "\t", `\t`)
	return r.Replace(s)
}

func editScript(content, cmd string) string {
	full := "Edit " + cmd
	return "win\ntype 1 " + content + "\ntag 1 " + escapeForScript(full) +
		"\nb2 1.tag 23 " + itoa(23+len([]rune(full))) + "\nprint 1\n"
}

func editScriptDot(content, cmd string, dot int) string {
	full := "Edit " + cmd
	return "win\ntype 1 " + content + "\ndot 1 " + itoa(dot) + " " + itoa(dot) +
		"\ntag 1 " + escapeForScript(full) +
		"\nb2 1.tag 23 " + itoa(23+len([]rune(full))) + "\nprint 1\n"
}

func editThen(content, cmd, after string) string {
	full := "Edit " + cmd
	return "win\ntype 1 " + content + "\ntag 1 " + escapeForScript(full) +
		"\nb2 1.tag 23 " + itoa(23+len([]rune(full))) + "\n" + after
}

func mfScript(cmd, dumpwho string) string {
	full := "Edit " + cmd
	end := 23 + len([]rune(full))
	return "win\nname 1 /a/one\nbody 1 hello\n" +
		"win\nname 2 /a/two\nbody 2 world\n" +
		"win\ntype 3 x\ntag 3 " + full +
		"\nb2 3.tag 23 " + itoa(end) + "\nprint " + dumpwho + "\n"
}

func addrScript(content string, q0, q1 int, cmd, pre, addr string) string {
	line := cmd + " 1"
	if pre != "" {
		line += " " + pre
	}
	line += " " + escapeForScript(addr)
	return "win\nbody 1 " + escapeForScript(content) +
		"\ndot 1 " + itoa(q0) + " " + itoa(q1) +
		"\n" + line + "\n"
}

func stackBody(n int) string {
	var b strings.Builder
	for k := 1; k <= n; k++ {
		b.WriteString("win\nbody ")
		b.WriteString(itoa(k))
		b.WriteString(" a\\nb\\nc\\nd\\ne\\nf\\ng\\nh\n")
	}
	return b.String()
}

func rep(s string, n int) string { return strings.Repeat(s, n) }

// ---- file emission ----

var used = map[string]bool{}

func sanitize(s string) string {
	var b strings.Builder
	for _, r := range s {
		switch {
		case r >= 'a' && r <= 'z', r >= '0' && r <= '9':
			b.WriteRune(r)
		case r >= 'A' && r <= 'Z':
			b.WriteRune(r + 32)
		}
	}
	out := b.String()
	if len(out) > 18 {
		out = out[:18]
	}
	if out == "" {
		out = "x"
	}
	return out
}

// emit writes one scenario file. extra holds optional members (e.g. "file").
func emit(name, script string, extra map[string]string) {
	// Guard against case-only collisions: testdata lives on case-insensitive
	// filesystems (macOS), where "indentON" and "indenton" are the same file.
	lc := strings.ToLower(name)
	if used[lc] {
		panic("duplicate (case-insensitive) scenario name: " + name)
	}
	used[lc] = true
	ar := &txtar.Archive{}
	ar.Files = append(ar.Files, txtar.File{Name: "script", Data: testar.Encode([]byte(script))})
	keys := make([]string, 0, len(extra))
	for k := range extra {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	for _, k := range keys {
		ar.Files = append(ar.Files, txtar.File{Name: k, Data: testar.Encode([]byte(extra[k]))})
	}
	path := filepath.Join(outDir, name+".txtar")
	if err := os.WriteFile(path, txtar.Format(ar), 0666); err != nil {
		panic(err)
	}
}

// emitSlice writes index-prefixed scenarios from a (description, script) list.
func emitSlice(group string, items []struct{ hint, script string }) {
	for i, it := range items {
		name := fmt.Sprintf("%s_%02d_%s", group, i, sanitize(it.hint))
		emit(name, it.script, nil)
	}
}

func emitMap(group string, m map[string]string) {
	keys := make([]string, 0, len(m))
	for k := range m {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	for _, k := range keys {
		emit(group+"_"+k, m[k], nil)
	}
}

func main() {
	if _, err := os.Stat("cmd/acme"); err != nil {
		fmt.Fprintln(os.Stderr, "run from the src/ module root")
		os.Exit(1)
	}
	os.RemoveAll(outDir)
	if err := os.MkdirAll(outDir, 0777); err != nil {
		panic(err)
	}

	genDriver()
	genEdit()
	genEditExec()
	genEditErr()
	genEditFile()
	genBuiltins()
	genAddr()
	genRegexp()
	genMultifile()
	genUndo()
	genZerox()
	genDclick()
	genBsw()
	genBuff()
	genWind()
	genGeometry()
	genCover()
	gen9p()

	fmt.Printf("wrote %d scenarios to %s\n", len(used), outDir)
}

func genDriver() {
	m := map[string]string{
		"one":          "win\nbody 1 hello world\nprint 1\n",
		"dot":          "win\nbody 1 hello world\ndot 1 0 5\nprint 1\n",
		"name":         "win\nname 1 /tmp/file.txt\nbody 1 some text\nprint 1\n",
		"twocol":       "win\nbody 1 first\nnewcol\nwin\nbody 2 second\ndump\n",
		"tag":          "win\nname 1 /a/b\ntag 1  hello\nprint 1.tag\n",
		"multiline":    "win\nbody 1 line1\\nline2\\nline3\nprint 1\n",
		"type":         "win\ntype 1 hello world\nkey 1 nl\ntype 1 second line\nprint 1\n",
		"typebs":       "win\ntype 1 hello\nkey 1 bs bs\nprint 1\n",
		"undo":         "win\ntype 1 hello\nb2 1.tag 11 15\nprint 1\n",
		"edit":         "win\ntype 1 alpha beta gamma\ntag 1 Edit ,s/a/X/g\nb2 1.tag 23 36\nprint 1\n",
		"move":         "win\ntype 1 hello world\nkey 1 left left left\nprint 1\n",
		"movends":      "win\ntype 1 ab\\ncd\\nef\nkey 1 home\nkey 1 end\nprint 1\n",
		"updown":       "win\ntype 1 ab\\ncd\\nef\nkey 1 up\nprint 1\n",
		"selovl":       "win\ntype 1 hello world bye\nb1 1 2 12\nb1 1 4 8\nprint 1\n",
		"selext":       "win\ntype 1 hello world bye\nb1 1 4 8\nb1 1 2 14\nprint 1\n",
		"redo":         "win\ntype 1 hello\nb2 1.tag 11 15\nb2 1.tag 11 15\nprint 1\n",
		"cutpaste":     "win\ntype 1 hello world\nb1 1 0 6\ntag 1 Cut Paste\nb2 1.tag 23 26\nb2 1.tag 27 32\nprint 1\n",
		"snarf":        "win\ntype 1 hello world\nb1 1 0 5\nb2 1.tag 5 10\nb1 1 6 11\ntag 1 Paste\nb2 1.tag 23 28\nprint 1\n",
		"del":          "win\nb2 1.tag 1 4\ndump\n",
		"delforce":     "win\ntype 1 hello\ntag 1 Delete\nb2 1.tag 23 29\ndump\n",
		"deltwo":       "win\nbody 1 keep\nnewcol\nwin\nb2 2.tag 1 4\ndump\n",
		"stack2":       "win\nbody 1 first\nwin\nbody 2 second\ndump\n",
		"stack3":       "win\nbody 1 one\nwin\nbody 2 two\nwin\nbody 3 three\ndump\n",
		"stackmany":    "win\nwin\nwin\nwin\nwin\nwin\nwin\nwin\nwin\nwin\nwin\nwin\nwin\nwin\nwin\nwin\nwin\nwin\nwin\nwin\ndump\n",
		"colclosemid":  "win\nbody 1 aaa\nwin\nbody 2 bbb\nwin\nbody 3 ccc\nb2 2.tag 1 4\ndump\n",
		"colcloselast": "win\nbody 1 aaa\nwin\nbody 2 bbb\nwin\nbody 3 ccc\nb2 3.tag 1 4\ndump\n",
		"newcolmulti":  "win\nbody 1 aaa\nwin\nbody 2 bbb\nnewcol\ndump\n",
		"allkeys":      "win\ntype 1 a\\nb\\nc\\nd\\ne\nkey 1 del esc tab right down pgup pgdn\nprint 1\n",
		"colcmd":       "win\nnewcol\ncol 0\nwin\nbody 2 second\ndump\n",
		"escapes":      "win\nbody 1 x\\\\y\\qz\nprint 1\n",
		"emptybody":    "win\nbody 1\nprint 1\n",
		"blanklines":   "win\n\n#a comment\nbody 1 hi\nprint 1\n",
		"puttag":       "win\ntype 1 hello\nname 1 /tmp/f\nprint 1.tag\n",
		"renametwice":  "win\nname 1 /tmp/a\nname 1 /tmp/a\nprint 1.tag\n",
		"typeover":     "win\ntype 1 hello world\nb1 1 0 5\ntype 1 X\nprint 1\n",
		"esc":          "win\ntype 1 hello\nkey 1 esc\nprint 1\n",
		"bsstart":      "win\ntype 1 hi\nkey 1 left left bs\nprint 1\n",
		"tagmove":      "win\ntype 1.tag hi\nkey 1.tag left\nprint 1.tag\n",
		"zerox":        "win\ntype 1 hello world\ntag 1 Zerox\nb2 1.tag 23 28\ndump\n",
		"zeroxundo":    "win\ntype 1 hello\ntag 1 Zerox\nb2 1.tag 23 28\nb2 2.tag 11 15\ndump\n",
		"chordedit":    "win\ntype 1 hello\ntag 1 Edit\nwin\nbody 2 ,s/h/H/\nb2arg 1.tag 23 27 2 0 7\nprint 1\n",
		"execpoint":    "win\nbody 1 xyzcmd\nb2 1 3 3\nprint 1\n",
		"execnotcmd":   "win\nbody 1 zzword\nb2 1 0 6\nprint 1\n",
		"execinsel":    "win\nbody 1 Undo\nb1 1 0 4\nb2 1 2 2\nprint 1\n",
		"execblank":    "win\nbody 1 a  b\nb2 1 2 2\nprint 1\n",
		"lookupblank":  "win\nbody 1 a   b\nb2 1 1 4\nprint 1\n",
		"pasteempty":   "win\nbody 1 Paste\nb2 1 0 5\nprint 1\n",
		"editnl":       "win\ntype 1 hello\\nworld\ndot 1 8 8\ntag 1 Edit\nwin\nbody 2 \\n\nb2arg 1.tag 23 27 2 0 1\nprint 1\n",
		"editnlnext":   "win\ntype 1 hello\\nworld\ndot 1 0 6\ntag 1 Edit\nwin\nbody 2 \\n\nb2arg 1.tag 23 27 2 0 1\nprint 1\n",
		"snarfbody":    "win\nbody 1 hello\ntag 1 Snarf\nb1 1 0 3\nb2 1.tag 18 23\nprint 1\n",
		"snarftag":     "win\nbody 1 hi\ntag 1 Snarf\nb1 1.tag 1 4\nb1 1 0 0\nb2 1.tag 18 23\nprint 1.tag\n",
	}
	emitMap("driver", m)

	// TestStackWindows
	for _, n := range []int{6, 10, 16} {
		emit("driver_stackwin"+itoa(n), stackBody(n)+"dump\n", nil)
	}

	// TestTextScroll
	var b strings.Builder
	b.WriteString("win\ntype 1 ")
	for i := 0; i < 200; i++ {
		b.WriteString("line")
		b.WriteString(itoa(i))
		b.WriteString("\\n")
	}
	b.WriteString("\n")
	b.WriteString("key 1 home\n")
	b.WriteString("key 1 pgdn pgdn pgup down down up\n")
	b.WriteString("type 1 Z\n")
	b.WriteString("b1 1 10 900\n")
	b.WriteString("key 1 bs\n")
	b.WriteString("print 1\n")
	emit("driver_textscroll", b.String(), nil)
}

func genEdit() {
	cmds := []struct{ content, cmd string }{
		{"a\\nb\\nc\\nd", "$-1d"},
		{"abcabc", "?b?d"},
		{"a\\nb\\nc", "2m0"},
		{"a\\nb\\nc", "1t$"},
		{"hello world", ",s/o/0/g"},
		{"a b c d", ",x/[a-z]/ s/./X/"},
		{"foo\\nbar\\nbaz", "/bar/d"},
		{"abcde", ".,$d"},
		{"hello", "99d"},
		{"hello", "/xyz/d"},
		{"hello", ",s/z/Q/g"},
		{"xyz", "{,s/x/X/ ,s/y/Y/ }"},
		{"hello", "1u"},
		{"hello", "saXbX"},
		{"a\\nb\\nc\\nd\\ne", "2,4x/./ s/./Z/"},
		{"a\\nb\\nc", "2"},
		{"ab\\ncd", ",x/^/ a/>/"},
		{"hello", "1m0"},
		{"abc", "5,2d"},
		{"abc", "#99d"},
		{"abc", "Q"},
		{"abc", "}"},
		{"abc", ",m"},
		{"a\\nb\\nc", "g/b/d"},
		{"abcabc", ",x/b/"},
		{"aab", ",s/a*/-/g"},
		{"aa", ",s/b*/-/g"},
		{"abab", ",s2/a/X/"},
		{"cat", ",s/a/[&]/"},
		{"hello", ",s/[/X/"},
		{"foo\\nbar", ",g/[/d"},
		{"foo bar", ",x/[/d"},
		{"a\\nb\\nc", ",x/^/ a/>/"},
		{"foo", ",y/o/d"},
		{"aaab", ",x/a*/d"},
		{"a\\nb\\nc\\nd", "1m3"},
		{"a\\nb\\nc\\nd", "2,3m2"},
		{"abcdef", "#3+#2d"},
		{"abcdef", "#5-#2d"},
		{"a\\nb\\nc", "1;3d"},
		{"abc", "'a"},
		{"a\\nb\\nc\\nd\\ne", "3-2d"},
		{"a\\nb\\nc\\nd\\ne", "4+d"},
		{"foo\\nbar\\nbaz", ",x d"},
		{"abcabc", "?b?+#1d"},
		{"a\\nb\\nc", "2-0d"},
		{"a\\nb\\nc", "1+0d"},
		{"abc", "/[/d"},
		{"a\\nb\\nc\\nd\\ne\\nf", "6-10d"},
		{"a\\nb\\nc", "3,1d"},
		{"hello", "w x"},
		{"abcabc", "?zzz?d"},
		{"a\\nb\\nc\\nd", "1,2x d"},
		{"a\\nb\\nc", "1,2{ d }"},
		{"abcabc", "/x*/d"},
		{"abcabc", "?x*?d"},
		{"a\\nb\\nc\\nd", "{ 3a/Y/\n1a/X/\n}"},
		{"a\\nb\\nc\\nd", "{ 3c/Y/\n1c/X/\n}"},
		{"a\\nb\\nc\\nd", "{ 3d\n1d\n}"},
		{"a\\nb\\nc", "{ 1a/X/\n1a/Y/\n}"},
		{"abc", "#2c//"},
		{"abc", ",s/x*//g"},
	}
	var items []struct{ hint, script string }
	for _, c := range cmds {
		items = append(items, struct{ hint, script string }{c.cmd, editScript(c.content, c.cmd)})
	}
	emitSlice("edit", items)
}

func genEditExec() {
	cases := []struct {
		content, cmd string
		dot          int
	}{
		{"hello\\nworld\\n", ",d", 0},
		{"hello\\nworld\\n", "1d", 0},
		{"hello\\nworld\\n", "2d", 0},
		{"abcabc", ",s/b/X/g", 0},
		{"abcabc", ",s/b/X/", 0},
		{"hello", "a/!!!/", 5},
		{"hello", "i/>>/", 0},
		{"hello world", "/world/c/there/", 0},
		{"one two three", `,s/(t..) (t....)/\2 \1/`, 0},
		{"foo bar baz", ",x/[a-z]+/d", 0},
		{"a1b2c3", ",x/[0-9]/d", 0},
		{"foo bar foo", ",g/bar/d", 0},
		{"foo bar foo", ",v/bar/d", 0},
		{"a\\nb\\nc\\n", "1,2d", 0},
		{"a\\nb\\nc\\n", "2m0", 0},
		{"a\\nb\\nc\\n", "1t$", 0},
		{"abcde", "#2a/XY/", 0},
		{"abcde", "#2c/Q/", 0},
		{"abc", "$a/END/", 0},
		{"foo bar baz", ",x/[a-z]+/ g/a/ d", 0},
		{"a1b2c3d4", ",s/[0-9]//g", 0},
		{"x\\ny\\nz\\n", ",x d", 0},
		{"a\\nb\\nc\\n", "1a\nINS\n.\n", 0},
		{"a\\nb\\nc\\n", "1,2c\nQ\n.\n", 0},
		{"cat", `,s/a/\z/`, 0},
		{"hello", `,s/l/X\nY/`, 0},
		{"a\\nb\\nc\\n", "{\n1d\n2d\n}", 0},
		{"axb\\n", `,s/a\/b/Z/`, 0},
		{"a/b\\n", `,s/a\/b/Z/`, 0},
		{"1 2\\n3 4\\n", "2/3/ d", 0},
		{"a\\nb\\nc\\n", "1,2{\nd\n}", 0},
		{"abc", "/z*/d", 1},
		{"abc", "?z*?d", 2},
		{"abc", "?^?d", 0},
		{"a\\nb\\nc\\n", "1m1", 0},
		{"ab\\ncd", ",x d", 0},
		{"a\\nb\\nc\\nd\\n", ",{\nd\nd\n}", 0},
		{"line1\\nline2\\nline3\\nline4\\nline5\\n", "3", 0},
		{"line1\\nline2\\nline3\\nline4\\nline5\\n", "#5", 0},
		{"line1\\nline2\\nline3\\nline4\\nline5\\n", "$", 0},
		{"line1\\nline2\\nline3\\nline4\\nline5\\n", "/line3/", 0},
		{"line1\\nline2\\nline3\\nline4\\nline5\\n", "1,3", 0},
		{"line1\\nline2\\nline3\\nline4\\nline5\\n", ".", 6},
		{"line1\\nline2\\nline3\\nline4\\nline5\\n", "2,4", 0},
	}
	var items []struct{ hint, script string }
	for _, c := range cases {
		items = append(items, struct{ hint, script string }{c.cmd, editScriptDot(c.content, c.cmd, c.dot)})
	}
	emitSlice("editexec", items)
}

func genEditErr() {
	cmds := []string{
		"cd/x/y/",
		"1b",
		"g",
		"=",
		"w foo bar",
		"$.",
		`+"a"d`,
		",,",
		"Q",
		",g/foo",
		`,s/a\\b/Z/`,
		`"x".d`,
		`"x"1d`,
		",c/x\\\n",
		" ",
		",s//X/",
		"1a\nfoo",
	}
	var items []struct{ hint, script string }
	for _, cmd := range cmds {
		items = append(items, struct{ hint, script string }{cmd, editScript("hello\\nworld", cmd)})
	}
	emitSlice("editerr", items)
}

// genEditFile covers the file-oriented edit commands (=, f, b, u, e, r, B, D).
// The command text is supplied as a 2-1 chord argument from a separate window,
// so the executing window's tag is just "Edit": at runes 23..27 when the window
// is dirty (a typed body adds " Undo"), or 18..22 when it is clean.
func genEditFile() {
	// '=' reports dot's position (line / #chars / line+#chars). The report goes
	// to the +Errors window in C but to stderr in this port, so it is not in the
	// -D stdout; the command is still exercised (print shows the unchanged body).
	emit("editf_eq", "win\ntype 1 hello\ntag 1 Edit\nwin\nbody 2 =\n"+
		"b2arg 1.tag 23 27 2 0 1\nprint 1\n", nil)
	emit("editf_eqchars", "win\ntype 1 hello\ntag 1 Edit\nwin\nbody 2 =#\n"+
		"b2arg 1.tag 23 27 2 0 2\nprint 1\n", nil)
	emit("editf_eqlinechars", "win\ntype 1 hello\ntag 1 Edit\nwin\nbody 2 =+\n"+
		"b2arg 1.tag 23 27 2 0 2\nprint 1\n", nil)
	// 'f' sets (and prints) the file name; print shows the renamed window.
	emit("editf_setname", "win\ntype 1 hello\ntag 1 Edit\nwin\nbody 2 f newname\n"+
		"b2arg 1.tag 23 27 2 0 9\nprint 1\n", nil)
	// 'u' undoes a prior edit (here the preceding substitution).
	emit("editf_undo", "win\ntype 1 hello\ntag 1 Edit\nwin\nbody 2 ,s/l/L/g\n"+
		"b2arg 1.tag 23 27 2 0 8\nwin\nbody 3 u\nb2arg 1.tag 23 27 3 0 1\nprint 1\n", nil)
	// 'b' switches the current file to a named window (its name is printed to
	// stderr, as for '='); the executing window's body is unchanged.
	emit("editf_switchfile", "win\ntype 1 x\ntag 1 Edit\nwin\nname 2 /a/two\nbody 2 yy\n"+
		"win\nbody 3 b /a/two\nb2arg 1.tag 23 27 3 0 8\nprint 1\n", nil)
	// 'r' reads a file's text into dot.
	emit("editf_readdot", "win\ntype 1 X\ndot 1 1 1\ntag 1 Edit\nwin\nbody 2 r f\n"+
		"b2arg 1.tag 23 27 2 0 3\nprint 1\n", map[string]string{"file": "ins\n"})
	// 'e' replaces the whole (clean) file with a file's contents.
	emit("editf_readall", "win\nname 1 $FILE\nwrite 1/ctl \"get\"\ntag 1 Edit\n"+
		"win\nbody 2 e\nb2arg 1.tag 19 23 2 0 1\nprint 1\n",
		map[string]string{"file": "data\n"})
	// 'B' opens a file as a new window.
	emit("editf_openfiles", "win\ntype 1 x\ntag 1 Edit\nwin\nbody 2 B f\n"+
		"b2arg 1.tag 23 27 2 0 3\nread 3/body\n", map[string]string{"file": "bb\n"})
	// 'D' closes a (clean, named) window other than the executing one.
	emit("editf_delwin", "win\ntype 1 x\ntag 1 Edit\nwin\nname 2 $FILE\n"+
		"write 2/ctl \"get\"\nwin\nbody 3 D f\nb2arg 1.tag 23 27 3 0 3\ndump\n",
		map[string]string{"file": "z\n"})

	// the |, <, > pipe commands run an external program (synchronously): | pipes
	// dot through it, < replaces dot with its output, > sends dot to it (output
	// to +Errors, so the body is unchanged).
	emit("editf_pipe", editScript("hello", ",|tr a-z A-Z"), nil)
	emit("editf_pipein", editScript("x", ",<echo hi"), nil)
	emit("editf_pipeout", editScript("hello", ",>cat"), nil)
	emit("editf_pipenoarg", editScript("hello", ",|"), nil) // no command -> error

	// '=' formatting branches over a multi-line selection (the report goes to
	// stderr, so these exercise the formatting, not the output).
	eqsel := func(name, q1, cmd string, n int) {
		emit(name, "win\ntype 1 a\\nb\\nc\ndot 1 0 "+q1+"\ntag 1 Edit\n"+
			"win\nbody 2 "+cmd+"\nb2arg 1.tag 23 27 2 0 "+itoa(n)+"\nprint 1\n", nil)
	}
	eqsel("editf_eqsel", "3", "=", 1)           // multi-line: l2 != l1
	eqsel("editf_eqselnl", "2", "=", 1)         // selection ends in newline: l2--
	eqsel("editf_eqcharssel", "3", "=#", 2)     // #chars range: ,#q1
	eqsel("editf_eqlinecharssel", "3", "=+", 2) // line+#chars range
	// '=' with a bad (non-newline) argument.
	emit("editf_eqbad", "win\ntype 1 hi\ntag 1 Edit\nwin\nbody 2 =x\n"+
		"b2arg 1.tag 23 27 2 0 2\nprint 1\n", nil)
	// 'f' to an absolute name (cmdname's '/'-prefixed branch).
	emit("editf_fabs", "win\ntype 1 x\ntag 1 Edit\nwin\nbody 2 f /abs\n"+
		"b2arg 1.tag 23 27 2 0 6\nprint 1\n", nil)
	// 'f' to a name another window already holds (duplicate-name warning).
	emit("editf_fdup", "win\nname 1 /a/x\nwin\ntype 2 y\ntag 2 Edit\n"+
		"win\nbody 3 f /a/x\nb2arg 2.tag 23 27 3 0 6\nprint 2\n", nil)
	// 'w' with no name on an unnamed window.
	emit("editf_wnoname", "win\ntype 1 x\ntag 1 Edit\nwin\nbody 2 w\n"+
		"b2arg 1.tag 23 27 2 0 1\nprint 1\n", nil)
	// 'b' naming a window that does not exist.
	emit("editf_bnone", "win\ntype 1 x\ntag 1 Edit\nwin\nbody 2 b nope\n"+
		"b2arg 1.tag 23 27 2 0 6\nprint 1\n", nil)
	// 'D' naming a window that does not exist.
	emit("editf_dnone", "win\ntype 1 x\ntag 1 Edit\nwin\nbody 2 D nope\n"+
		"b2arg 1.tag 23 27 2 0 6\nprint 1\n", nil)
	// 'B' with no name argument, and with a blank argument (empty new window).
	emit("editf_bnoname", "win\ntype 1 x\ntag 1 Edit\nwin\nbody 2 B\n"+
		"b2arg 1.tag 23 27 2 0 1\nprint 1\n", nil)
	emit("editf_bblank", "win\ntype 1 x\ntag 1 Edit\nwin\nbody 2 B  \n"+
		"b2arg 1.tag 23 27 2 0 3\ndump\n", nil)
	// 'e' on a dirty window is refused (winclean fails; a >=100-rune unnamed
	// body makes winclean report "modified" rather than silently allowing it).
	emit("editf_edirty", "win\ntype 1 "+rep("a", 100)+"\ntag 1 Edit\nwin\nbody 2 e\n"+
		"b2arg 1.tag 23 27 2 0 1\nread 1/ctl\n", nil)
	// 'e' with no name on a clean unnamed window (tag "Edit" at the clean offset 18).
	emit("editf_enoname", "win\ntag 1 Edit\nwin\nbody 2 e\n"+
		"b2arg 1.tag 18 22 2 0 1\nprint 1\n", nil)
	// 'r' reading a file with NUL bytes elides them (and warns).
	emit("editf_rnul", "win\ntype 1 X\ndot 1 1 1\ntag 1 Edit\nwin\nbody 2 r f\n"+
		"b2arg 1.tag 23 27 2 0 3\nprint 1\n", map[string]string{"file": "a\x00b\n"})
}

func genBuiltins() {
	emitMap("builtins", map[string]string{
		"newcol":         "win\nbody 1 Newcol\nb2 1 0 6\ndump\n",
		"sort":           "win\nname 1 /b/x\nwin\nname 2 /a/y\nbody 1 Sort\nb2 1 0 4\ndump\n",
		"delcol":         "win\nbody 1 x\nnewcol\nwin\nbody 2 Delcol\nb2 2 0 6\ndump\n",
		"id":             "win\nbody 1 ID\nb2 1 0 2\nprint 1\n",
		"tab":            "win\nbody 1 Tab 8\nb2 1 0 5\nprint 1\n",
		"tabnoarg":       "win\nbody 1 Tab\nb2 1 0 3\nprint 1\n",
		"indenton":       "win\nbody 1 Indent on\nb2 1 0 9\nprint 1\n",
		"indentoff":      "win\nbody 1 Indent off\nb2 1 0 10\nprint 1\n",
		"indentglobalon": "win\nbody 1 Indent ON\nb2 1 0 9\nprint 1\n",
		// Send appends the selection (snarfed) to the end of the body, ensuring a
		// trailing newline; the "Send" tag entry sits at runes 23..27.
		"send":   "win\ntype 1 abc\nb1 1 0 3\ntag 1 Send\nb2 1.tag 23 27\nprint 1\n",
		"sendnl": "win\ntype 1 ab\\n\nb1 1 0 3\ntag 1 Send\nb2 1.tag 23 27\nprint 1\n",
	})
	// Incl adds a per-window include directory; a later <f> look resolves through
	// it. The "Incl ." menu entry appended to a dirty window's tag is at 23..29.
	emit("builtins_incl", "win\ntype 1 <f>\ntag 1 Incl .\nb2 1.tag 23 29\n"+
		"open 1/event\nb3 1 1 1\nread 1/event\n", map[string]string{"file": "x\n"})
	// Incl error/edge paths (warnings go to stderr; the body is unchanged):
	// a directory supplied as a 2-1 chord argument,
	emit("builtins_inclchord", "win\ntype 1 x\ntag 1 Incl\nwin\nbody 2 .\n"+
		"b2arg 1.tag 23 27 2 0 1\nprint 1\n", nil)
	// a non-directory, an absolute miss, and a relative miss.
	emit("builtins_inclnotdir", "win\ntype 1 x\ntag 1 Incl f\nb2 1.tag 23 29\nprint 1\n",
		map[string]string{"file": "y\n"})
	emit("builtins_inclabs", "win\ntype 1 x\ntag 1 Incl /nope\nb2 1.tag 23 33\nprint 1\n", nil)
	emit("builtins_inclrel", "win\ntype 1 x\ntag 1 Incl nope\nb2 1.tag 23 32\nprint 1\n", nil)

	// Font changes the body font; reading ctl shows the new font name. Loading a
	// font file (here euro.7, distinct from the pinned euro.8),
	fcmd := "Font /usr/local/plan9/font/lucsans/euro.7.font"
	emit("builtins_font", "win\nbody 1 "+fcmd+"\nb2 1 0 "+itoa(len([]rune(fcmd)))+"\nread 1/ctl\n", nil)
	// the fix/var flag (no file: the pinned default), and no argument (toggle).
	emit("builtins_fontvar", "win\nbody 1 Font var\nb2 1 0 8\nread 1/ctl\n", nil)
	emit("builtins_fontnoarg", "win\nbody 1 Font\nb2 1 0 4\nread 1/ctl\n", nil)
	// loading the same font twice hits the font cache the second time,
	emit("builtins_fonttwice", "win\nbody 1 "+fcmd+"\nb2 1 0 "+itoa(len([]rune(fcmd)))+
		"\nb2 1 0 "+itoa(len([]rune(fcmd)))+"\nread 1/ctl\n", nil)
	// a flag plus a file saves it as the new default (the save path),
	fvcmd := "Font var /usr/local/plan9/font/lucsans/euro.7.font"
	emit("builtins_fontsave", "win\nbody 1 "+fvcmd+"\nb2 1 0 "+itoa(len([]rune(fvcmd)))+
		"\nread 1/ctl\n", nil)
	// and an unopenable font is reported (to stderr) and leaves the font as is.
	emit("builtins_fontbad", "win\nbody 1 Font /nope.font\nb2 1 0 15\nread 1/ctl\n", nil)

	// a window named for a directory loads its (columnated) listing. Populate the
	// working directory with two files via Put, then get the directory ".".
	dirfiles := "win\nname 1 foo\ntype 1 x\nwrite 1/ctl \"put\"\n" +
		"win\nname 2 bar\ntype 2 y\nwrite 2/ctl \"put\"\n"
	emit("builtins_dir", dirfiles+"win\nname 3 .\nwrite 3/ctl \"get\"\nread 3/body\nread 3/tag\n", nil)

	// ^F filename completion. Populate the working directory with two files via
	// Put, then complete a partial name against them.
	mkfiles := "win\nname 1 foo\ntype 1 x\nwrite 1/ctl \"put\"\n" +
		"win\nname 2 foobar\ntype 2 y\nwrite 2/ctl \"put\"\n"
	// a prefix shared by two files advances to the common prefix,
	emit("builtins_complete", mkfiles+"win\ntype 3 fo\nkey 3 ^f\nprint 3\n", nil)
	// a prefix unique to one file completes it fully (with a trailing space),
	emit("builtins_completeone", mkfiles+"win\ntype 3 foob\nkey 3 ^f\nprint 3\n", nil)
	// and a prefix matching nothing leaves the text unchanged (warns to stderr).
	emit("builtins_completenone", mkfiles+"win\ntype 3 zzz\nkey 3 ^f\nprint 3\n", nil)
	// a prefix that is already the common prefix of several files lists them,
	emit("builtins_completelist", mkfiles+"win\ntype 3 foo\nkey 3 ^f\nprint 3\n", nil)
	// completion after a blank skips back only to the blank,
	emit("builtins_completespace", mkfiles+"win\ntype 3 x fo\nkey 3 ^f\nprint 3\n", nil)
	// a path element directs completion at that (here missing) subdirectory,
	emit("builtins_completepath", mkfiles+"win\ntype 3 sub/fo\nkey 3 ^f\nprint 3\n", nil)
	// and ^F in the middle of a word is a no-op.
	emit("builtins_completemid", mkfiles+"win\ntype 3 foo\nb1 3 1 1\nkey 3 ^f\nprint 3\n", nil)
}

func genAddr() {
	const lines = "line1\nline2\nline3\nline4\nline5\n"
	type tc struct {
		content  string
		q0, q1   int
		cmd, pre string
		addr     string
	}
	cases := []tc{
		{lines, 0, 0, "addr", "", "3"},
		{lines, 0, 0, "addr", "", "#5"},
		{lines, 0, 0, "addr", "", "$"},
		{lines, 0, 0, "addr", "", "0"},
		{lines, 0, 0, "addr", "", "/line3/"},
		{lines, 0, 0, "addr", "", "1,2"},
		{lines, 6, 11, "addr", "", "."},
		{lines, 0, 0, "addr", "", "#3,#7"},
		{lines, 0, 0, "addr", "", "2+1"},
		{lines, 0, 0, "addr", "", "3-1"},
		{lines, 6, 11, "addr", "", ".+1"},
		{lines, 0, 0, "addr", "", "0+1"},
		{lines, 0, 0, "addr", "", "#5+#3"},
		{lines, 0, 0, "addr", "", "/line2/+/line4/"},
		{lines, 30, 30, "addr", "", "?line2"},
		{lines, 0, 0, "addr", "", "1;3"},
		{lines, 0, 0, "addr", "", ",3"},
		{lines, 0, 0, "addr", "", "3,"},
		{lines, 0, 0, "addr", "", "2++"},
		{lines, 0, 0, "addr", "", "1+"},
		{lines, 0, 0, "addr", "", "#12"},
		{lines, 0, 0, "addr", "", "6"},
		{lines, 0, 0, "addr", "", "#0-#1"},
		{lines, 14, 14, "addr", "", ".-1"},
		{lines, 0, 0, "addr", "", "3z"},
		{lines, 0, 0, "addr", "", "3."},
		{lines, 0, 0, "addr", "", "#"},
		{lines, 0, 0, "addr", "", "#z"},
		{lines, 0, 0, "addr", "", "99"},
		{lines, 0, 0, "addr", "", "#100"},
		{lines, 0, 0, "addr", "", "2-5"},
		{lines, 0, 0, "addr", "", "/zzz/"},
		{lines, 0, 0, "addr", "", "/[/"},
		{lines, 0, 0, "addr", "", "//"},
		{lines, 0, 0, "addr", "", "/a)b/"},
		{lines, 0, 0, "addr", "", "/[a-/"},
		{lines, 0, 0, "addr", "", `/[\`},
		{lines, 0, 0, "addr", "", "/[-a]/"},
		{lines, 0, 0, "addr", "", "/[a-]/"},
		{lines, 0, 0, "addr", "", "/a|/"},
		{"abc abc abc", 11, 11, "addrrev", "", "/abc/"},
		{"abc abc abc", 0, 0, "addrlim", "0 5", "/abc/"},
		{"a.b axb a\\b", 0, 0, "addr", "", "/ax\nb"},
		{"a.b axb a\\b", 0, 0, "addr", "", `/a\.b/`},
		{"a.b axb a\\b", 0, 0, "addr", "", `/a\`},
	}
	var items []struct{ hint, script string }
	for _, c := range cases {
		items = append(items, struct{ hint, script string }{c.cmd + c.addr, addrScript(c.content, c.q0, c.q1, c.cmd, c.pre, c.addr)})
	}
	emitSlice("addr", items)
}

func genRegexp() {
	cmds := []struct{ content, cmd string }{
		{"aabbbc", ",s/b+/[&]/"},
		{"abc123def", ",s/[0-9]+/[&]/"},
		{"aayb", ",s/x|y/[&]/"},
		{"zabcz", ",s/a.c/[&]/"},
		{"a\\nba", ",s/^a/[&]/"},
		{"abc\\nx", ",s/c$/[&]/"},
		{"xxabbbcx", ",s/ab*c/[&]/"},
		{"acabbc", ",s/ab+c/[&]/"},
		{"axabcx", ",s/ab?c/[&]/"},
		{"12abc34", ",s/[^0-9]+/[&]/"},
		{"xxabcyy", ",s/[a-c]+/[&]/"},
		{"abXYZab", ",s/[^abc]+/[&]/"},
		{"abc\\ndefg", ",s/^def/[&]/"},
		{"xabc\\ny", ",s/abc$/[&]/"},
		{"xabbbcy", `,s/a(b+)c/[\1]/`},
		{"xababy", `,s/(ab)+/[\1]/`},
		{"xx(ab)cyy", `,s/\(([a-z]+)\)/[\1]/`},
		{"xa.byab", `,s/a\.b/D/`},
		{"xa*bx", `,s/a\*b/E/`},
		{"xa\\nbx", `,s/a\nb/L/`},
		{"a]x]b", `,s/[\]x]+/Y/`},
		{"ax\\nxb", `,s/[\nx]+/Y/`},
		{"zaz", `,s/(((((((((((a)))))))))))/[\1]/`},
		{"abXabYab", "?ab?d"},
		{"abXabYab", "?ab?"},
		{"xa\\nyab", "?^a?d"},
		{"abc\\ndef", "?d.f?"},
		{"abc\\ndef", "?[d-f]+?"},
		{"abc\\ndef", "?c$?"},
		{"hello", ",s/a(b/x/"},
		{"hello", ",s/[ab/x/"},
		{"hello", ",s/*ab/x/"},
		{"x", ",s/" + rep("a", 3000) + "/y/"},
		{rep("a", 20), ",s/" + rep("a*", 150) + "/X/"},
		{rep("a", 20), ",s/" + rep(".*", 150) + "/X/"},
		{rep("a", 20), ",s/" + rep("[a]*", 150) + "/X/"},
		{rep("a", 20), ",s/" + rep("[^z]*", 130) + "/X/"},
		{rep("a", 20), ",s/" + rep("(a|b)*", 60) + "/X/"},
		{rep("a", 20), "?" + rep("a*", 150) + "?d"},
		{rep("a", 20), "?" + rep(".*", 150) + "?d"},
		{rep("a", 20), "?" + rep("[a]*", 150) + "?d"},
		{rep("a", 20), "?" + rep("[^z]*", 130) + "?d"},
		{rep("a", 20), "?" + rep("[^z]", 130) + "?d"},
		{rep("a", 20), "?" + rep("(a|b)*", 60) + "?d"},
		{"b\\nabc", "?^a?d"},
	}
	var items []struct{ hint, script string }
	for i, c := range cmds {
		hint := c.cmd
		if len(hint) > 20 {
			hint = "big" + itoa(i)
		}
		items = append(items, struct{ hint, script string }{hint, editScript(c.content, c.cmd)})
	}
	emitSlice("regexp", items)
}

func genMultifile() {
	addr := []struct{ cmd, who string }{
		{`"two"d`, "2"},
		{`"two",d`, "2"},
		{`"two"w`, "2"},
		{`"two",s/world/WORLD/`, "2"},
	}
	var items []struct{ hint, script string }
	for _, c := range addr {
		items = append(items, struct{ hint, script string }{c.cmd, mfScript(c.cmd, c.who)})
	}
	emitSlice("mfaddr", items)

	emitMap("mfx", map[string]string{
		"x": mfScript(`X/[ot]/ ,s/o/0/g`, "1") + "print 2\n",
		"y": mfScript(`Y/zzz/ ,s/o/0/g`, "1") + "print 2\n",
		"xundo": "win\nname 1 /a/one\nbody 1 foo\n" +
			"win\nname 2 /a/two\nbody 2 woo\n" +
			"win\ntype 3 x\ntag 3 Edit X/[ot]/ ,s/o/0/g\n" +
			"b2 3.tag 23 " + itoa(23+len([]rune("Edit X/[ot]/ ,s/o/0/g"))) + "\n" +
			"b2 1.tag 17 21\ndump\n",
		"xnopat":   mfScript(`X d`, "1"),
		"toomany":  mfScript(`"."d`, "1"),
		"badre":    mfScript(`"["d`, "1"),
		"nomatchx": mfScript(`X/zzz/ d`, "1"),
		"zeroxX": "win\nname 1 /a/one\nbody 1 hello\nwin\nname 2 /a/two\nbody 2 world\n" +
			"tag 1 Zerox\nb2 1.tag 24 29\nwin\ntype 4 x\ntag 4 Edit X/o/ ,s/o/0/g\n" +
			"b2 4.tag 23 " + itoa(23+len([]rune("Edit X/o/ ,s/o/0/g"))) + "\ndump\n",
		"zeroxQ": "win\nname 1 /a/one\nbody 1 hello\nwin\nname 2 /a/two\nbody 2 world\n" +
			"tag 1 Zerox\nb2 1.tag 24 29\nwin\ntype 4 x\ntag 4 Edit \"two\"d\n" +
			"b2 4.tag 23 " + itoa(23+len([]rune(`Edit "two"d`))) + "\ndump\n",
	})
}

func genUndo() {
	emitMap("undo", map[string]string{
		"typeundo": "win\ntype 1 hello world\nundo 1\nprint 1\n",
		"typeredo": "win\ntype 1 hello\nundo 1\nredo 1\nprint 1\n",
		"multiundo": "win\ntype 1 one\nkey 1 nl\ntype 1 two\nkey 1 nl\ntype 1 three\n" +
			"undo 1\nundo 1\nprint 1\n",
		"multiredo": "win\ntype 1 one\nkey 1 nl\ntype 1 two\n" +
			"undo 1\nundo 1\nredo 1\nprint 1\n",
		"delundo":     "win\ntype 1 hello world\nb1 1 0 6\nkey 1 bs\nundo 1\nprint 1\n",
		"editsubundo": editThen("aaaa", ",s/a/XY/g", "undo 1\nprint 1\n"),
		"editappundo": editThen("a\\nb\\nc", ",x/^/ a/>/", "undo 1\nprint 1\n"),
		"editdelundo": editThen("a\\nb\\nc\\nd", "2,3d", "undo 1\nredo 1\nprint 1\n"),
		"editreundo":  editThen("hello", ",s/l/L/g", "undo 1\nredo 1\nundo 1\nprint 1\n"),
		"epsclear":    "win\ntype 1 abc\nundo 1\ntype 1 X\nprint 1\n",
		"zeroxdel":    "win\ntype 1 hello\ntag 1 Zerox\nb2 1.tag 23 28\nb2 2.tag 1 4\ndump\n",
		"nameundo":    "win\ntype 1 hello\ntype 1.tag /tmp/foo\nundo 1\nprint 1\n",
		"undonone":    "win\ntype 1 hi\nredo 1\nprint 1\n",
		"redonone":    "win\nundo 1\nprint 1\n",
	})
}

func genZerox() {
	emitMap("zerox", map[string]string{
		"sync": "win\ntype 1 hello world\ntag 1 Zerox\nb2 1.tag 23 28\n" +
			"type 1 AB\nkey 1 bs\nb1 1 0 2\nkey 1 bs\ndump\n",
		"editcopy": "win\ntype 1 hello\ntag 1 Zerox\nb2 1.tag 23 28\n" +
			"type 2  world\nb1 2 0 5\nkey 2 bs\ndump\n",
		"editsub": "win\ntype 1 hello\ntag 1 Zerox\nb2 1.tag 23 28\n" +
			"tag 1 Edit ,s/l/LL/g\nb2 1.tag 28 42\ndump\n",
		"cachebs": "win\ntype 1 hi\ntag 1 Zerox\nb2 1.tag 23 28\ntype 1 AB\\bC\ndump\n",
		"undoboth": "win\ntype 1 hello\ntag 1 Zerox\nb2 1.tag 23 28\n" +
			"type 1 XYZ\nundo 1\ndump\n",
	})
}

func genDclick() {
	cases := []struct {
		body  string
		click int
	}{
		{"foo bar", 1},
		{"foo bar", 5},
		{"(bar)", 1},
		{"(bar)", 4},
		{"a [x] b", 3},
		{"'quote'", 1},
		{"foo123 x", 2},
		{"(foo", 1},
		{"x\\ny", 1},
		{"(a(b)c)", 1},
		{"abc", 0},
		{"abc", 3},
	}
	var items []struct{ hint, script string }
	for _, c := range cases {
		s := "win\nbody 1 " + c.body + "\ndclick 1 " + itoa(c.click) + "\nprint 1\n"
		items = append(items, struct{ hint, script string }{c.body + itoa(c.click), s})
	}
	emitSlice("dclick", items)
}

func genBsw() {
	emitMap("bsw", map[string]string{
		"bs":            "win\ntype 1 hello\nkey 1 bs\nprint 1\n",
		"wordback":      "win\ntype 1 foo bar\nkey 1 ^w\nprint 1\n",
		"wordbackspace": "win\ntype 1 foo bar \nkey 1 ^w\nprint 1\n",
		"lineback":      "win\ntype 1 abc\\ndefg\nkey 1 ^u\nprint 1\n",
		"wordatbol":     "win\ntype 1 abc\\n\nkey 1 ^w\nprint 1\n",
		"wordmulti":     "win\ntype 1 one two three\nkey 1 ^w ^w ^w\nprint 1\n",
		"linemulti":     "win\ntype 1 aa\\nbb\\ncc\nkey 1 ^u ^u\nprint 1\n",
		"upmove":        "win\ntype 1 line1\\nline2\\nline3\\nline4\nkey 1 up up\ntype 1 X\nprint 1\n",
		"wordmid":       "win\ntype 1 hello world\nb1 1 8 8\nkey 1 ^w\nprint 1\n",
		// ^A/^E move to beginning/end of line.
		"bol":       "win\ntype 1 hello\nkey 1 ^a\nprint 1\n",
		"bolindent": "win\ntype 1 x\\n  hi\nkey 1 ^a\nprint 1\n",
		"eol":       "win\ntype 1 hi\\nlo\nb1 1 0 0\nkey 1 ^e\nprint 1\n",
		// %-keys: copy / cut / paste / undo / redo.
		"cmdcut":   "win\ntype 1 hello\nb1 1 0 3\nkey 1 %x\nprint 1\n",
		"cmdpaste": "win\ntype 1 hello\nb1 1 0 3\nkey 1 %x %v\nprint 1\n",
		"cmdcopy":  "win\ntype 1 hello\nb1 1 0 3\nkey 1 %c end %v\nprint 1\n",
		"cmdundo":  "win\ntype 1 hello\nb1 1 0 5\nkey 1 %x %z\nprint 1\n",
		"cmdredo":  "win\ntype 1 hello\nb1 1 0 5\nkey 1 %x %z %Z\nprint 1\n",
		// typing over a selection cuts (snarfs) it, so a later paste restores it.
		"typeoversel": "win\ntype 1 hello\nb1 1 0 3\ntype 1 Z\nkey 1 %v\nprint 1\n",
	})
}

func genBuff() {
	files := map[string]string{
		"ascii":     "hello world\n",
		"utf8":      "héllo — wörld αβγ\n",
		"nuls":      "a\x00b\x00\x00c",
		"empty":     "",
		"multiline": "line1\nline2\nline3\n",
		"binary":    "\x00\x01\x02ABC\xffZ",
	}
	keys := make([]string, 0, len(files))
	for k := range files {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	for _, k := range keys {
		emit("buff_"+k, "win\nload 1 $FILE\nprint 1\n", map[string]string{"file": files[k]})
	}
}

func genWind() {
	big := rep("x", 150)
	emitMap("wind", map[string]string{
		"scratchdel":    "win\nname 1 /tmp/+Errors\nb2 1.tag 13 16\ndump\n",
		"nameddirty":    "win\nname 1 /tmp/f\ntype 1 hi\nb2 1.tag 7 10\ndump\n",
		"smalldirty":    "win\ntype 1 hi\nb2 1.tag 1 4\ndump\n",
		"bigdirty":      "win\ntype 1 " + big + "\nb2 1.tag 1 4\ndump\n",
		"closeptrs":     "win\ntype 1 hi\nb1 1 0 2\ntag 1 Delete\nb2 1.tag 23 29\ndump\n",
		"scratchrename": "win\nname 1 /tmp/+Errors\nname 1 /tmp/+Errors\nname 1 /tmp/plain\nb2 1.tag 11 14\ndump\n",
	})
}

// genCover adds scenarios that reach the last few -D-reachable code paths the
// main scenario set misses: colcmp's length branches (Sort with prefix-related
// names of differing lengths), the Indent OFF/short/2-1-chord-arg paths, the Tab
// numeric/non-digit paths, the \t text escape, and rowclose deleting the last vs
// a non-last column (Delcol).
func genCover() {
	emitMap("cover", map[string]string{
		// Sort with names where one is a prefix of another and lengths differ:
		// covers colcmp's "len(r2)<n" and "return len(r1)-len(r2)" branches.
		"sortlen": "win\nname 1 /x\nwin\nname 2 /xy\nbody 1 Sort\nb2 1 0 4\ndump\n",
		// Delcol on a column holding a large dirty window: colclean fails, the
		// column is kept (delcol's early return).
		"delcoldirty": "win\ntype 1 " + rep("x", 150) + "\ntag 1 Delcol\nb2 1.tag 23 29\ndump\n",
		// Delcol on the only (clean) column: rowclose paints the whole row and
		// returns.
		"delcollast": "win\nbody 1 Delcol\nb2 1 0 6\ndump\n",
		// Delcol on a non-last column with another column present: rowclose
		// extends the next column left.
		"delcolfirst": "win\nbody 1 Delcol\nnewcol\nwin\nbody 2 x\nb2 1 0 6\ndump\n",
		// Indent with a one-character argument: indentval returns iError.
		"indentshort": "win\nbody 1 Indent x\nb2 1 0 8\nprint 1\n",
		// Indent OFF: the global-off branch of indentval.
		"indentglobaloff": "win\nbody 1 Indent OFF\nb2 1 0 10\nprint 1\n",
		// Indent with the argument supplied by a 2-1 chord (argt): the getarg path.
		"indentarg": "win\nbody 1 Indent\nwin\nbody 2 on\nb2arg 1 0 6 2 0 2\nprint 1\n",
		// Tab with a numeric argument supplied by a 2-1 chord (argt path).
		"tabarg": "win\nbody 1 Tab\nwin\nbody 2 4\nb2arg 1 0 3 2 0 1\nprint 1\n",
		// Tab with a trailing non-digit: runesAtoi stops at the non-digit.
		"tabnondigit": "win\nbody 1 Tab 8x\nb2 1 0 6\nprint 1\n",
		// a \t escape in inserted text: the driver's torunes tab branch.
		"bodytab": "win\nbody 1 a\\tb\nprint 1\n",
	})
}

// gen9p drives the 9P read/write verbs (xfid.go) for the addr/data/body/tag
// files: address evaluation, addressed read/write through data, body/tag append
// (with backspace handling), UTF-8 round-trips, and error paths.
func gen9p() {
	emitMap("9p", map[string]string{
		// set an addressed range, read it back, replace it via data, re-read.
		"addrdata": "win\nbody 1 hello world\nwrite 1/addr \"#0,#5\"\nread 1/addr\n" +
			"write 1/data \"HELLO\"\nread 1/body\nread 1/addr\n",
		// data read returns from addr to end of body and advances addr.
		"dataread": "win\nbody 1 hello world\nwrite 1/addr \"#6,#6\"\nread 1/data\nread 1/addr\n",
		// a line address (no #) selects whole lines.
		"addrline": "win\nbody 1 a\\nb\\nc\\n\nwrite 1/addr \"2\"\nread 1/addr\nread 1/data\n",
		// append to the body via a write at end-of-file.
		"bodyappend": "win\nbody 1 hello\nwrite 1/body \" world\"\nread 1/body\n",
		// append to the tag.
		"tagappend": "win\nname 1 /a/b\nwrite 1/tag \" More\"\nread 1/tag\n",
		// body write honors embedded backspaces (textbsinsert); \x08 is a BS byte.
		"bodybs": "win\nbody 1 hello\nwrite 1/body \"XYZ\\x08\\x08Q\"\nread 1/body\n",
		// UTF-8 round-trip: read back shows multibyte runes as \xHH.
		"utf": "win\nwrite 1/body \"héllo\"\nread 1/body\n",
		// read the (empty) body and the default tag of a fresh window.
		"empty": "win\nread 1/body\nread 1/tag\n",
		// $ address (end of file) then append via data.
		"dollar": "win\nbody 1 abc\nwrite 1/addr \"$\"\nwrite 1/data \"DEF\"\nread 1/body\n",
		// a line address out of range fails to evaluate.
		"baddr": "win\nbody 1 hello\nwrite 1/addr \"99\"\nread 1/addr\n",
		// a malformed address is a syntax error.
		"badsyntax": "win\nbody 1 hello\nwrite 1/addr \"#x\"\nread 1/addr\n",
		// delete a range: address it, then write empty data.
		"datadelete": "win\nbody 1 hello world\nwrite 1/addr \"#0,#6\"\nwrite 1/data \"\"\nread 1/body\n",
		// regexp address via 9P, then read the matched data.
		"addrre": "win\nbody 1 foo bar baz\nwrite 1/addr \"/bar/\"\nread 1/addr\nread 1/data\n",
		// quoting round-trip: a body with quote, backslash, tab and CR shows the
		// matching escapes on read (printquoted / dquote, dunquote).
		"quoteout": "win\nwrite 1/body \"a\\\"b\\\\c\\td\\ne\\rf\"\nread 1/body\n",
		// hex escapes (mixed case) inject raw bytes; read shows them as \\xHH.
		"hex": "win\nwrite 1/body \"\\xc3\\xA9z\"\nread 1/body\n",
		// a body write with leading backspaces deletes back past the start
		// (textbsinsert's initial-overflow clamp).
		"bsdel": "win\nbody 1 h\nwrite 1/body \"\\x08\\x08\\x08X\"\nread 1/body\n",
		// addr left pointing past a body that was then emptied: addr read clamps.
		"clamp": "win\nbody 1 hello\nwrite 1/addr \"$\"\nb1 1 0 5\nkey 1 bs\nread 1/addr\n",
		// same, but data read/write past end of body reports an error.
		"dataoob": "win\nbody 1 hello\nwrite 1/addr \"$\"\nb1 1 0 5\nkey 1 bs\n" +
			"read 1/data\nwrite 1/data \"X\"\nread 1/body\n",
		// the top-level index file: one line per window (ctl prefix + tag).
		"index": "win\nname 1 /a/one\nbody 1 hi\nnewcol\nwin\nname 2 /a/two\nbody 2 yo\nread index\n",
		// index with a zeroxed window: only the active copy is listed.
		"indexzerox": "win\ntype 1 hi\ntag 1 Zerox\nb2 1.tag 23 28\nread index\n",
		// index truncates a tag at its first newline.
		"indextag": "win\nwrite 1/tag \"\\nmore\"\nread index\n",
		// xdata reads exactly the addressed range [q0,q1).
		"xdata": "win\nbody 1 hello world\nwrite 1/addr \"#3,#8\"\nread 1/xdata\n",
		// the ctl file: id, tag/body lengths, dir/dirty, width, font, tab, undo
		// flags. Differentiable because both acmes load the same -f/$font font.
		"ctlread": "win\nname 1 /a/b\ntype 1 hi\nread 1/ctl\n",

		// --- ctl writes ---
		// clean clears the modified flag (the tag's Put disappears).
		"ctlclean": "win\nname 1 /a/b\ntype 1 hi\nwrite 1/ctl \"clean\"\nread 1/tag\n",
		// dirty sets it (Put appears).
		"ctldirty": "win\nname 1 /a/b\nwrite 1/ctl \"dirty\"\nread 1/tag\n",
		// name renames the window (the tag's name field changes).
		"ctlname": "win\nwrite 1/ctl \"name /x/y\\n\"\nread 1/tag\n",
		// nomenu/menu toggle the file menu in the tag.
		"ctlnomenu": "win\nname 1 /a/b\nwrite 1/ctl \"nomenu\"\nread 1/tag\n",
		"ctlmenu":   "win\nname 1 /a/b\nwrite 1/ctl \"nomenu\"\nwrite 1/ctl \"menu\"\nread 1/tag\n",
		// cleartag wipes the tag right of the bar.
		"ctlcleartag": "win\nname 1 /a/b\ntag 1  extra stuff\nwrite 1/ctl \"cleartag\"\nread 1/tag\n",
		// dot=addr moves dot to the 9P address.
		"ctldotaddr": "win\nbody 1 hello\nwrite 1/addr \"#1,#3\"\nwrite 1/ctl \"dot=addr\"\nprint 1\n",
		// addr=dot copies dot into the 9P address.
		"ctladdrdot": "win\nbody 1 hello\nb1 1 1 4\nwrite 1/ctl \"addr=dot\"\nread 1/addr\n",
		// limit=addr sets the search limit (verified by the write count).
		"ctllimit": "win\nbody 1 hello\nwrite 1/addr \"#0,#5\"\nwrite 1/ctl \"limit=addr\"\nread 1/tag\n",
		// mark adds an undo point: a later edit undoes back to the mark.
		"ctlmark": "win\ntype 1 hi\nwrite 1/ctl \"mark\"\ntype 1 yo\nundo 1\nprint 1\n",
		// nomark suppresses marking: the data write is not separately undoable.
		"ctlnomark": "win\nbody 1 abc\nwrite 1/ctl \"nomark\"\nwrite 1/addr \"#0,#0\"\nwrite 1/data \"X\"\nundo 1\nprint 1\n",
		// del deletes a clean window; delete deletes unconditionally.
		"ctldel":    "win\nbody 1 x\nwrite 1/ctl \"del\"\ndump\n",
		"ctldelete": "win\ntype 1 hi\nwrite 1/ctl \"delete\"\ndump\n",
		// dump/dumpdir set the dump strings (verified by the write count).
		"ctldump": "win\nwrite 1/ctl \"dump echo hi\\n\"\nread 1/tag\n",
		// lock/unlock and show have no visible effect (verified by write count).
		"ctllock": "win\nwrite 1/ctl \"lock\"\nwrite 1/ctl \"unlock\"\nread 1/tag\n",
		"ctlshow": "win\nbody 1 hi\nwrite 1/ctl \"show\"\nread 1/tag\n",
		// an unknown ctl command is an error.
		"ctlbogus": "win\nwrite 1/ctl \"zzz\"\nread 1/tag\n",
		// name/dump/dumpdir need a newline-terminated argument.
		"ctlnamenonl":    "win\nwrite 1/ctl \"name x\"\nread 1/tag\n",
		"ctlnamebad":     "win\nwrite 1/ctl \"name a\\x01b\\n\"\nread 1/tag\n",
		"ctldumpnonl":    "win\nwrite 1/ctl \"dump x\"\nread 1/tag\n",
		"ctldumpdir":     "win\nwrite 1/ctl \"dumpdir /tmp\\n\"\nread 1/tag\n",
		"ctldumpdirnonl": "win\nwrite 1/ctl \"dumpdir x\"\nread 1/tag\n",
		// del refuses a dirty (named, modified) window.
		"ctldeldirty": "win\nname 1 /a/b\ntype 1 hi\nwrite 1/ctl \"del\"\ndump\n",
		// several commands in one message (the inter-command newline skip).
		"ctlmulti": "win\ntype 1 hi\nwrite 1/ctl \"nomark\\nmark\"\nread 1/tag\n",
		// cleartag with the tag selection past the bar (the q0/q1 clamp).
		"ctlcleartagsel": "win\nname 1 /a/b\ntag 1  extra\nb1 1.tag 99 99\nwrite 1/ctl \"cleartag\"\nread 1/tag\n",
		// get/put with no file name: a no-op (warns to the unflushed +Errors).
		"getnoname": "win\nwrite 1/ctl \"get\"\nread 1/body\n",
		"putnoname": "win\ntype 1 hi\nwrite 1/ctl \"put\"\nread 1/tag\n",
		// the top-level label file: write is accepted (no-op), read is empty.
		"label": "win\nwrite label \"my title\"\nread label\n",
		// event: with the event file open, a button-2 execute is reported (not
		// run). Owner 'D' (the driver's winlock char) prefixes each record.
		"event": "win\ntype 1 hello\nopen 1/event\nb2 1 0 5\nread 1/event\n",
		// a builtin name sets the e-flag (1); it is still only reported.
		"eventbuiltin": "win\nbody 1 Undo\nopen 1/event\nb2 1 0 4\nread 1/event\n",
		// a tag execute reports 'x' (lower case).
		"eventtag": "win\nname 1 /a/b\nopen 1/event\nb2 1.tag 1 4\nread 1/event\n",
		// an expanded (point) execute reports two records (the expand flag).
		"eventexpand": "win\nbody 1 word\nopen 1/event\nb2 1 2 2\nread 1/event\n",
		// a 9P body write with the event file open reports an insert ('E' owner).
		"eventinsert": "win\nopen 1/event\nwrite 1/body \"hi\"\nread 1/event\n",
		// a 9P data delete reports a delete record.
		"eventdelete": "win\nbody 1 hello\nopen 1/event\nwrite 1/addr \"#0,#2\"\nwrite 1/data \"\"\nread 1/event\n",
		// a 2-1 chord execute with the event file open reports the argument too.
		"eventarg": "win\nbody 1 Edit\nwin\nbody 2 ,p\nopen 1/event\nb2arg 1 0 4 2 0 2\nread 1/event\n",
		// a selection larger than EVENTSIZE reports the range with no text.
		"eventbig": "win\nbody 1 " + rep("z", 300) + "\nopen 1/event\nb2 1 0 300\nread 1/event\n",
		// the global activity log records window operations (zerox here).
		"logzerox": "win\ntype 1 x\nopen log\ntag 1 Zerox\nb2 1.tag 23 28\nread log\n",
		// walking "new" creates a window; reading its ctl returns the new id.
		"new": "newcol\nread new/ctl\ndump\n",
		// the new window can then be written and read like any other.
		"newwrite": "newcol\nread new/ctl\nwrite 1/body \"hi\"\nread 1/body\n",
		// rdsel reads the body's current selection.
		"rdsel": "win\nbody 1 hello world\nb1 1 0 5\nread 1/rdsel\n",
		// wrsel replaces the selection: opening it cuts, the write fills.
		"wrsel": "win\nbody 1 hello world\nb1 1 0 5\nwrite 1/wrsel \"HI\"\nread 1/body\n",
		// writing cons creates the shared +Errors window (with no column yet, it
		// also makes the column).
		"cons": "write cons \"an error\\n\"\ndump\n",
		// a second cons write appends to the same +Errors window.
		"cons2": "newcol\nwrite cons \"one\\n\"\nwrite cons \"two\\n\"\ndump\n",
		// an empty body write is a no-op.
		"bodyempty": "win\nbody 1 x\nwrite 1/body \"\"\nread 1/body\n",
		// button-3 (point click) expands to the word and searches forward.
		"look": "win\nbody 1 foo bar foo baz\nb3 1 1 1\nprint 1\n",
		// search wraps around to the first occurrence.
		"lookwrap": "win\nbody 1 foo bar foo\nb3 1 9 9\nprint 1\n",
		// button-3 on a point inside the current selection uses the selection.
		"looksel": "win\nbody 1 abc abc\nb1 1 0 3\nb3 1 1 1\nprint 1\n",
		// button-3 with the event file open reports the look (l/L) events.
		"lookevent": "win\nbody 1 foo bar foo\nopen 1/event\nb3 1 1 1\nread 1/event\n",
		// a relative name in a named window resolves against its directory.
		"lookdir": "win\nname 1 /a/b\nbody 1 c\nb3 1 0 0\nprint 1\n",
		// a colon not followed by an address terminates the file name.
		"lookcolon": "win\nbody 1 abcd:zz\nb3 1 0 0\nprint 1\n",
		// clicking inside the address (past the colon) finds the colon scanning back.
		"lookcolonback": "win\nbody 1 foo:23\nb3 1 5 5\nprint 1\n",
		// a file:addr followed by text bounds the address scan at the space.
		"lookcolonaddr": "win\nbody 1 foo:23 rest\nb3 1 0 0\nprint 1\n",
		// reverse button-3 searches backward to the previous occurrence.
		"lookrev": "win\nbody 1 foo bar foo\nb3r 1 9 9\nprint 1\n",
		// reverse look with the event file open reports an 'R'-cased event.
		"lookrevevent": "win\nbody 1 foo bar foo\nopen 1/event\nb3r 1 9 9\nread 1/event\n",
		// a URL is recognized as a name (reported via the event file, no open).
		"lookurl": "win\nbody 1 http://example.com\nopen 1/event\nb3 1 0 0\nread 1/event\n",
		// an <include> name resolves against the standard include directories.
		"lookinc": "win\nbody 1 <u.h>\nopen 1/event\nb3 1 2 2\nread 1/event\n",
		// a long (7+ char) non-URL word exercises the URL-prefix comparison.
		"looklong": "win\nbody 1 abcdefgh ij abcdefgh\nb3 1 1 1\nprint 1\n",
		// a URL with a trailing period drops the period from the name.
		"lookurldot": "win\nbody 1 http://x.\nopen 1/event\nb3 1 0 0\nread 1/event\n",
		// reverse search from the first occurrence wraps to the last.
		"lookrevwrap": "win\nbody 1 foo bar foo\nb3r 1 1 1\nprint 1\n",
		// a reverse look on a non-colon selection clears the reverse-address flag.
		"lookrevsel": "win\nbody 1 foo foo\nb1 1 0 3\nb3r 1 1 1\nprint 1\n",
		// a selection whose colon is not an address is not a file name.
		"lookselcolon": "win\nbody 1 ab:xy\nb1 1 0 5\nb3 1 1 1\nprint 1\n",
		// a selection with a non-file character is not a file name.
		"lookselbad": "win\nbody 1 a!b\nb1 1 0 3\nb3 1 1 1\nprint 1\n",
		// a look inside a tag selection does not jump the window's dot.
		"looktagsel": "win\nbody 1 Del x\nb1 1.tag 1 4\nb3 1.tag 2 2\nprint 1\n",
		// a bare :addr re-addresses the current window (empty file name).
		"lookaddronly": "win\nbody 1 aa\\nbb\\ncc\\n:2\nb3 1 9 9\nprint 1\n",
		// an <include> that resolves to nothing is left as written.
		"lookincnf": "win\nbody 1 <nyetsuch.h>\nb3 1 2 2\nprint 1\n",
		// an absolute <include> path is not searched.
		"lookincabs": "win\nbody 1 </x.h>\nb3 1 2 2\nprint 1\n",
		// button-3 on whitespace (between words) expands to nothing (no-op).
		"lookspace": "win\nbody 1 a  b\nb3 1 2 2\nprint 1\n",
		// button-3 on an exact range with the event open sends only one message.
		"lookrangeevent": "win\nbody 1 foo bar foo\nopen 1/event\nb3 1 0 3\nread 1/event\n",
	})

	// get/put need a file on disk; the harness writes the "file" member as "f"
	// in a per-run working directory and substitutes $FILE -> "f".
	emit("9p_get", "win\nname 1 $FILE\nwrite 1/ctl \"get\"\nread 1/body\nread 1/tag\n",
		map[string]string{"file": "disk content\n"})
	// put writes the body to disk; a later get reads it back (proving the write).
	// get-first stamps the file so put's modified-since check passes.
	emit("9p_put", "win\nname 1 $FILE\nwrite 1/ctl \"get\"\ntype 1 saved\n"+
		"write 1/ctl \"put\"\nwrite 1/ctl \"get\"\nread 1/body\nread 1/tag\n",
		map[string]string{"file": ""})
	// get refuses to overwrite an unsaved (dirty) body.
	emit("9p_getdirty", "win\nname 1 $FILE\nwrite 1/ctl \"get\"\ntype 1 X\n"+
		"write 1/ctl \"get\"\nread 1/body\n",
		map[string]string{"file": "orig\n"})
	// button-3 on a file name opens it in a new window (id 2); read its body
	// (the name would leak the absolute temp path, so don't dump it).
	emit("9p_lookopen", "win\nbody 1 f\nb3 1 0 0\nread 2/body\n",
		map[string]string{"file": "opened from disk\n"})
	// button-3 on file:addr opens the file at the address; expose the resulting
	// dot via addr=dot (avoiding the path-bearing name in a dump).
	emit("9p_lookaddr", "win\nbody 1 f:2\nb3 1 0 0\nwrite 2/ctl \"addr=dot\"\nread 2/addr\n",
		map[string]string{"file": "line one\nline two\nline three\n"})
	// button-3 on a file name with the event file open reports a file look event
	// (the file flags) instead of opening it.
	emit("9p_lookfileevent", "win\nbody 1 f\nopen 1/event\nb3 1 0 0\nread 1/event\n",
		map[string]string{"file": "x\n"})
	// the same, with an address, reports the name:addr in the second message.
	emit("9p_lookaddrevent", "win\nbody 1 f:2\nopen 1/event\nb3 1 0 0\nread 1/event\n",
		map[string]string{"file": "one\ntwo\nthree\n"})
	// the activity log records get and put operations (with the file name).
	emit("9p_logget", "win\nname 1 $FILE\nopen log\nwrite 1/ctl \"get\"\nread log\n",
		map[string]string{"file": "x\n"})
	emit("9p_logput", "win\nname 1 $FILE\nwrite 1/ctl \"get\"\nopen log\n"+
		"type 1 z\nwrite 1/ctl \"put\"\nread log\n",
		map[string]string{"file": "x\n"})
	// put to an existing, never-read file refuses (file already exists): the
	// window stays dirty (tag keeps Put). Covers the modified-since/sha1 path.
	emit("9p_putexists", "win\nname 1 $FILE\ntype 1 new\nwrite 1/ctl \"put\"\nread 1/tag\n",
		map[string]string{"file": "old\n"})
	// get of a missing file is a no-op (can't open): the body stays empty.
	emit("9p_getnofile", "win\nname 1 g\nwrite 1/ctl \"get\"\nread 1/body\n",
		map[string]string{"file": "ignored\n"})
	// get elides NUL bytes from the loaded file.
	emit("9p_getnul", "win\nname 1 $FILE\nwrite 1/ctl \"get\"\nread 1/body\n",
		map[string]string{"file": "a\x00b\x00c\n"})
	// get of a file taller than the window: the frame fills and stops
	// (lastlinefull), but the whole file is in the buffer.
	emit("9p_getbig", "win\nname 1 $FILE\nwrite 1/ctl \"get\"\nread 1/ctl\n",
		map[string]string{"file": rep("line\n", 200)})
	// get reloading a zeroxed file resizes the other text on the shared file.
	// Zerox an empty (clean) window first so the later get is not refused.
	emit("9p_getzerox", "win\ntag 1 Zerox\nb2 1.tag 18 23\nname 1 $FILE\n"+
		"write 1/ctl \"get\"\nread 1/body\n",
		map[string]string{"file": "shared\n"})
	// get restores the selection (line,rune) across a reload (nlcount path).
	emit("9p_getsel", "win\nname 1 $FILE\nwrite 1/ctl \"get\"\nb1 1 3 6\n"+
		"write 1/ctl \"get\"\nprint 1\n",
		map[string]string{"file": "l1\nl2\nl3\n"})

	// writing an event record re-dispatches it through look3/execute, exactly as
	// if the user had acted on it (xfideventwrite). Each record is owner byte,
	// command byte, q0, q1, newline.
	// 'L' looks in the body: expand "foo" and search forward; expose the new dot.
	// (a trailing space before the newline exercises the address terminator.)
	emit("9p_eventwrite", "win\nbody 1 foo bar foo\nwrite 1/event \"ML0 3 \\n\"\n"+
		"write 1/ctl \"addr=dot\"\nread 1/addr\n", nil)
	// 'R' looks in reverse from the last occurrence.
	emit("9p_eventwriterev", "win\nbody 1 foo bar foo\nwrite 1/event \"MR8 11\\n\"\n"+
		"write 1/ctl \"addr=dot\"\nread 1/addr\n", nil)
	// a lowercase command letter directs the look at the tag instead of the body.
	emit("9p_eventwritetag", "win\nwrite 1/event \"Ml0 0\\n\"\n", nil)
	// 'X' executes the [q0,q1) text as a command (here the Newcol built-in).
	emit("9p_eventwritex", "win\nbody 1 Newcol\nwrite 1/event \"MX0 6\\n\"\ndump\n", nil)
	// malformed event records are rejected with "bad event syntax":
	emit("9p_eventshort", "win\nbody 1 ab\nwrite 1/event \"M\"\n", nil)           // truncated
	emit("9p_eventbadq0", "win\nbody 1 ab\nwrite 1/event \"ML\\n\"\n", nil)       // no q0
	emit("9p_eventbadq1", "win\nbody 1 ab\nwrite 1/event \"ML0\\n\"\n", nil)      // no q1
	emit("9p_eventbadnl", "win\nbody 1 ab\nwrite 1/event \"ML0 1\"\n", nil)       // no newline
	emit("9p_eventbadc", "win\nbody 1 ab\nwrite 1/event \"M!0 1\\n\"\n", nil)     // bad command
	emit("9p_eventbadrange", "win\nbody 1 ab\nwrite 1/event \"ML5 5\\n\"\n", nil) // addr past end
	// writing the errors file appends to the +Errors window for the source
	// window's directory (window 2; here /a/b/+Errors).
	emit("9p_errorswrite", "win\nname 1 /a/b/x\nwrite 1/errors \"oops\\n\"\nread 2/body\n", nil)
	// a relative name whose directory cleans to "." uses the plain +Errors window.
	emit("9p_errorswritedot", "win\nname 1 ./x\nwrite 1/errors \"hi\\n\"\nread 2/body\n", nil)
	// the editout files are writable only during an edit pipe; -D never starts
	// one, so the write is denied (edittext returns "permission denied").
	emit("9p_editout", "win\nwrite 1/editout \"x\"\n", nil)
	emit("9p_editoutglobal", "write editout \"x\"\n", nil)

	// --- button-2 builtins (exec.c/look.c) ---
	// New with no argument adds an empty window to the column.
	emit("9p_newcmd", "win\nbody 1 New\nb2 1 0 3\ndump\n", nil)
	// New with a file name opens that file in a new window (id 2).
	emit("9p_newfile", "win\nbody 1 New f\nb2 1 0 5\nread 2/body\n",
		map[string]string{"file": "from disk\n"})
	// Look with an argument searches the body for it.
	emit("9p_lookcmd", "win\nbody 1 Look ab ab\nb2 1 0 7\nprint 1\n", nil)
	// Look with no argument searches for the current body selection (the default
	// tag's built-in Look menu entry sits at runes 13..17).
	emit("9p_looknoarg", "win\nbody 1 foo foo\nb1 1 0 3\nb2 1.tag 13 17\nprint 1\n", nil)
	// Put writes the executing window's body to its (new) file; the log records it.
	emit("9p_putcmd", "win\nname 1 newfile\nbody 1 Put\nopen log\nb2 1 0 3\nread log\n", nil)
	// Put with a relative name argument writes to that name in the window's dir.
	emit("9p_putcmdarg", "win\nbody 1 Put other\nopen log\nb2 1 0 9\nread log\n", nil)
	// Put on an unnamed window with no name argument has nothing to write to.
	emit("9p_putcmdnoname", "win\nbody 1 Put\nb2 1 0 3\n", nil)
	// Put with a name argument in a window whose directory cleans to ".".
	emit("9p_putdotdir", "win\nname 1 ./x\nbody 1 Put sub\nopen log\nb2 1 0 7\nread log\n", nil)
	// Put with the name supplied as a 2-1 chord argument (getname's isput path).
	emit("9p_putargt", "win\nbody 1 Put\nnewcol\nwin\nbody 2 myname\n"+
		"open log\nb2arg 1 0 3 2 0 6\nread log\n", nil)
	// the same, but the chord argument contains a slash (no directory is added).
	emit("9p_putargtslash", "win\nbody 1 Put\nnewcol\nwin\nbody 2 ./sub\n"+
		"open log\nb2arg 1 0 3 2 0 5\nread log\n", nil)
	// Get reloads the executing window's file (here the body already equals it).
	emit("9p_getcmd", "win\nname 1 $FILE\nwrite 1/ctl \"get\"\nopen log\nb2 1 0 3\nread log\n",
		map[string]string{"file": "Get\n"})
	// Putall writes every dirty, named, existing window to its file.
	emit("9p_putall", "win\nname 1 $FILE\nwrite 1/ctl \"get\"\ntype 1 X\nnewcol\n"+
		"win\nbody 2 Putall\nopen log\nb2 2 0 6\nread log\n",
		map[string]string{"file": "orig\n"})
}

func genGeometry() {
	stack4 := stackBody(4)
	emitMap("geom", map[string]string{
		"grow1":     stack4 + "colgrow 2 1\nrects\n",
		"grow2":     stack4 + "colgrow 2 2\nrects\n",
		"grow3":     stack4 + "colgrow 2 3\nrects\n",
		"growneg1":  stack4 + "colgrow 2 -1\nrects\n",
		"growlast":  stack4 + "colgrow 4 -1\nrects\n",
		"growfirst": stack4 + "colgrow 1 1\nrects\n",
		"growseq":   stack4 + "colgrow 2 1\ncolgrow 3 2\ncolgrow 1 3\nrects\n",
		"addmid":    stack4 + "coladd 0 300\nrects\n",
		"closemid":  stack4 + "colclose 2\nrects\n",
		"closelast": stack4 + "colclose 4\nrects\n",
		"closeall":  stack4 + "colclose 1\ncolclose 2\ncolclose 3\nrects\n",
		"colshrink": stack4 + "colresize 0 0 0 600 768\nrects\n",
		"rowgrow":   "win\nbody 1 x\nnewcol\nwin\nbody 2 y\nrowresize 0 0 900 768\nrects\n",
		"rowshrink": "win\nbody 1 x\nnewcol\nwin\nbody 2 y\nrowresize 0 0 700 768\nrects\n",
		"rowaddmid": "win\nbody 1 x\nnewcol\nwin\nbody 2 y\nrowadd 500\nrects\n",
		"stack8":    stackBody(8) + "rects\n",
		"stack12":   stackBody(12) + "dump\n",
		// the scrollbar thumb reflects the visible range; scrolling a tall body
		// down moves the thumb (textscrdraw/scrpos).
		"scroll": "win\nbody 1 " + rep("x\\n", 100) + "\nrects\nkey 1 pgdn\nrects\n",
		// Kup/Kdown in a multi-line tag shrink/expand it (the tag rect changes
		// height while the window rect stays the same).
		"tagshrink": "win\ntag 1 " + rep("x", 200) + "\nrects\nkey 1.tag up\nrects\n",
		"tagexpand": "win\ntag 1 " + rep("x", 200) + "\nkey 1.tag up\nrects\nkey 1.tag down\nrects\n",
	})
}
