package draw

// SetLabel sets the host window's title.
func (d *Display) SetLabel(label string) {
	d.conn.Label(label)
}

// Top moves the host window to the top of the host window pile.
func (d *Display) Top() {
	d.conn.Top()
}

// Resize resizes the host window.
func (d *Display) Resize(r Rectangle) {
	d.conn.Resize(r)
}
