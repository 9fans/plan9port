// Package sam defines the wire protocol shared by sam and samterm.
package sam

import (
	"encoding/binary"
	"fmt"
	"io"
	"slices"
	"strconv"
)

// A Tmesg is a type for a message sent by the terminal (samterm).
type Tmesg int

const (
	Tversion Tmesg = iota
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

var tname = [...]string{
	Tversion:      "Tversion",
	Tstartcmdfile: "Tstartcmdfile",
	Tcheck:        "Tcheck",
	Trequest:      "Trequest",
	Torigin:       "Torigin",
	Tstartfile:    "Tstartfile",
	Tworkfile:     "Tworkfile",
	Ttype:         "Ttype",
	Tcut:          "Tcut",
	Tpaste:        "Tpaste",
	Tsnarf:        "Tsnarf",
	Tstartnewfile: "Tstartnewfile",
	Twrite:        "Twrite",
	Tclose:        "Tclose",
	Tlook:         "Tlook",
	Tsearch:       "Tsearch",
	Tsend:         "Tsend",
	Tdclick:       "Tdclick",
	Tstartsnarf:   "Tstartsnarf",
	Tsetsnarf:     "Tsetsnarf",
	Tack:          "Tack",
	Texit:         "Texit",
	Tplumb:        "Tplumb",
}

func (t Tmesg) String() string {
	if 0 <= int(t) && int(t) < len(tname) {
		return tname[t]
	}
	return fmt.Sprintf("Ttype(%d)", int(t))
}

// An Hmesg is a type for a message sent by the host (sam -R).
type Hmesg int

const (
	Hversion Hmesg = iota
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

var hname = [...]string{
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

func (t Hmesg) String() string {
	if 0 <= int(t) && int(t) < len(hname) {
		return hname[t]
	}
	return fmt.Sprintf("Htype(%d)", int(t))
}

// An Hframe is a protocol message read from the host.
type Hframe struct {
	Typ   Hmesg
	Data  []byte
	Extra []byte
	Raw   []byte
}

func ReadHframe(r io.Reader) (*Hframe, error) {
	raw := make([]byte, 3, 128)
	if _, err := io.ReadFull(r, raw); err != nil {
		return nil, err
	}
	n := int(binary.LittleEndian.Uint16(raw[1:]))
	raw = slices.Grow(raw, n)
	data := raw[3 : 3+n]
	if _, err := io.ReadFull(r, data); err != nil {
		return nil, err
	}
	typ := Hmesg(raw[0])
	var extra []byte
	if (typ == Hplumb || typ == Hsetsnarf) && len(data) >= 2 {
		xn := int(binary.LittleEndian.Uint16(data[0:]))
		raw = slices.Grow(raw, xn)
		data = raw[3 : 3+n]
		extra = raw[3+n : 3+n+xn]
		if _, err := io.ReadFull(r, extra); err != nil {
			return nil, err
		}
	}
	return &Hframe{typ, data, extra, raw}, nil
}

func short(b []byte, i int) string {
	if i+2 > len(b) {
		return "?"
	}
	return fmt.Sprint(binary.LittleEndian.Uint16(b[i:]))
}

func long(b []byte, i int) string {
	if i+4 > len(b) {
		return "?"
	}
	return fmt.Sprint(binary.LittleEndian.Uint32(b[i:]))
}

func vlong(b []byte, i int) string {
	if i+8 > len(b) {
		return "?"
	}
	return fmt.Sprint(binary.LittleEndian.Uint64(b[i:]))
}

func (m *Hframe) String() string {
	d := m.Data
	name := m.Typ.String()
	switch m.Typ {
	case Hack, Hunlock, Hexit:
		return name
	case Hplumb:
		// The payload carries an absolute wdir, so render only the name to keep
		// goldens stable; the raw-byte comparison still checks the full message.
		return name
	case Hversion:
		return fmt.Sprintf("%s %v", name, short(d, 0))
	case Hsetsnarf:
		snarf := string(m.Extra)
		if len(snarf) > 1000 {
			snarf = snarf[:1000] + "..."
		}
		return fmt.Sprintf("%s %v %q", name, short(d, 0), snarf)
	case Hcurrent, Hnewname, Hclean, Hdirty, Hdelname, Hclose, Hcheck, Hcheck0, Hunlockfile:
		return fmt.Sprintf("%s tag=%v", name, short(d, 0))
	case Hsnarflen:
		return fmt.Sprintf("%s %v", name, long(d, 0))
	case Horigin, Hmoveto:
		return fmt.Sprintf("%s tag=%v p=%v", name, short(d, 0), long(d, 2))
	case Hgrow, Hcut, Hsetdot:
		return fmt.Sprintf("%s tag=%v p=%v n=%v", name, short(d, 0), long(d, 2), long(d, 6))
	case Hbindname:
		return fmt.Sprintf("%s tag=%v", name, short(d, 0)) // omit ptr (terminal-chosen)
	case Hmovname:
		return fmt.Sprintf("%s tag=%v %q", name, short(d, 0), string(d[2:]))
	case Hsetpat:
		return fmt.Sprintf("%s %q", name, string(d))
	case Hdata:
		return fmt.Sprintf("%s tag=%v p=%v %q", name, short(d, 0), long(d, 2), string(d[6:]))
	case Hgrowdata:
		return fmt.Sprintf("%s tag=%v p=%v n=%v %q", name, short(d, 0), long(d, 2), long(d, 6), string(d[10:]))
	default:
		return name + " " + strconv.Quote(string(d))
	}
}
