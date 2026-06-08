package draw

import "fmt"

// A Color represents an RGBA value, 8 bits per element: 0xRRGGBBAA.
//
// The element values correspond to illumination,
// so 0x00000000 is transparent, 0x000000FF is opaque black,
// 0xFFFFFFFF is opaque white, 0xFF0000FF is opaque red, and so on.
//
// The R, G, B values have been pre-multiplied by A (alpha).
// For example, a 50% red is 0x7F00007F not 0xFF00007F.
// See also the WithAlpha method.
type Color uint32

// RGBA returns 16-bit r, g, b, a values for the color c,
// implementing the image/color package's Color interface.
func (c Color) RGBA() (r, g, b, a uint32) {
	r = uint32(c >> 24)
	g = uint32(c>>16) & 0xFF
	b = uint32(c>>8) & 0xFF
	a = uint32(c) & 0xFF
	return r | r<<8, g | g<<8, b | b<<8, a | a<<8
}

const (
	Transparent   Color = 0x00000000
	Opaque        Color = 0xFFFFFFFF
	Black         Color = 0x000000FF
	White         Color = 0xFFFFFFFF
	Red           Color = 0xFF0000FF
	Green         Color = 0x00FF00FF
	Blue          Color = 0x0000FFFF
	Cyan          Color = 0x00FFFFFF
	Magenta       Color = 0xFF00FFFF
	Yellow        Color = 0xFFFF00FF
	PaleYellow    Color = 0xFFFFAAFF
	DarkYellow    Color = 0xEEEE9EFF
	DarkGreen     Color = 0x448844FF
	PaleGreen     Color = 0xAAFFAAFF
	MedGreen      Color = 0x88CC88FF
	DarkBlue      Color = 0x000055FF
	PaleBlueGreen Color = 0xAAFFFFFF
	PaleBlue      Color = 0x0000BBFF
	BlueGreen     Color = 0x008888FF
	GreyGreen     Color = 0x55AAAAFF
	PaleGreyGreen Color = 0x9EEEEEFF
	YellowGreen   Color = 0x99994CFF
	MedBlue       Color = 0x000099FF
	GreyBlue      Color = 0x005DBBFF
	PaleGreyBlue  Color = 0x4993DDFF
	PurpleBlue    Color = 0x8888CCFF

	// NoFill is a special value recognized by AllocImage.
	NoFill Color = 0xFFFFFF00
)

// WithAlpha performs the alpha computation for a color,
// ignoring its initial alpha value and multiplying the components
// by the supplied alpha. For example, Red.WithAlpha(0x7F)
// is a 50% red color value.
func (c Color) WithAlpha(alpha uint8) Color {
	r := uint32(c >> 24)
	g := uint32(c>>16) & 0xFF
	b := uint32(c>>8) & 0xFF
	r = (r * uint32(alpha)) / 255
	g = (g * uint32(alpha)) / 255
	b = (b * uint32(alpha)) / 255
	return Color(r<<24 | g<<16 | b<<8 | uint32(alpha))
}

// Pix represents a pixel format described simple notation: r8g8b8 for RGB24, m8
// for color-mapped 8 bits, etc. The representation is 8 bits per channel,
// starting at the low end, with each byte represnted as a channel specifier
// (CRed etc.) in the high 4 bits and the number of pixels in the low 4 bits.
type Pix uint32

const (
	CRed = iota
	CGreen
	CBlue
	CGrey
	CAlpha
	CMap
	CIgnore
	NChan
)

var (
	GREY1  Pix = MakePix(CGrey, 1)
	GREY2  Pix = MakePix(CGrey, 2)
	GREY4  Pix = MakePix(CGrey, 4)
	GREY8  Pix = MakePix(CGrey, 8)
	CMAP8  Pix = MakePix(CMap, 8)
	RGB15  Pix = MakePix(CIgnore, 1, CRed, 5, CGreen, 5, CBlue, 5)
	RGB16      = MakePix(CRed, 5, CGreen, 6, CBlue, 5)
	RGB24      = MakePix(CRed, 8, CGreen, 8, CBlue, 8)
	BGR24      = MakePix(CBlue, 8, CGreen, 8, CRed, 8)
	RGBA32     = MakePix(CRed, 8, CGreen, 8, CBlue, 8, CAlpha, 8)
	ARGB32     = MakePix(CAlpha, 8, CRed, 8, CGreen, 8, CBlue, 8) // stupid VGAs
	ABGR32     = MakePix(CAlpha, 8, CBlue, 8, CGreen, 8, CRed, 8)
	XRGB32     = MakePix(CIgnore, 8, CRed, 8, CGreen, 8, CBlue, 8)
	XBGR32     = MakePix(CIgnore, 8, CBlue, 8, CGreen, 8, CRed, 8)
)

// MakePix returns a Pix by placing the successive integers into 4-bit nibbles.
func MakePix(list ...int) Pix {
	var p Pix
	for _, x := range list {
		p <<= 4
		p |= Pix(x)
	}
	return p
}

// Split returns the succesive integers making up p.
// That is, MakePix(p.Split()) == p.
// For example, RGB16.Split() is [CRed 5, CGreen, 6, CBlue, 5].
func (p Pix) Split() []int {
	var list []int
	for p != 0 {
		list = append(list, int(p&15), int(p>>4)&15)
		p >>= 8
	}
	for i, j := 0, len(list)-1; i < j; i, j = i+1, j-1 {
		list[i], list[j] = list[j], list[i]
	}
	return list
}

// ParsePix is the reverse of String, turning a pixel string such as "r8g8b8" into a Pix value.
func ParsePix(s string) (Pix, error) {
	var p Pix
	s0 := s
	if len(s) > 8 {
		goto Malformed
	}
	for ; len(s) > 0; s = s[2:] {
		if len(s) == 1 {
			goto Malformed
		}
		p <<= 4
		switch s[0] {
		default:
			goto Malformed
		case 'r':
			p |= CRed
		case 'g':
			p |= CGreen
		case 'b':
			p |= CBlue
		case 'a':
			p |= CAlpha
		case 'k':
			p |= CGrey
		case 'm':
			p |= CMap
		case 'x':
			p |= CIgnore
		}
		p <<= 4
		if s[1] < '1' || s[1] > '8' {
			goto Malformed
		}
		p |= Pix(s[1] - '0')
	}
	return p, nil

Malformed:
	return 0, fmt.Errorf("malformed pix descriptor %q", s0)
}

// String prints the pixel format as a string: "r8g8b8" for example.
func (p Pix) String() string {
	var buf [8]byte
	i := len(buf)
	if p == 0 {
		return "0"
	}
	for p > 0 {
		i -= 2
		buf[i] = "rgbkamxzzzzzzzzz"[(p>>4)&15]
		buf[i+1] = "0123456789abcdef"[p&15]
		p >>= 8
	}
	return string(buf[i:])
}

func (p Pix) Depth() int {
	n := 0
	for p > 0 {
		n += int(p & 15)
		p >>= 8
	}
	return n
}
