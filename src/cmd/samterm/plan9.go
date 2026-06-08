//FIX: rename to sys.go

package main

import (
	"fmt"
	"os"
	"strconv"

	"9fans.net/plan9port/draw"
)

//FIX: add usage like in C

//FIX: rename argv to args
func (t *Term) getscreen(argv []string) error {
	//FIX: use rsc.io/getopt
	for i := 0; i < len(argv); i++ {
		a := argv[i]
		switch {
		case a == "-a":
			t.autoindent = 1
		case a == "-W":
			i++
			if i < len(argv) {
				t.winsize = argv[i]
			}
		}
	}
	disp, err := draw.Init(nil, "", "sam", t.winsize)
	if err != nil {
		return err
	}
	t.disp = disp
	t.font = disp.Font
	if ts := os.Getenv("tabstop"); ts != "" {
		if n, err := strconv.Atoi(ts); err == nil {
			t.maxtab = n
		}
	}
	//FIX: set t.screen = disp.ScreenImage and use t.screen throughout
	disp.ScreenImage.Draw(disp.ScreenImage.Clipr, disp.White, nil, draw.ZP)
	return nil
}

func (t *Term) screensize() (w, h int, ok bool) {
	return 0, 0, false
}

//FIX: change to use slice length as length instead of nc and n.
func (t *Term) snarfswap(fromsam []byte, nc int) (tosam []byte, n int) {
	buf := make([]byte, SNARFSIZE)
	got, size, err := t.disp.ReadSnarf(buf)
	if err == nil && size > 0 {
		if got > size {
			got = size
		}
		tosam = append([]byte(nil), buf[:got]...)
	}
	t.disp.WriteSnarf(fromsam[:nc])
	return tosam, len(tosam)
}

//FIX: C prints 'rcvstring' as well
func (t *Term) dumperrmsg(count, typ, count0, c int) {
	fmt.Fprintf(os.Stderr, "samterm: host mesg: count %d %x %x %x ...ignored\n",
		count, typ, count0, c)
}

//FIX: lost C removeextern, extproc, extstart, plumbformat, plumbproc, plumbstart, hostproc, hoststart
