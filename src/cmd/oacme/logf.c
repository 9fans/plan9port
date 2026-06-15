#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <plumb.h>
#include <libsec.h>
#include "dat.h"
#include "fns.h"

// State for global log file.
typedef struct Log Log;
struct Log
{
	QLock lk;
	Rendez r;

	vlong start; // msg[0] corresponds to 'start' in the global sequence of events

	// queued events (nev=entries in ev, mev=capacity of p)
	char **ev;
	int nev;
	int mev;

	// open acme/put files that need to read events
	Fid **f;
	int nf;
	int mf;

	// active (blocked) reads waiting for events
	Xfid **read;
	int nread;
	int mread;
};

static Log eventlog;

void
xfidlogopen(Xfid *x)
{
	qlock(&eventlog.lk);
	if(eventlog.nf >= eventlog.mf) {
		eventlog.mf = eventlog.mf*2;
		if(eventlog.mf == 0)
			eventlog.mf = 8;
		eventlog.f = erealloc(eventlog.f, eventlog.mf*sizeof eventlog.f[0]);
	}
	eventlog.f[eventlog.nf++] = x->f;
	x->f->logoff = eventlog.start + eventlog.nev;

	qunlock(&eventlog.lk);
}

void
xfidlogclose(Xfid *x)
{
	int i;

	qlock(&eventlog.lk);
	for(i=0; i<eventlog.nf; i++) {
		if(eventlog.f[i] == x->f) {
			eventlog.f[i] = eventlog.f[--eventlog.nf];
			break;
		}
	}
	qunlock(&eventlog.lk);
}

void
xfidlogread(Xfid *x)
{
	char *p;
	int i;
	Fcall fc;

	qlock(&eventlog.lk);
	if(eventlog.nread >= eventlog.mread) {
		eventlog.mread = eventlog.mread*2;
		if(eventlog.mread == 0)
			eventlog.mread = 8;
		eventlog.read = erealloc(eventlog.read, eventlog.mread*sizeof eventlog.read[0]);
	}
	eventlog.read[eventlog.nread++] = x;

	if(eventlog.r.l == nil)
		eventlog.r.l = &eventlog.lk;
	x->flushed = FALSE;
	while(x->f->logoff >= eventlog.start+eventlog.nev && !x->flushed)
		rsleep(&eventlog.r);

	for(i=0; i<eventlog.nread; i++) {
		if(eventlog.read[i] == x) {
			eventlog.read[i] = eventlog.read[--eventlog.nread];
			break;
		}
	}

	if(x->flushed) {
		qunlock(&eventlog.lk);
		return;
	}

	i = x->f->logoff - eventlog.start;
	p = estrdup(eventlog.ev[i]);
	x->f->logoff++;
	qunlock(&eventlog.lk);

	fc.data = p;
	fc.count = strlen(p);
	respond(x, &fc, nil);
	free(p);
}

void
xfidlogflush(Xfid *x)
{
	int i;
	Xfid *rx;

	qlock(&eventlog.lk);
	for(i=0; i<eventlog.nread; i++) {
		rx = eventlog.read[i];
		if(rx->fcall.tag == x->fcall.oldtag) {
			rx->flushed = TRUE;
			rwakeupall(&eventlog.r);
		}
	}
	qunlock(&eventlog.lk);
}

/*
 * add a log entry for op on w.
 * expected calls:
 *
 * op == "new" for each new window
 *	- caller of coladd or makenewwindow responsible for calling
 *		xfidlog after setting window name
 *	- exception: zerox
 *
 * op == "zerox" for new window created via zerox
 *	- called from zeroxx
 *
 * op == "get" for Get executed on window
 *	- called from get
 *
 * op == "put" for Put executed on window
 *	- called from put
 *
 * op == "del" for deleted window
 *	- called from winclose
 */
void
xfidlog(Window *w, char *op)
{
	int i, n;
	vlong min;
	File *f;
	char *name;

	qlock(&eventlog.lk);
	if(eventlog.nev >= eventlog.mev) {
		// Remove and free any entries that all readers have read.
		min = eventlog.start + eventlog.nev;
		for(i=0; i<eventlog.nf; i++) {
			if(min > eventlog.f[i]->logoff)
				min = eventlog.f[i]->logoff;
		}
		if(min > eventlog.start) {
			n = min - eventlog.start;
			for(i=0; i<n; i++)
				free(eventlog.ev[i]);
			eventlog.nev -= n;
			eventlog.start += n;
			memmove(eventlog.ev, eventlog.ev+n, eventlog.nev*sizeof eventlog.ev[0]);
		}

		// Otherwise grow.
		if(eventlog.nev >= eventlog.mev) {
			eventlog.mev = eventlog.mev*2;
			if(eventlog.mev == 0)
				eventlog.mev = 8;
			eventlog.ev = erealloc(eventlog.ev, eventlog.mev*sizeof eventlog.ev[0]);
		}
	}
	f = w->body.file;
	name = runetobyte(f->name, f->nname);
	if(name == nil)
		name = estrdup("");
	eventlog.ev[eventlog.nev++] = smprint("%d %s %s\n", w->id, op, name);
	free(name);
	if(eventlog.r.l == nil)
		eventlog.r.l = &eventlog.lk;
	rwakeupall(&eventlog.r);
	qunlock(&eventlog.lk);
}
