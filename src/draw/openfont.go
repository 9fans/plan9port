package draw

import (
	"bytes"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"os/exec"
	"strings"
)

func parsefontscale(name string) (scale int, fname string) {
	i := 0
	scale = 0
	for i < len(name) && '0' <= name[i] && name[i] <= '9' {
		scale = scale*10 + int(name[i]) - '0'
		i++
	}
	if i < len(name) && name[i] == '*' && scale > 0 {
		return scale, name[i+1:]
	}
	return 1, name
}

// OpenFont reads the named file and returns the font it defines.
// The name may be an absolute path, or it may identify a file
// in a standard font directory:
// /lib/font/bit, /usr/local/plan9, /mnt/font, etc.
//
// In contrast to Plan 9, but matching Plan 9 from User Space,
// font names are a small language describing the desired font.
// See the package documentation for details.
func (d *Display) OpenFont(name string) (*Font, error) {
	// nil display is allowed, for querying font metrics
	// in non-draw program.
	if d != nil {
		d.mu.Lock()
		defer d.mu.Unlock()
	}
	return d.openFont(name)
}

func (d *Display) openFont1(name string) (*Font, error) {
	scale, fname := parsefontscale(name)

	data, err := ioutil.ReadFile(fname)

	if err != nil && strings.HasPrefix(fname, "/lib/font/bit/") {
		root := os.Getenv("PLAN9")
		if root == "" {
			root = "/usr/local/plan9"
		}
		name1 := root + "/font/" + fname[len("/lib/font/bit/"):]
		data1, err1 := ioutil.ReadFile(name1)
		fname, data, err = name1, data1, err1
		if scale > 1 {
			name = fmt.Sprintf("%d*%s", scale, fname)
		} else {
			name = fname
		}
	}

	if err != nil && strings.HasPrefix(fname, "/mnt/font/") {
		data1, err1 := fontPipe(fname[len("/mnt/font/"):])
		if err1 == nil {
			data, err = data1, err1
		}
	}
	if err != nil {
		return nil, err
	}

	f, err := d.buildFont(data, name)
	if err != nil {
		return nil, err
	}

	if scale != 1 {
		f.Scale = scale
		f.Height *= scale
		f.Ascent *= scale
		f.width *= scale
	}
	return f, nil
}

func swapfont(targ *Font, oldp, newp **Font) {
	if targ != *oldp {
		log.Fatalf("bad swapfont %p %p %p", targ, *oldp, *newp)
	}

	old := *oldp
	new := *newp
	var tmp Font
	copyfont(&tmp, old)
	copyfont(old, new)
	copyfont(new, &tmp)

	*oldp = new
	*newp = old
}

func copyfont(dst, src *Font) {
	dst.Display = src.Display
	dst.Name = src.Name
	dst.Height = src.Height
	dst.Ascent = src.Ascent
	dst.Scale = src.Scale
	dst.width = src.width
	dst.age = src.age
	dst.maxdepth = src.maxdepth
	dst.cache = src.cache
	dst.subf = src.subf
	dst.sub = src.sub
	dst.cacheimage = src.cacheimage
}

func hidpiname(f *Font) string {
	// If font name has form x,y return y.
	i := strings.Index(f.namespec, ",")
	if i >= 0 {
		return f.namespec[i+1:]
	}

	// If font name is /mnt/font/Name/Size/font, scale Size.
	if strings.HasPrefix(f.Name, "/mnt/font/") {
		i := strings.Index(f.Name[len("/mnt/font/"):], "/")
		if i < 0 {
			goto Scale
		}
		i += len("/mnt/font/") + 1
		if i >= len(f.Name) || f.Name[i] < '0' || '9' < f.Name[i] {
			goto Scale
		}
		j := i
		size := 0
		for j < len(f.Name) && '0' <= f.Name[j] && f.Name[j] <= '9' {
			size = size*10 + int(f.Name[j]) - '0'
			j++
		}
		return fmt.Sprintf("%s%d%s", f.Name[:i], size*2, f.Name[j:])
	}

	// Otherwise use pixel doubling.
Scale:
	return fmt.Sprintf("%d*%s", f.Scale*2, f.Name)
}

func loadhidpi(f *Font) {
	if f.hidpi == f {
		return
	}
	if f.hidpi != nil {
		swapfont(f, &f.lodpi, &f.hidpi)
		return
	}

	name := hidpiname(f)
	fnew, err := f.Display.openFont1(name)
	if err != nil {
		return
	}
	f.hidpi = fnew
	swapfont(f, &f.lodpi, &f.hidpi)
}

func (d *Display) openFont(name string) (*Font, error) {
	// If font name has form x,y use x for lodpi, y for hidpi
	namespec := name
	if i := strings.Index(name, ","); i >= 0 {
		name = name[:i]
	}

	f, err := d.openFont1(name)
	if err != nil {
		return nil, err
	}
	f.lodpi = f
	f.namespec = namespec

	// add to display list for when dpi changes.
	// d can be nil when invoked from mc.
	if d != nil {
		f.ondisplaylist = true
		f.prev = d.lastfont
		f.next = nil
		if f.prev != nil {
			f.prev.next = f
		} else {
			d.firstfont = f
		}
		d.lastfont = f

		// if this is a hi-dpi display, find hi-dpi version and swap
		if d.HiDPI() {
			loadhidpi(f)
		}
	}

	return f, nil
}

func fontPipe(name string) ([]byte, error) {
	data, err := exec.Command("fontsrv", "-pp", name).CombinedOutput()

	// Success marked with leading \001. Otherwise an error happened.
	if len(data) > 0 && data[0] != '\001' {
		i := bytes.IndexByte(data, '\n')
		if i >= 0 {
			data = data[:i]
		}
		return nil, fmt.Errorf("fontsrv -pp %s: %v", name, data)
	}
	if err != nil {
		return nil, err
	}
	return data[1:], nil
}
