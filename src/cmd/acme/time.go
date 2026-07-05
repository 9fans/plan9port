package main

import (
	"sync"
	"time"
)

// Go port of acme's timer subsystem (time.c). In C a dedicated timerproc sleeps
// ~10ms, measures the elapsed time, decrements every active Timer by it, and
// fires (sends on the timer's channel) those that reach zero, recycling Timers
// through a free list. The proc + ctimer channel exist only to decouple that
// real-time loop from its single consumer; the single-threaded -D port instead
// arms timers directly and advances them with timertick(dt), the body of
// timerproc's per-tick loop. The live driver (the keyboard/mouse threads, which
// call timerstart for the tag-commit and scroll auto-repeat timers) is a
// goroutine that sleeps and calls timertick with the real elapsed ms; tests call
// timertick with a chosen dt, so the firing/cancel logic is exercised
// deterministically without wall-clock sleeps.

// Timer fires by sending on C once it has been ticked for its full duration
// (time.c Timer). C is buffered one deep so a fire is delivered even if the
// consumer is not yet waiting (C uses an unbuffered channel with a non-blocking
// send that retries each tick until the consumer is ready; for acme's
// select-driven consumers the two are equivalent).
type Timer struct {
	c      chan int
	dt     int
	cancel bool
	next   *Timer // free-list link
}

// timers holds the active timers and the free list (time.c statics).
type timers struct {
	lk     *sync.Mutex // serializes timerproc vs timerstart/cancel; nil under -D
	active []*Timer
	free   *Timer
}

// timerstart arms a timer for dt ms, reusing one from the free list if possible
// (time.c timerstart). In C the new timer is handed to timerproc over ctimer;
// here it is added to the active list directly.
func (a *Acme) timerstart(dt int) *Timer {
	if a.tmr.lk != nil {
		a.tmr.lk.Lock()
		defer a.tmr.lk.Unlock()
	}
	t := a.tmr.free
	if t != nil {
		a.tmr.free = t.next
		select { // drop any unconsumed fire from a previous use
		case <-t.c:
		default:
		}
	} else {
		t = &Timer{c: make(chan int, 1)}
	}
	t.next = nil
	t.dt = dt
	t.cancel = false
	a.tmr.active = append(a.tmr.active, t)
	return t
}

// timercancel marks t to be stopped at the next tick (time.c timercancel). It
// runs on a consumer thread (keyboardthread/scrsleep) concurrently with the
// timerproc goroutine's timertick, which reads t.cancel, so it takes the timer
// lock. C needs no lock because libthread schedules its procs cooperatively; the
// Go goroutines are truly parallel.
func (a *Acme) timercancel(t *Timer) {
	if a.tmr.lk != nil {
		a.tmr.lk.Lock()
		defer a.tmr.lk.Unlock()
	}
	t.cancel = true
}

// timerstop returns t to the free list (time.c timerstop). It runs on a consumer
// thread after a fire, so it locks; timertick, already holding the lock, calls
// timerstopLocked instead.
func (a *Acme) timerstop(t *Timer) {
	if a.tmr.lk != nil {
		a.tmr.lk.Lock()
		defer a.tmr.lk.Unlock()
	}
	a.timerstopLocked(t)
}

func (a *Acme) timerstopLocked(t *Timer) {
	t.next = a.tmr.free
	a.tmr.free = t
}

// timerinit launches the real timer goroutine that calls timertick every ~10ms
// with the wall-clock elapsed time (time.c timerproc + timerinit). The -D path
// drives timertick directly for determinism; the live editor uses this proc.
func (a *Acme) timerinit() {
	a.tmr.lk = new(sync.Mutex)
	go a.timerproc()
}

// timerproc is the live wall-clock driver of timertick (time.c timerproc).
func (a *Acme) timerproc() {
	old := time.Now()
	for {
		time.Sleep(10 * time.Millisecond)
		now := time.Now()
		dt := int(now.Sub(old) / time.Millisecond)
		old = now
		if dt < 0 {
			// untested: timer edge path not exercised
			continue
		}
		a.tmr.lk.Lock()
		a.timertick(dt)
		a.tmr.lk.Unlock()
	}
}

// timertick advances every active timer by dt ms, firing (and freeing) those
// that reach zero and stopping cancelled ones -- the body of time.c timerproc's
// per-tick loop. A real driver calls this with the elapsed ms each ~10ms; tests
// call it with a chosen dt.
func (a *Acme) timertick(dt int) {
	act := a.tmr.active[:0]
	for _, x := range a.tmr.active {
		x.dt -= dt
		switch {
		case x.cancel:
			// cancelled: free it and drop from active (C timerstop + del).
			a.timerstopLocked(x) // already under a.tmr.lk (timerproc / -D direct)
		case x.dt <= 0:
			// fire via a non-blocking send (C nbsendul): on success drop from
			// active but do NOT free -- the consumer frees via timerstop. On
			// failure (consumer not yet receiving) keep it active and retry next
			// tick, exactly as C's timerproc does to avoid a send/recv deadlock.
			select {
			case x.c <- 0:
			default:
				// untested: timer edge path not exercised
				act = append(act, x)
			}
		default:
			act = append(act, x)
		}
	}
	a.tmr.active = act
}
