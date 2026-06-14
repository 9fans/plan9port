// +build !plan9

package plumb

import (
	"9fans.net/go/plan9/client"
)

func mountPlumb() (*client.Fsys, error) {
	return client.MountService("plumb")
}
