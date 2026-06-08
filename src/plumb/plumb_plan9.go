package plumb

import (
	"9fans.net/go/plan9/client"
)

func mountPlumb() {
	fsys = &client.Fsys{Mtpt: "/mnt/plumb"}
	fsysErr = nil
}
