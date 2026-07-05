package draw

import (
	"os"
)

// Only called during init, so no need to synchronize.

var theDisplay *Display

func display() *Display {
	os.Setenv("DEVDRAW_TEST_MEMORY", "1")
	if theDisplay == nil {
		var err error
		theDisplay, err = Init(nil, "", "test", "", nil)
		if err != nil {
			panic(err)
		}
	}
	return theDisplay
}
