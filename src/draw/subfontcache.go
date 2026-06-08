package draw

import "sync"

/*
 * Easy versions of the cache routines; may be substituted by fancier ones for other purposes
 */

var lastfont struct {
	sync.Mutex
	name string
	sub  *subfont
}

func lookupsubfont(d *Display, name string) *subfont {
	if d != nil && name == "*default*" {
		return d.defaultSubfont
	}
	lastfont.Lock()
	defer lastfont.Unlock()
	if lastfont.name == name && d == lastfont.sub.Bits.Display {
		lastfont.sub.ref++
		return lastfont.sub
	}
	return nil
}

func installsubfont(name string, subfont *subfont) {
	lastfont.Lock()
	defer lastfont.Unlock()
	lastfont.name = name
	lastfont.sub = subfont /* notice we don't free the old one; that's your business */
}

func uninstallsubfont(subfont *subfont) {
	lastfont.Lock()
	defer lastfont.Unlock()
	if subfont == lastfont.sub {
		lastfont.name = ""
		lastfont.sub = nil
	}
}
