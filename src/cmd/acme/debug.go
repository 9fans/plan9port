package main

import "io"

// debugthread is the live editor's -D command loop (debug.c debugthread): it
// reads the -D protocol from script (os.Stdin for `acme -D`) and drives the
// editor with the same verb dispatch (dcmd via debugLoop) the frame-differential
// tests reach through runD. threadmain has already initialized the editor and
// spawned the input threads, so this runs only the read loop. Under -D there are
// no live GUI events, so those threads stay idle, and the mouse verbs drive
// textselect directly through a synthetic Mousectl (driver.go) rather than the
// real mousectl, so there is no contention with the mousethread. A clean Exit
// sends cexit, which unwinds threadmain, so the loop simply returns on EOF.
func (a *Acme) debugthread(script io.Reader) {
	a.debugLoop(script)
}
