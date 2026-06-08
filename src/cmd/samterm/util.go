package main

func boolToInt(b bool) int {
	if b {
		return 1
	}
	return 0
}

// windex returns the index of layer l within t.l.
func (t *Term) windex(tx *Text, l *Flayer) int {
	//FIX: use unsafe to make O(1)
	for i := range tx.l {
		if &tx.l[i] == l {
			return i
		}
	}
	panic("windex")
}
