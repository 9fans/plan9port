package draw

import (
	"fmt"
	"os"
	"strings"
)

/*
 * Default version: convert to file name
 */

func subfontname(cfname, fname string, maxdepth int) string {
	scale, base := parsefontscale(fname)

	t := cfname
	if cfname == "*default*" {
		return t
	}
	if !strings.HasPrefix(t, "/") {
		dir := base
		i := strings.LastIndex(dir, "/")
		if i >= 0 {
			dir = dir[:i]
		} else {
			dir = "."
		}
		t = dir + "/" + t
	}
	if maxdepth > 8 {
		maxdepth = 8
	}
	for i := 3; i >= 0; i-- {
		if 1<<uint(i) > maxdepth {
			continue
		}
		// try i-bit grey
		tmp2 := fmt.Sprintf("%s.%d", t, i)
		if _, err := os.Stat(tmp2); err == nil {
			if scale > 1 {
				tmp2 = fmt.Sprintf("%d*%s", scale, tmp2)
			}
			return tmp2
		}
	}

	// try default
	if strings.HasPrefix(t, "/mnt/font/") {
		if scale > 1 {
			t = fmt.Sprintf("%d*%s", scale, t)
		}
		return t
	}
	if _, err := os.Stat(t); err == nil {
		if scale > 1 {
			t = fmt.Sprintf("%d*%s", scale, t)
		}
		return t
	}

	return ""
}
