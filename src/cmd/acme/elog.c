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
#include "edit.h"

static char Wsequence[] = "warning: changes out of sequence\n";
static int	warned = FALSE;

/*
 * Log of changes made by editing commands.  Three reasons for this:
 * 1) We want addresses in commands to apply to old file, not file-in-change.
 * 2) It's difficult to track changes correctly as things move, e.g. ,x m$
 * 3) This gives an opportunity to optimize by merging adjacent changes.
 * It's a little bit like the Undo/Redo log in Files, but Point 3) argues for a
 * separate implementation.  To do this well, we use Replace as well as
 * Insert and Delete
 */

typedef struct Buflog Buflog;
struct Buflog
{
	short	type;		/* Replace, Filename */
	uint		q0;		/* location of change (unused in f) */
	uint		nd;		/* # runes to delete */
	uint		nr;		/* # runes in string or file name */
};

enum
{
	Buflogsize = sizeof(Buflog)/sizeof(Rune)
};

/*
 * Minstring shouldn't be very big or we will do lots of I/O for small changes.
 * Maxstring is RBUFSIZE so we can fbufalloc() once and not realloc elog.r.
 */
enum
{
	Minstring = 16,		/* distance beneath which we merge changes */
	Maxstring = RBUFSIZE	/* maximum length of change we will merge into one */
};

void
eloginit(File *f)
{
	if(f->elog.type != Empty)
		return;
	f->elog.type = Null;
	if(f->elogbuf == nil)
		f->elogbuf = emalloc(sizeof(Buffer));
	if(f->elog.r == nil)
		f->elog.r = fbufalloc();
	bufreset(f->elogbuf);
}

void
elogclose(File *f)
{
	if(f->elogbuf){
		bufclose(f->elogbuf);
		free(f->elogbuf);
		f->elogbuf = nil;
	}
}

void
elogreset(File *f)
{
	f->elog.type = Null;
	f->elog.nd = 0;
	f->elog.nr = 0;
}

void
elogterm(File *f)
{
	elogreset(f);
	if(f->elogbuf)
		bufreset(f->elogbuf);
	f->elog.type = Empty;
	fbuffree(f->elog.r);
	f->elog.r = nil;
	warned = FALSE;
}

void
elogflush(File *f)
{
	Buflog b;

	b.type = f->elog.type;
	b.q0 = f->elog.q0;
	b.nd = f->elog.nd;
	b.nr = f->elog.nr;
	switch(f->elog.type){
	default:
		warning(nil, "unknown elog type 0x%ux\n", f->elog.type);
		break;
	case Null:
		break;
	case Insert:
	case Replace:
		if(f->elog.nr > 0)
			bufinsert(f->elogbuf, f->elogbuf->nc, f->elog.r, f->elog.nr);
		/* fall through */
	case Delete:
		bufinsert(f->elogbuf, f->elogbuf->nc, (Rune*)&b, Buflogsize);
		break;
	}
	elogreset(f);
}

void
elogreplace(File *f, int q0, int q1, Rune *r, int nr)
{
	uint gap;

	if(q0==q1 && nr==0)
		return;
	eloginit(f);
	if(f->elog.type!=Null && q0<f->elog.q0){
		if(warned++ == 0)
			warning(nil, Wsequence);
		elogflush(f);
	}
	/* try to merge with previous */
	gap = q0 - (f->elog.q0+f->elog.nd);	/* gap between previous and this */
	if(f->elog.type==Replace && f->elog.nr+gap+nr<Maxstring){
		if(gap < Minstring){
			if(gap > 0){
				bufread(&f->b, f->elog.q0+f->elog.nd, f->elog.r+f->elog.nr, gap);
				f->elog.nr += gap;
			}
			f->elog.nd += gap + q1-q0;
			runemove(f->elog.r+f->elog.nr, r, nr);
			f->elog.nr += nr;
			return;
		}
	}
	elogflush(f);
	f->elog.type = Replace;
	f->elog.q0 = q0;
	f->elog.nd = q1-q0;
	f->elog.nr = nr;
	if(nr > RBUFSIZE)
		editerror("internal error: replacement string too large(%d)", nr);
	runemove(f->elog.r, r, nr);
}

void
eloginsert(File *f, int q0, Rune *r, int nr)
{
	int n;

	if(nr == 0)
		return;
	eloginit(f);
	if(f->elog.type!=Null && q0<f->elog.q0){
		if(warned++ == 0)
			warning(nil, Wsequence);
		elogflush(f);
	}
	/* try to merge with previous */
	if(f->elog.type==Insert && q0==f->elog.q0 && f->elog.nr+nr<Maxstring){
		runemove(f->elog.r+f->elog.nr, r, nr);
		f->elog.nr += nr;
		return;
	}
	while(nr > 0){
		elogflush(f);
		f->elog.type = Insert;
		f->elog.q0 = q0;
		n = nr;
		if(n > RBUFSIZE)
			n = RBUFSIZE;
		f->elog.nr = n;
		runemove(f->elog.r, r, n);
		r += n;
		nr -= n;
	}
}

