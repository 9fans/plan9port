package draw

import (
	"os"
	"strconv"
	"strings"
)

var mss struct {
	lines   int
	percent float64
}

// Mousescrollsize computes the number of lines of text that
// should be scrolled in response to a mouse scroll wheel
// click.  Maxlines is the number of lines visible in the text
// window.
//
// The default scroll increment is one line.  This default can
// be overridden by setting the $mousescrollsize environment
// variable to an integer, which specifies a constant number of
// lines, or to a real number followed by a percent character,
// indicating that the scroll increment should be a percentage
// of the total number of lines in the window.  For example,
// setting $mousescrollsize to 50% causes a half-window scroll
// increment.
func MouseScrollSize(maxlines int) int {
	if mss.lines == 0 && mss.percent == 0 {
		if s := strings.TrimSpace(os.Getenv("mousescrollsize")); s != "" {
			if strings.HasSuffix(s, "%") {
				mss.percent, _ = strconv.ParseFloat(strings.TrimSpace(s[:len(s)-1]), 64)
			} else {
				mss.lines, _ = strconv.Atoi(s)
			}
		}
		if mss.lines == 0 && mss.percent == 0 {
			mss.lines = 1
		}
		if mss.percent >= 100 {
			mss.percent = 100
		}
	}

	if mss.lines != 0 {
		return mss.lines
	}
	return int(mss.percent * float64(maxlines) / 100.0)
}
