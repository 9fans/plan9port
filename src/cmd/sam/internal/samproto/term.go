// Package samproto implements the sam wire protocol, for use in tests.
package samproto

import (
	"encoding/binary"
	"fmt"
	"io"
	"strconv"
	"strings"
	"time"
)

// Version is the protocol version the terminal claims (mesg.h VERSION).
const Version = 2

// tfile is the terminal's model of one host file.
type tfile struct {
	length int
}

// Term is a model samterm driving a host over a connection. It auto-responds
// to flow-control (Hack->Tack) and fetches grown text (Hgrow->Trequest), and
// records every Hmesg received for comparison.
type Term struct {
	c      *conn
	ch     chan Hmsg
	closed chan struct{}
	idle   time.Duration

	raw        []byte   // concatenated raw Hmesg bytes, in order
	transcript []string // human-readable rendering

	cmdtag  int
	files   map[int]*tfile
	autoReq bool // auto-fetch grown text with Trequest
	expect  bool // expectation mode: pump() is a no-op; Expect drives reading
}

// New starts a terminal on the given streams (r = host stdout, w = host stdin).
func New(r io.Reader, w io.Writer) *Term {
	t := &Term{
		c:       newConn(r, w),
		ch:      make(chan Hmsg, 64),
		closed:  make(chan struct{}),
		idle:    150 * time.Millisecond,
		cmdtag:  -1,
		files:   map[int]*tfile{},
		autoReq: true,
	}
	go t.reader()
	return t
}

func (t *Term) reader() {
	for {
		m, err := t.c.readH()
		if err != nil {
			close(t.closed)
			return
		}
		t.ch <- m
	}
}

func (t *Term) file(tag int) *tfile {
	f := t.files[tag]
	if f == nil {
		f = &tfile{}
		t.files[tag] = f
	}
	return f
}

// pump reads and handles messages until the host goes idle (no message for
// t.idle) or the connection closes.
func (t *Term) pump() {
	if t.expect {
		// In expectation mode the script names each message, so Expect drives
		// reading; there is no idle wait.
		return
	}
	timer := time.NewTimer(t.idle)
	defer timer.Stop()
	for {
		if !timer.Stop() {
			select {
			case <-timer.C:
			default:
			}
		}
		timer.Reset(t.idle)
		select {
		case m, ok := <-t.ch:
			if !ok {
				return
			}
			t.handle(m)
		case <-t.closed:
			// drain anything buffered
			for {
				select {
				case m := <-t.ch:
					t.handle(m)
				default:
					return
				}
			}
		case <-timer.C:
			return
		}
	}
}

func (t *Term) handle(m Hmsg) {
	t.raw = append(t.raw, m.Raw...)
	t.transcript = append(t.transcript, render(m))
	switch m.Typ {
	case Hversion:
		t.c.send(msg(Tversion).short(Version))
	case Hack:
		t.c.send(msg(Tack))
	case Hbindname:
		if t.cmdtag < 0 {
			t.cmdtag = short(m.Data, 0)
		}
	case Hgrow:
		tag := short(m.Data, 0)
		p := long(m.Data, 2)
		n := long(m.Data, 6)
		t.file(tag).length += n
		// Fetch the grown text in TBLOCKSIZE chunks, as real samterm does;
		// the host replies into a fixed buffer, so a single large request
		// would overflow it.
		if t.autoReq {
			for off := 0; off < n; off += 512 {
				m := n - off
				if m > 512 {
					m = 512
				}
				t.c.send(msg(Trequest).short(tag).long(p + off).short(m))
			}
		}
	case Hgrowdata:
		tag := short(m.Data, 0)
		n := long(m.Data, 6)
		t.file(tag).length += n
	case Hcut:
		tag := short(m.Data, 0)
		n := long(m.Data, 6)
		t.file(tag).length -= n
	}
}