void
elogdelete(File *f, int q0, int q1)
{
	if(q0 == q1)
		return;
	eloginit(f);
	if(f->elog.type!=Null && q0<f->elog.q0+f->elog.nd){
		if(warned++ == 0)
			warning(nil, Wsequence);
		elogflush(f);
	}
	/* try to merge with previous */
	if(f->elog.type==Delete && f->elog.q0+f->elog.nd==q0){
		f->elog.nd += q1-q0;
		return;
	}
	elogflush(f);
	f->elog.type = Delete;
	f->elog.q0 = q0;
	f->elog.nd = q1-q0;
}

#define tracelog 0
void
elogapply(File *f)
{
	Buflog b;
	Rune *buf;
	uint i, n, up, mod;
	uint tq0, tq1;
	Buffer *log;
	Text *t;
	int owner;

	elogflush(f);
	log = f->elogbuf;
	t = f->curtext;

	buf = fbufalloc();
	mod = FALSE;

	owner = 0;
	if(t->w){
		owner = t->w->owner;
		if(owner == 0)
			t->w->owner = 'E';
	}

	/*
	 * The edit commands have already updated the selection in t->q0, t->q1,
	 * but using coordinates relative to the unmodified buffer.  As we apply the log,
	 * we have to update the coordinates to be relative to the modified buffer.
	 * Textinsert and textdelete will do this for us; our only work is to apply the
	 * convention that an insertion at t->q0==t->q1 is intended to select the
	 * inserted text.
	 */

	/*
	 * We constrain the addresses in here (with textconstrain()) because
	 * overlapping changes will generate bogus addresses.   We will warn
	 * about changes out of sequence but proceed anyway; here we must
	 * keep things in range.
	 */

	while(log->nc > 0){
		up = log->nc-Buflogsize;
		bufread(log, up, (Rune*)&b, Buflogsize);
		switch(b.type){
		default:
			fprint(2, "elogapply: 0x%ux\n", b.type);
			abort();
			break;

		case Replace:
			if(tracelog)
				warning(nil, "elog replace %d %d (%d %d)\n",
					b.q0, b.q0+b.nd, t->q0, t->q1);
			if(!mod){
				mod = TRUE;
				filemark(f);
			}
			textconstrain(t, b.q0, b.q0+b.nd, &tq0, &tq1);
			textdelete(t, tq0, tq1, TRUE);
			up -= b.nr;
			for(i=0; i<b.nr; i+=n){
				n = b.nr - i;
				if(n > RBUFSIZE)
					n = RBUFSIZE;
				bufread(log, up+i, buf, n);
				textinsert(t, tq0+i, buf, n, TRUE);
			}
			if(t->q0 == b.q0 && t->q1 == b.q0)
				t->q1 += b.nr;
			break;

		case Delete:
			if(tracelog)
				warning(nil, "elog delete %d %d (%d %d)\n",
					b.q0, b.q0+b.nd, t->q0, t->q1);
			if(!mod){
				mod = TRUE;
				filemark(f);
			}
			textconstrain(t, b.q0, b.q0+b.nd, &tq0, &tq1);
			textdelete(t, tq0, tq1, TRUE);
			break;

		case Insert:
			if(tracelog)
				warning(nil, "elog insert %d %d (%d %d)\n",
					b.q0, b.q0+b.nr, t->q0, t->q1);
			if(!mod){
				mod = TRUE;
				filemark(f);
			}
			textconstrain(t, b.q0, b.q0, &tq0, &tq1);
			up -= b.nr;
			for(i=0; i<b.nr; i+=n){
				n = b.nr - i;
				if(n > RBUFSIZE)
					n = RBUFSIZE;
				bufread(log, up+i, buf, n);
				textinsert(t, tq0+i, buf, n, TRUE);
			}
			if(t->q0 == b.q0 && t->q1 == b.q0)
				t->q1 += b.nr;
			break;

/*		case Filename:
			f->seq = u.seq;
			fileunsetname(f, epsilon);
			f->mod = u.mod;
			up -= u.n;
			free(f->name);
			if(u.n == 0)
				f->name = nil;
			else
				f->name = runemalloc(u.n);
			bufread(delta, up, f->name, u.n);
			f->nname = u.n;
			break;
*/
		}
		bufdelete(log, up, log->nc);
	}
	fbuffree(buf);
	elogterm(f);

	/*
	 * Bad addresses will cause bufload to crash, so double check.
	 * If changes were out of order, we expect problems so don't complain further.
	 */
	if(t->q0 > f->b.nc || t->q1 > f->b.nc || t->q0 > t->q1){
		if(!warned)
			warning(nil, "elogapply: can't happen %d %d %d\n", t->q0, t->q1, f->b.nc);
		t->q1 = min(t->q1, f->b.nc);
		t->q0 = min(t->q0, t->q1);
	}

	if(t->w)
		t->w->owner = owner;
}
