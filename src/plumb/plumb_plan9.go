package plumb

import (
	"9fans.net/go/plan9/client"
)

func mountPlumb() (*client.Fsys, error) {
	return &client.Fsys{Mtpt: "/mnt/plumb"}, nil
}