// Start performs the version handshake and opens the command file.
func (t *Term) Start() {
	t.pump() // read Hversion (+ any startup file msgs); handle() sends Tversion
	t.c.send(msg(Tstartcmdfile).vlong(0x1000))
	t.pump()
}

// Type types text into the command window (at its current end) and runs the
// host until it goes idle. A trailing newline triggers command execution.
// Ttype is not echoed (the terminal already has the chars), so advance our own
// length model by the typed runes; any command output the host appends is
// echoed separately and counted by handle().
func (t *Term) Type(text string) {
	pos := t.file(t.cmdtag).length
	t.c.send(msg(Ttype).short(t.cmdtag).long(pos).str(text))
	t.file(t.cmdtag).length += len([]rune(text))
	t.pump()
}

// TypeAt types text into file tag at position p (Ttype). Like SendText, the
// chars are not echoed, so advance the file's length model.
func (t *Term) TypeAt(tag, p int, text string) {
	t.c.send(msg(Ttype).short(tag).long(p).str(text))
	t.pump()
	t.file(tag).length += len([]rune(text))
}

// StartFile asks the host to open the file with the given tag for viewing.
func (t *Term) StartFile(tag int) {
	t.c.send(msg(Tstartfile).short(tag).vlong(0x2000 + int64(tag)))
	t.pump()
}

// Search sends Tsearch (search for the last regexp).
func (t *Term) Search() {
	t.c.send(msg(Tsearch).short(0).long(0).long(0))
	t.pump()
}

// SetAutoRequest controls whether the terminal automatically fetches grown
// text with Trequest. Disable it to drive partial requests explicitly.
func (t *Term) SetAutoRequest(v bool) { t.autoReq = v }

// Request asks for runes [p,p+n) of file tag (Trequest).
func (t *Term) Request(tag, p, n int) {
	t.c.send(msg(Trequest).short(tag).long(p).short(n))
	t.pump()
}

// Check asks the host to poke the file with Hcheck (Tcheck).
func (t *Term) Check(tag int) {
	t.c.send(msg(Tcheck).short(tag))
	t.pump()
}

// Origin asks the host for an origin near line l in file tag (Torigin).
func (t *Term) Origin(tag, l, l1 int) {
	t.c.send(msg(Torigin).short(tag).long(l).long(l1))
	t.pump()
}

// Startnewfile opens a new empty frame (Tstartnewfile).
func (t *Term) Startnewfile() {
	t.c.send(msg(Tstartnewfile).vlong(0x3000))
	t.pump()
}

// Workfile sets the working file and its dot (Tworkfile).
func (t *Term) Workfile(tag, p0, p1 int) {
	t.c.send(msg(Tworkfile).short(tag).long(p0).long(p1))
	t.pump()
}

// Cut deletes [p0,p1) in the file (Tcut).
func (t *Term) Cut(tag, p0, p1 int) {
	t.c.send(msg(Tcut).short(tag).long(p0).long(p1))
	t.pump()
}

// Snarf copies [p0,p1) into the snarf buffer (Tsnarf).
func (t *Term) Snarf(tag, p0, p1 int) {
	t.c.send(msg(Tsnarf).short(tag).long(p0).long(p1))
	t.pump()
}

// Plumb sends a plumb request for [p0,p1) in the file (Tplumb).
func (t *Term) Plumb(tag, p0, p1 int) {
	t.c.send(msg(Tplumb).short(tag).long(p0).long(p1))
	t.pump()
}

// Paste inserts the snarf buffer at p0 (Tpaste).
func (t *Term) Paste(tag, p0 int) {
	t.c.send(msg(Tpaste).short(tag).long(p0))
	t.pump()
}

// Startsnarf asks the host to export its snarf buffer (Tstartsnarf).
func (t *Term) Startsnarf() {
	t.c.send(msg(Tstartsnarf))
	t.pump()
}

