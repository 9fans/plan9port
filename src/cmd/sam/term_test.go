package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"strings"
	"testing/synctest"
	"time"

	"9fans.net/plan9port/internal/sam"
)

// ScriptTerm is a fake script-based samterm connected to a real sam.
type ScriptTerm struct {
	r          io.Reader
	w          io.WriteCloser
	read       chan *sam.Hframe // messages received from sam
	cmdtag     int              // tag for command window (~~sam~~)
	cmdend     int              // end position of command window
	raw        []byte           // concatenated raw Hmesg bytes, in order
	transcript []string         // human-readable rendering
}

// New starts a terminal on the given streams (r = host stdout, w = host stdin).
func NewTerm(r io.Reader, w io.WriteCloser) *ScriptTerm {
	t := &ScriptTerm{
		r:      r,
		w:      w,
		read:   make(chan *sam.Hframe, 64),
		cmdtag: -1,
	}
	go t.reader()
	return t
}

// reader reads messages and sends them on t.read.
func (t *ScriptTerm) reader() {
	for {
		m, err := sam.ReadHframe(t.r)
		if err != nil {
			close(t.read)
			return
		}
		t.read <- m
	}
}

// CmdFile is the "pointer" used for the ~~sam~~ window.
const CmdFile = 0x1000

// Raw returns the raw byte sequence received from sam.
func (t *ScriptTerm) Raw() []byte { return t.raw }

// Transcript returns a human-readable transcript of messages received from sam.
func (t *ScriptTerm) Transcript() string {
	return strings.Join(t.transcript, "\n") + "\n"
}

// Send sends a protocol message to sam.
func (t *ScriptTerm) Send(typ sam.Tmesg, args ...any) {
	b := make([]byte, 3)
	b[0] = byte(typ)
	for _, x := range args {
		switch x := x.(type) {
		default:
			panic(fmt.Sprintf("bad type in Send: %T", x))
		case uint16:
			b = binary.LittleEndian.AppendUint16(b, x)
		case uint32:
			b = binary.LittleEndian.AppendUint32(b, x)
		case uint64:
			b = binary.LittleEndian.AppendUint64(b, x)
		case string:
			b = append(b, x...)
			b = append(b, 0)
		case []byte:
			b = append(b, x...)
		}
	}
	binary.LittleEndian.PutUint16(b[1:], uint16(len(b)-3))
	t.w.Write(b)

	if typ == sam.Ttype && len(args) >= 3 && args[0] == uint16(t.cmdtag) {
		t.cmdend += len([]rune(args[2].(string)))
	}
}

// WriteRaw writes the raw bytes b to sam.
func (t *ScriptTerm) WriteRaw(b []byte) error {
	_, err := t.w.Write(b)
	return err
}

// Cmd adds text to the command window.
func (t *ScriptTerm) Cmd(text string) {
	t.Send(sam.Ttype, uint16(t.cmdtag), uint32(t.cmdend), text)
}

// Shutdown closes the writer connected to sam.
func (t *ScriptTerm) Shutdown() {
	t.w.Close()
}

// Wait reads from sam until it stops sending responses.
func (t *ScriptTerm) Wait() []string {
	n := len(t.transcript)
	done := make(chan bool)
	go func() {
		synctest.Wait()
		done <- true
	}()
Loop:
	for {
		select {
		case m, ok := <-t.read:
			if !ok {
				<-done
				break Loop
			}
			t.update(m)
		case <-done:
			break Loop
		}
	}
	return t.transcript[n:]
}

// Expect reads the next expected message from sam.
func (t *ScriptTerm) Expect(want string) error {
	select {
	case <-time.After(2 * time.Second):
		return fmt.Errorf("timeout waiting for: %s", want)
	case m, ok := <-t.read:
		if !ok {
			return fmt.Errorf("EOF waiting for: %s", want)
		}
		have := m.String()
		t.update(m)
		if have != want {
			return fmt.Errorf("have %q, want %q", have, want)
		}
		return nil
	}
}

// ExpectEOF reads an EOF from sam or else returns the messages it received.
func (t *ScriptTerm) ExpectEOF() []string {
	var extra []string
	for {
		select {
		case <-time.After(2 * time.Second):
			return append(extra, "timed out waiting for EOF")
		case m, ok := <-t.read:
			if !ok {
				return extra
			}
			extra = append(extra, m.String())
		}
	}
}

// update updates t's metadata based on receiving m from the host.
func (t *ScriptTerm) update(m *sam.Hframe) {
	t.raw = append(t.raw, m.Raw...)
	t.transcript = append(t.transcript, m.String())
	switch m.Typ {
	case sam.Hbindname:
		if vlong(m.Data, 2) == CmdFile {
			t.cmdtag = short(m.Data, 0)
		}
	case sam.Hgrow:
		tag := short(m.Data, 0)
		n := long(m.Data, 6)
		if tag == t.cmdtag {
			t.cmdend += n
		}
	case sam.Hgrowdata:
		tag := short(m.Data, 0)
		n := long(m.Data, 6)
		if tag == t.cmdtag {
			t.cmdend += n
		}
	case sam.Hcut:
		tag := short(m.Data, 0)
		n := long(m.Data, 6)
		if tag == t.cmdtag {
			t.cmdend -= n
		}
	}
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

func vlong(b []byte, i int) int {
	if i+8 > len(b) {
		return 0
	}
	return int(binary.LittleEndian.Uint64(b[i:]))
}
