package draw

// Free frees the server resources for the Subfont. Subfonts have a finalizer that
// calls Free automatically, if necessary, for garbage collected Images, but it
// is more efficient to be explicit.
// TODO: Implement the finalizer!
func (f *subfont) Free() {
	if f == nil {
		return
	}
	f.Bits.Display.mu.Lock()
	defer f.Bits.Display.mu.Unlock()
	f.free()
}

func (f *subfont) free() {
	if f == nil {
		return
	}
	f.ref--
	if f.ref > 0 {
		return
	}
	uninstallsubfont(f)
	f.Bits.free()
}