// Setsnarf sets the host snarf buffer to text (Tsetsnarf + raw bytes).
func (t *Term) Setsnarf(text string) {
	t.c.send(msg(Tsetsnarf).short(len(text)))
	t.c.sendRaw([]byte(text))
	t.pump()
}

// Send simulates the "send" action: snarf [p0,p1) of the command window and
// run it (Tsend).
func (t *Term) Send(p0, p1 int) {
	t.c.send(msg(Tsend).short(0).long(p0).long(p1))
	t.pump()
}

// SendText types text into the command window without a newline, then sends
// that range (Tsend) so the host runs it as a command. Ttype is not echoed
// back (the terminal is telling the host about chars it already has), so we
// advance our own length model by the typed runes.
func (t *Term) SendText(text string) {
	n := len([]rune(text))
	p0 := t.file(t.cmdtag).length
	t.c.send(msg(Ttype).short(t.cmdtag).long(p0).str(text))
	t.pump()
	t.file(t.cmdtag).length += n
	t.c.send(msg(Tsend).short(0).long(p0).long(p0 + n))
	t.pump()
}

// Dclick performs a double-click at p in the file (Tdclick).
func (t *Term) Dclick(tag, p int) {
	t.c.send(msg(Tdclick).short(tag).long(p))
	t.pump()
}

// Look searches for the literal text [p0,p1) (Tlook).
func (t *Term) Look(tag, p0, p1 int) {
	t.c.send(msg(Tlook).short(tag).long(p0).long(p1))
	t.pump()
}

// Write writes the file to disk (Twrite).
func (t *Term) Write(tag int) {
	t.c.send(msg(Twrite).short(tag))
	t.pump()
}

// Close asks the host to close the file (Tclose).
func (t *Term) Close(tag int) {
	t.c.send(msg(Tclose).short(tag))
	t.pump()
}

// CloseConn drops the connection to the host without sending Texit, so the
// host sees EOF (exercising the host's rescue/exit-on-EOF path).
func (t *Term) CloseConn() {
	if wc, ok := t.c.w.(interface{ Close() error }); ok {
		wc.Close()
	}
	select {
	case <-t.closed:
	case <-time.After(2 * time.Second):
	}
	t.pump()
}

// Exit sends Texit and waits for the host to close the connection.
func (t *Term) Exit() {
	t.c.send(msg(Texit))
	select {
	case <-t.closed:
	case <-time.After(2 * time.Second):
	}
	t.pump()
}

// Crash sends a message whose type byte is out of range. The host rejects it in
// inmesg with panic("inmesg") -> rescue -> abort, so (like CloseConn) it then
// just waits for the host to drop the connection.
func (t *Term) Crash() {
	t.c.send(msg(0xff)) // type > TMAX
	select {
	case <-t.closed:
	case <-time.After(2 * time.Second):
	}
	t.pump()
}

// SetExpect switches the terminal into expectation mode: the test script names
// every message, so pump() no longer waits for idle and Expect drives reading.
func (t *Term) SetExpect(v bool) { t.expect = v }

// SendStartcmdfile sends Tstartcmdfile, the second half of Start()'s bootstrap;
// the expectation-mode driver calls it after reading the host's Hversion.
func (t *Term) SendStartcmdfile() { t.c.send(msg(Tstartcmdfile).vlong(0x1000)) }

// Expect reads the next host message, verifies its rendering equals want, and
// handles it (the same Tack/Trequest/model side effects pump would run). It
// errors on mismatch, EOF, or a 2s timeout (a hung or short host) -- none of
// which happen on a passing run, so the common path never waits.
func (t *Term) Expect(want string) error {
	var m Hmsg
	select {
	case mm, ok := <-t.ch:
		if !ok {
			return fmt.Errorf("expected %q, got closed channel", want)
		}
		m = mm
	case <-t.closed:
		// The host closed; consume anything buffered before declaring EOF.
		select {
		case m = <-t.ch:
		default:
			return fmt.Errorf("expected %q, got EOF", want)
		}
	case <-time.After(2 * time.Second):
		return fmt.Errorf("expected %q, timed out", want)
	}
	got := render(m)
	t.handle(m)
	if got != want {
		return fmt.Errorf("expected %q, got %q", want, got)
	}
	return nil
}

