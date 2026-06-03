package samproto

import (
	"bufio"
	"encoding/binary"
	"fmt"
	"io"
)

// Tmesg values (terminal->host), mesg.h enum Tmesg.
const (
	Tversion = iota
	Tstartcmdfile
	Tcheck
	Trequest
	Torigin
	Tstartfile
	Tworkfile
	Ttype
	Tcut
	Tpaste
	Tsnarf
	Tstartnewfile
	Twrite
	Tclose
	Tlook
	Tsearch
	Tsend
	Tdclick
	Tstartsnarf
	Tsetsnarf
	Tack
	Texit
	Tplumb
)

// Hmesg values (host->terminal), mesg.h enum Hmesg.
const (
	Hversion = iota
	Hbindname
	Hcurrent
	Hnewname
	Hmovname
	Hgrow
	Hcheck0
	Hcheck
	Hunlock
	Hdata
	Horigin
	Hunlockfile
	Hsetdot
	Hgrowdata
	Hmoveto
	Hclean
	Hdirty
	Hcut
	Hsetpat
	Hdelname
	Hclose
	Hsetsnarf
	Hsnarflen
	Hack
	Hexit
	Hplumb
)

var hname = map[int]string{
	Hversion:    "Hversion",
	Hbindname:   "Hbindname",
	Hcurrent:    "Hcurrent",
	Hnewname:    "Hnewname",
	Hmovname:    "Hmovname",
	Hgrow:       "Hgrow",
	Hcheck0:     "Hcheck0",
	Hcheck:      "Hcheck",
	Hunlock:     "Hunlock",
	Hdata:       "Hdata",
	Horigin:     "Horigin",
	Hunlockfile: "Hunlockfile",
	Hsetdot:     "Hsetdot",
	Hgrowdata:   "Hgrowdata",
	Hmoveto:     "Hmoveto",
	Hclean:      "Hclean",
	Hdirty:      "Hdirty",
	Hcut:        "Hcut",
	Hsetpat:     "Hsetpat",
	Hdelname:    "Hdelname",
	Hclose:      "Hclose",
	Hsetsnarf:   "Hsetsnarf",
	Hsnarflen:   "Hsnarflen",
	Hack:        "Hack",
	Hexit:       "Hexit",
	Hplumb:      "Hplumb",
}

// HName returns the printable name of an Hmesg type.
func HName(typ int) string {
	if n, ok := hname[typ]; ok {
		return n
	}
	return fmt.Sprintf("H?%d", typ)
}

// Hmsg is a decoded host message.
type Hmsg struct {
	Typ  int
	Data []byte // payload bytes (after the 3-byte header)
	Raw  []byte // full framed message bytes
}

// conn frames messages on the wire.
type conn struct {
	r   *bufio.Reader
	w   io.Writer
	out []byte // scratch for building a Tmesg
}

func newConn(r io.Reader, w io.Writer) *conn {
	return &conn{r: bufio.NewReader(r), w: w}
}

// readH reads one framed Hmesg.
func (c *conn) readH() (Hmsg, error) {
	hdr := make([]byte, 3)
	if _, err := io.ReadFull(c.r, hdr); err != nil {
		return Hmsg{}, err
	}
	count := int(binary.LittleEndian.Uint16(hdr[1:]))
	data := make([]byte, count)
	if _, err := io.ReadFull(c.r, data); err != nil {
		return Hmsg{}, err
	}
	raw := append(append([]byte(nil), hdr...), data...)
	// Hplumb is followed by n unframed payload bytes (the packed plumb message),
	// where n is the short in the frame body; the host writes them with a raw
	// write after the header (sam's outTs(Hplumb,n) + Write). Consume them so
	// the frame reader stays in sync.
	if int(hdr[0]) == Hplumb && len(data) >= 2 {
		n := int(binary.LittleEndian.Uint16(data))
		payload := make([]byte, n)
		if _, err := io.ReadFull(c.r, payload); err != nil {
			return Hmsg{}, err
		}
		raw = append(raw, payload...)
		data = append(data, payload...)
	}
	return Hmsg{Typ: int(hdr[0]), Data: data, Raw: raw}, nil
}

// builder accumulates a Tmesg payload.
type builder struct {
	typ  int
	data []byte
}

func msg(typ int) *builder { return &builder{typ: typ} }

func (b *builder) short(v int) *builder {
	b.data = binary.LittleEndian.AppendUint16(b.data, uint16(v))
	return b
}

func (b *builder) long(v int) *builder {
	b.data = binary.LittleEndian.AppendUint32(b.data, uint32(v))
	return b
}

func (b *builder) vlong(v int64) *builder {
	b.data = binary.LittleEndian.AppendUint64(b.data, uint64(v))
	return b
}

func (b *builder) bytes(p []byte) *builder {
	b.data = append(b.data, p...)
	return b
}

func (b *builder) str(s string) *builder {
	b.data = append(b.data, []byte(s)...)
	b.data = append(b.data, 0)
	return b
}

// send frames and writes a Tmesg.
func (c *conn) send(b *builder) error {
	n := len(b.data)
	frame := make([]byte, 0, n+3)
	frame = append(frame, byte(b.typ))
	frame = binary.LittleEndian.AppendUint16(frame, uint16(n))
	frame = append(frame, b.data...)
	_, err := c.w.Write(frame)
	return err
}

// sendRaw writes unframed bytes (used after Tsetsnarf, which the host reads
// directly via rcvchar).
func (c *conn) sendRaw(p []byte) error {
	_, err := c.w.Write(p)
	return err
}