// ExpectEOF, after the script is exhausted, returns any extra messages the host
// sent before closing (empty on success).
func (t *Term) ExpectEOF() []string {
	var extra []string
	for {
		select {
		case m, ok := <-t.ch:
			if !ok {
				return extra
			}
			extra = append(extra, render(m))
			t.handle(m)
		case <-t.closed:
			for {
				select {
				case m := <-t.ch:
					extra = append(extra, render(m))
					t.handle(m)
				default:
					return extra
				}
			}
		case <-time.After(2 * time.Second):
			return extra
		}
	}
}

// Mark and Since bracket the transcript so the -update path can record which
// messages each action produced.
func (t *Term) Mark() int               { return len(t.transcript) }
func (t *Term) Since(mark int) []string { return append([]string(nil), t.transcript[mark:]...) }

// Raw returns the concatenated raw Hmesg byte stream received from the host.
func (t *Term) Raw() []byte { return t.raw }

// Transcript returns the human-readable rendering of the Hmesg stream.
func (t *Term) Transcript() string {
	return strings.Join(t.transcript, "\n") + "\n"
}

func short(b []byte, i int) int {
	if i+2 > len(b) {
		return 0
	}
	return int(binary.LittleEndian.Uint16(b[i:]))
}

func long(b []byte, i int) int {
	if i+4 > len(b) {
		return 0
	}
	return int(binary.LittleEndian.Uint32(b[i:]))
}

// render produces a stable, readable line for an Hmesg.
func render(m Hmsg) string {
	d := m.Data
	name := HName(m.Typ)
	switch m.Typ {
	case Hack, Hunlock, Hexit:
		return name
	case Hplumb:
		// The payload carries an absolute wdir, so render only the name to keep
		// goldens stable; the raw-byte comparison still checks the full message.
		return name
	case Hversion, Hsetsnarf:
		return fmt.Sprintf("%s %d", name, short(d, 0))
	case Hcurrent, Hnewname, Hclean, Hdirty, Hdelname, Hclose, Hcheck, Hcheck0, Hunlockfile:
		return fmt.Sprintf("%s tag=%d", name, short(d, 0))
	case Hsnarflen:
		return fmt.Sprintf("%s %d", name, long(d, 0))
	case Horigin, Hmoveto:
		return fmt.Sprintf("%s tag=%d p=%d", name, short(d, 0), long(d, 2))
	case Hgrow, Hcut, Hsetdot:
		return fmt.Sprintf("%s tag=%d p=%d n=%d", name, short(d, 0), long(d, 2), long(d, 6))
	case Hbindname:
		return fmt.Sprintf("%s tag=%d", name, short(d, 0)) // omit ptr (terminal-chosen)
	case Hmovname:
		return fmt.Sprintf("%s tag=%d %q", name, short(d, 0), string(d[2:trimNul(d, 2)]))
	case Hsetpat:
		return fmt.Sprintf("%s %q", name, string(d[:trimNul(d, 0)]))
	case Hdata:
		return fmt.Sprintf("%s tag=%d p=%d %q", name, short(d, 0), long(d, 2), string(d[6:trimNul(d, 6)]))
	case Hgrowdata:
		return fmt.Sprintf("%s tag=%d p=%d n=%d %q", name, short(d, 0), long(d, 2), long(d, 6), string(d[10:trimNul(d, 10)]))
	default:
		return name + " " + strconv.Quote(string(d))
	}
}

func trimNul(b []byte, start int) int {
	for i := start; i < len(b); i++ {
		if b[i] == 0 {
			return i
		}
	}
	return len(b)
}
