#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <bio.h>
#include <plumb.h>
#include <libsec.h>
#include "dat.h"
#include "fns.h"

static Rune Lcolhdr[] = {
	'N', 'e', 'w', 'c', 'o', 'l', ' ',
	'K', 'i', 'l', 'l', ' ',
	'P', 'u', 't', 'a', 'l', 'l', ' ',
	'D', 'u', 'm', 'p', ' ',
	'E', 'x', 'i', 't', ' ',
	0
};

void
rowinit(Row *row, Rectangle r)
{
	Rectangle r1;
	Text *t;

	draw(screen, r, display->white, nil, ZP);
	row->r = r;
	row->col = nil;
	row->ncol = 0;
	r1 = r;
	r1.max.y = r1.min.y + font->height;
	t = &row->tag;
	textinit(t, fileaddtext(nil, t), r1, rfget(FALSE, FALSE, FALSE, nil), tagcols);
	t->what = Rowtag;
	t->row = row;
	t->w = nil;
	t->col = nil;
	r1.min.y = r1.max.y;
	r1.max.y += Border;
	draw(screen, r1, display->black, nil, ZP);
	textinsert(t, 0, Lcolhdr, 29, TRUE);
	textsetselect(t, t->file->b.nc, t->file->b.nc);
}

Column*
rowadd(Row *row, Column *c, int x)
{
	Rectangle r, r1;
	Column *d;
	int i;

	d = nil;
	r = row->r;
	r.min.y = row->tag.fr.r.max.y+Border;
	if(x<r.min.x && row->ncol>0){	/*steal 40% of last column by default */
		d = row->col[row->ncol-1];
		x = d->r.min.x + 3*Dx(d->r)/5;
	}
	/* look for column we'll land on */
	for(i=0; i<row->ncol; i++){
		d = row->col[i];
		if(x < d->r.max.x)
			break;
	}
	if(row->ncol > 0){
		if(i < row->ncol)
			i++;	/* new column will go after d */
		r = d->r;
		if(Dx(r) < 100)
			return nil;
		draw(screen, r, display->white, nil, ZP);
		r1 = r;
		r1.max.x = min(x-Border, r.max.x-50);
		if(Dx(r1) < 50)
			r1.max.x = r1.min.x+50;
		colresize(d, r1);
		r1.min.x = r1.max.x;
		r1.max.x = r1.min.x+Border;
		draw(screen, r1, display->black, nil, ZP);
		r.min.x = r1.max.x;
	}
	if(c == nil){
		c = emalloc(sizeof(Column));
		colinit(c, r);
		incref(&reffont.ref);
	}else
		colresize(c, r);
	c->row = row;
	c->tag.row = row;
	row->col = realloc(row->col, (row->ncol+1)*sizeof(Column*));
	memmove(row->col+i+1, row->col+i, (row->ncol-i)*sizeof(Column*));
	row->col[i] = c;
	row->ncol++;
	clearmouse();
	return c;
}

void
rowresize(Row *row, Rectangle r)
{
	int i, deltax;
	Rectangle or, r1, r2;
	Column *c;

	or = row->r;
	deltax = r.min.x - or.min.x;
	row->r = r;
	r1 = r;
	r1.max.y = r1.min.y + font->height;
	textresize(&row->tag, r1, TRUE);
	r1.min.y = r1.max.y;
	r1.max.y += Border;
	draw(screen, r1, display->black, nil, ZP);
	r.min.y = r1.max.y;
	r1 = r;
	r1.max.x = r1.min.x;
	for(i=0; i<row->ncol; i++){
		c = row->col[i];
		r1.min.x = r1.max.x;
		/* the test should not be necessary, but guarantee we don't lose a pixel */
		if(i == row->ncol-1)
			r1.max.x = r.max.x;
		else
			r1.max.x = (c->r.max.x-or.min.x)*Dx(r)/Dx(or) + deltax;
		if(i > 0){
			r2 = r1;
			r2.max.x = r2.min.x+Border;
			draw(screen, r2, display->black, nil, ZP);
			r1.min.x = r2.max.x;
		}
		colresize(c, r1);
	}
}

void
rowdragcol(Row *row, Column *c, int _0)
{
	Rectangle r;
	int i, b, x;
	Point p, op;
	Column *d;

	USED(_0);

	clearmouse();
	setcursor2(mousectl, &boxcursor, &boxcursor2);
	b = mouse->buttons;
	op = mouse->xy;
	while(mouse->buttons == b)
		readmouse(mousectl);
	setcursor(mousectl, nil);
	if(mouse->buttons){
		while(mouse->buttons)
			readmouse(mousectl);
		return;
	}

	for(i=0; i<row->ncol; i++)
		if(row->col[i] == c)
			goto Found;
	error("can't find column");

  Found:
	p = mouse->xy;
	if((abs(p.x-op.x)<5 && abs(p.y-op.y)<5))
		return;
	if((i>0 && p.x<row->col[i-1]->r.min.x) || (i<row->ncol-1 && p.x>c->r.max.x)){
		/* shuffle */
		x = c->r.min.x;
		rowclose(row, c, FALSE);
		if(rowadd(row, c, p.x) == nil)	/* whoops! */
		if(rowadd(row, c, x) == nil)		/* WHOOPS! */
		if(rowadd(row, c, -1)==nil){		/* shit! */
			rowclose(row, c, TRUE);
			return;
		}
		colmousebut(c);
		return;
	}
	if(i == 0)
		return;
	d = row->col[i-1];
	if(p.x < d->r.min.x+80+Scrollwid)
		p.x = d->r.min.x+80+Scrollwid;
	if(p.x > c->r.max.x-80-Scrollwid)
		p.x = c->r.max.x-80-Scrollwid;
	r = d->r;
	r.max.x = c->r.max.x;
	draw(screen, r, display->white, nil, ZP);
	r.max.x = p.x;
	colresize(d, r);
	r = c->r;
	r.min.x = p.x;
	r.max.x = r.min.x;
	r.max.x += Border;
	draw(screen, r, display->black, nil, ZP);
	r.min.x = r.max.x;
	r.max.x = c->r.max.x;
	colresize(c, r);
	colmousebut(c);
}

void
rowclose(Row *row, Column *c, int dofree)
{
	Rectangle r;
	int i;

	for(i=0; i<row->ncol; i++)
		if(row->col[i] == c)
			goto Found;
	error("can't find column");
  Found:
	r = c->r;
	if(dofree)
		colcloseall(c);
	row->ncol--;
	memmove(row->col+i, row->col+i+1, (row->ncol-i)*sizeof(Column*));
	row->col = realloc(row->col, row->ncol*sizeof(Column*));
	if(row->ncol == 0){
		draw(screen, r, display->white, nil, ZP);
		return;
	}
	if(i == row->ncol){		/* extend last column right */
		c = row->col[i-1];
		r.min.x = c->r.min.x;
		r.max.x = row->r.max.x;
	}else{			/* extend next window left */
		c = row->col[i];
		r.max.x = c->r.max.x;
	}
	draw(screen, r, display->white, nil, ZP);
	colresize(c, r);
}

Column*
rowwhichcol(Row *row, Point p)
{
	int i;
	Column *c;

	for(i=0; i<row->ncol; i++){
		c = row->col[i];
		if(ptinrect(p, c->r))
			return c;
	}
	return nil;
}

Text*
rowwhich(Row *row, Point p)
{
	Column *c;

	if(ptinrect(p, row->tag.all))
		return &row->tag;
	c = rowwhichcol(row, p);
	if(c)
		return colwhich(c, p);
	return nil;
}

Text*
rowtype(Row *row, Rune r, Point p)
{
	Window *w;
	Text *t;

	if(r == 0)
		r = Runeerror;

	clearmouse();
	qlock(&row->lk);
	if(bartflag)
		t = barttext;
	else
		t = rowwhich(row, p);
	if(t!=nil && !(t->what==Tag && ptinrect(p, t->scrollr))){
		w = t->w;
		if(w == nil)
			texttype(t, r);
		else{
			winlock(w, 'K');
			wintype(w, t, r);
			/* Expand tag if necessary */
			if(t->what == Tag){
				t->w->tagsafe = FALSE;
				if(r == '\n')
					t->w->tagexpand = TRUE;
				winresize(w, w->r, TRUE, TRUE);
			}
			winunlock(w);
		}
	}
	qunlock(&row->lk);
	return t;
}

int
rowclean(Row *row)
{
	int clean;
	int i;

	clean = TRUE;
	for(i=0; i<row->ncol; i++)
		clean &= colclean(row->col[i]);
	return clean;
}

void
rowdump(Row *row, char *file)
{
	int i, j, fd, m, n, start, dumped;
	uint q0, q1;
	Biobuf *b;
	char *buf, *a, *fontname;
	Rune *r;
	Column *c;
	Window *w, *w1;
	Text *t;

	if(row->ncol == 0)
		return;
	buf = fbufalloc();
	if(file == nil){
		if(home == nil){
			warning(nil, "can't find file for dump: $home not defined\n");
			goto Rescue;
		}
		sprint(buf, "%s/acme.dump", home);
		file = buf;
	}
	fd = create(file, OWRITE, 0600);
	if(fd < 0){
		warning(nil, "can't open %s: %r\n", file);
		goto Rescue;
	}
	b = emalloc(sizeof(Biobuf));
	Binit(b, fd, OWRITE);
	r = fbufalloc();
	Bprint(b, "%s\n", wdir);
	Bprint(b, "%s\n", fontnames[0]);
	Bprint(b, "%s\n", fontnames[1]);
	for(i=0; i<row->ncol; i++){
		c = row->col[i];
		Bprint(b, "%11.7f", 100.0*(c->r.min.x-row->r.min.x)/Dx(row->r));
		if(i == row->ncol-1)
			Bputc(b, '\n');
		else
			Bputc(b, ' ');
	}
	for(i=0; i<row->ncol; i++){
		c = row->col[i];
		for(j=0; j<c->nw; j++)
			c->w[j]->body.file->dumpid = 0;
	}
	m = min(RBUFSIZE, row->tag.file->b.nc);
	bufread(&row->tag.file->b, 0, r, m);
	n = 0;
	while(n<m && r[n]!='\n')
		n++;
	Bprint(b, "w %.*S\n", n, r);
	for(i=0; i<row->ncol; i++){
		c = row->col[i];
		m = min(RBUFSIZE, c->tag.file->b.nc);
		bufread(&c->tag.file->b, 0, r, m);
		n = 0;
		while(n<m && r[n]!='\n')
			n++;
		Bprint(b, "c%11d %.*S\n", i, n, r);
	}
	for(i=0; i<row->ncol; i++){
		c = row->col[i];
		for(j=0; j<c->nw; j++){
			w = c->w[j];
			wincommit(w, &w->tag);
			t = &w->body;
			/* windows owned by others get special treatment */
			if(w->nopen[QWevent] > 0)
				if(w->dumpstr == nil)
					continue;
			/* zeroxes of external windows are tossed */
			if(t->file->ntext > 1)
				for(n=0; n<t->file->ntext; n++){
					w1 = t->file->text[n]->w;
					if(w == w1)
						continue;
					if(w1->nopen[QWevent])
						goto Continue2;
				}
			fontname = "";
			if(t->reffont->f != font)
				fontname = t->reffont->f->name;
			if(t->file->nname)
				a = runetobyte(t->file->name, t->file->nname);
			else
				a = emalloc(1);
			if(t->file->dumpid){
				dumped = FALSE;
				Bprint(b, "x%11d %11d %11d %11d %11.7f %s\n", i, t->file->dumpid,
					w->body.q0, w->body.q1,
					100.0*(w->r.min.y-c->r.min.y)/Dy(c->r),
					fontname);
			}else if(w->dumpstr){
				dumped = FALSE;
				Bprint(b, "e%11d %11d %11d %11d %11.7f %s\n", i, t->file->dumpid,
					0, 0,
					100.0*(w->r.min.y-c->r.min.y)/Dy(c->r),
					fontname);
			}else if((w->dirty==FALSE && access(a, 0)==0) || w->isdir){
				dumped = FALSE;
				t->file->dumpid = w->id;
				Bprint(b, "f%11d %11d %11d %11d %11.7f %s\n", i, w->id,
					w->body.q0, w->body.q1,
					100.0*(w->r.min.y-c->r.min.y)/Dy(c->r),
					fontname);
			}else{
				dumped = TRUE;
				t->file->dumpid = w->id;
				Bprint(b, "F%11d %11d %11d %11d %11.7f %11d %s\n", i, j,
					w->body.q0, w->body.q1,
					100.0*(w->r.min.y-c->r.min.y)/Dy(c->r),
					w->body.file->b.nc, fontname);
			}
			free(a);
			winctlprint(w, buf, 0);
			Bwrite(b, buf, strlen(buf));
			m = min(RBUFSIZE, w->tag.file->b.nc);
			bufread(&w->tag.file->b, 0, r, m);
			n = 0;
			while(n<m) {
				start = n;
				while(n<m && r[n]!='\n')
					n++;
				Bprint(b, "%.*S", n-start, r+start);
				if(n<m) {
					Bputc(b, 0xff); // \n in tag becomes 0xff byte (invalid UTF)
					n++;
				}
			}
			Bprint(b, "\n");
			if(dumped){
				q0 = 0;
				q1 = t->file->b.nc;
				while(q0 < q1){
					n = q1 - q0;
					if(n > BUFSIZE/UTFmax)
						n = BUFSIZE/UTFmax;
					bufread(&t->file->b, q0, r, n);
					Bprint(b, "%.*S", n, r);
					q0 += n;
				}
			}
			if(w->dumpstr){
				if(w->dumpdir)
					Bprint(b, "%s\n%s\n", w->dumpdir, w->dumpstr);
				else
					Bprint(b, "\n%s\n", w->dumpstr);
			}
    Continue2:;
		}
	}
	Bterm(b);
	close(fd);
	free(b);
	fbuffree(r);

   Rescue:
	fbuffree(buf);
}

static
char*
rdline(Biobuf *b, int *linep)
{
	char *l;

	l = Brdline(b, '\n');
	if(l)
		(*linep)++;
	return l;
}

/*
 * Get font names from load file so we don't load fonts we won't use
 */
void
rowloadfonts(char *file)
{
	int i;
	Biobuf *b;
	char *l;

	b = Bopen(file, OREAD);
	if(b == nil)
		return;
	/* current directory */
	l = Brdline(b, '\n');
	if(l == nil)
		goto Return;
	/* global fonts */
	for(i=0; i<2; i++){
		l = Brdline(b, '\n');
		if(l == nil)
			goto Return;
		l[Blinelen(b)-1] = 0;
		if(*l && strcmp(l, fontnames[i])!=0){
			free(fontnames[i]);
			fontnames[i] = estrdup(l);
		}
	}
    Return:
	Bterm(b);
}

int
rowload(Row *row, char *file, int initing)
{
	int i, j, line, y, nr, nfontr, n, ns, ndumped, dumpid, x, fd, done;
	double percent;
	Biobuf *b, *bout;
	char *buf, *l, *t, *fontname;
	Rune *r, *fontr;
	int rune;
	Column *c, *c1, *c2;
	uint q0, q1;
	Rectangle r1, r2;
	Window *w;

	buf = fbufalloc();
	if(file == nil){
		if(home == nil){
			warning(nil, "can't find file for load: $home not defined\n");
			goto Rescue1;
		}
		sprint(buf, "%s/acme.dump", home);
		file = buf;
	}
	b = Bopen(file, OREAD);
	if(b == nil){
		warning(nil, "can't open load file %s: %r\n", file);
		goto Rescue1;
	}
	/* current directory */
	line = 0;
	l = rdline(b, &line);
	if(l == nil)
		goto Rescue2;
	l[Blinelen(b)-1] = 0;
	if(chdir(l) < 0){
		warning(nil, "can't chdir %s\n", l);
		goto Rescue2;
	}
	/* global fonts */
	for(i=0; i<2; i++){
		l = rdline(b, &line);
		if(l == nil)
			goto Rescue2;
		l[Blinelen(b)-1] = 0;
		if(*l && strcmp(l, fontnames[i])!=0)
			rfget(i, TRUE, i==0 && initing, l);
	}
	if(initing && row->ncol==0)
		rowinit(row, screen->clipr);
	l = rdline(b, &line);
	if(l == nil)
		goto Rescue2;
	j = Blinelen(b)/12;
	if(j<=0 || j>10)
		goto Rescue2;
	for(i=0; i<j; i++){
		percent = atof(l+i*12);
		if(percent<0 || percent>=100)
			goto Rescue2;
		x = row->r.min.x+percent*Dx(row->r)/100+0.5;
		if(i < row->ncol){
			if(i == 0)
				continue;
			c1 = row->col[i-1];
			c2 = row->col[i];
			r1 = c1->r;
			r2 = c2->r;
			if(x<Border)
				x = Border;
			r1.max.x = x-Border;
			r2.min.x = x;
			if(Dx(r1) < 50 || Dx(r2) < 50)
				continue;
			draw(screen, Rpt(r1.min, r2.max), display->white, nil, ZP);
			colresize(c1, r1);
			colresize(c2, r2);
			r2.min.x = x-Border;
			r2.max.x = x;
			draw(screen, r2, display->black, nil, ZP);
		}
		if(i >= row->ncol)
			rowadd(row, nil, x);
	}
	done = 0;
	while(!done){
		l = rdline(b, &line);
		if(l == nil)
			break;
		switch(l[0]){
		case 'c':
			l[Blinelen(b)-1] = 0;
			i = atoi(l+1+0*12);
			r = bytetorune(l+1*12, &nr);
			ns = -1;
			for(n=0; n<nr; n++){
				if(r[n] == '/')
					ns = n;
				if(r[n] == ' ')
					break;
			}
			textdelete(&row->col[i]->tag, 0, row->col[i]->tag.file->b.nc, TRUE);
			textinsert(&row->col[i]->tag, 0, r+n+1, nr-(n+1), TRUE);
			free(r);
			break;
		case 'w':
			l[Blinelen(b)-1] = 0;
			r = bytetorune(l+2, &nr);
			ns = -1;
			for(n=0; n<nr; n++){
				if(r[n] == '/')
					ns = n;
				if(r[n] == ' ')
					break;
			}
			textdelete(&row->tag, 0, row->tag.file->b.nc, TRUE);
			textinsert(&row->tag, 0, r, nr, TRUE);
			free(r);
			break;
		default:
			done = 1;
			break;
		}
	}
	for(;;){
		if(l == nil)
			break;
		dumpid = 0;
		switch(l[0]){
		case 'e':
			if(Blinelen(b) < 1+5*12+1)
				goto Rescue2;
			l = rdline(b, &line);	/* ctl line; ignored */
			if(l == nil)
				goto Rescue2;
			l = rdline(b, &line);	/* directory */
			if(l == nil)
				goto Rescue2;
			l[Blinelen(b)-1] = 0;
			if(*l == '\0'){
				if(home == nil)
					r = bytetorune("./", &nr);
				else{
					t = emalloc(strlen(home)+1+1);
					sprint(t, "%s/", home);
					r = bytetorune(t, &nr);
					free(t);
				}
			}else
				r = bytetorune(l, &nr);
			l = rdline(b, &line);	/* command */
			if(l == nil)
				goto Rescue2;
			t = emalloc(Blinelen(b)+1);
			memmove(t, l, Blinelen(b));
			run(nil, t, r, nr, TRUE, nil, nil, FALSE);
			/* r is freed in run() */
			goto Nextline;
		case 'f':
			if(Blinelen(b) < 1+5*12+1)
				goto Rescue2;
			fontname = l+1+5*12;
			ndumped = -1;
			break;
		case 'F':
			if(Blinelen(b) < 1+6*12+1)
				goto Rescue2;
			fontname = l+1+6*12;
			ndumped = atoi(l+1+5*12+1);
			break;
		case 'x':
			if(Blinelen(b) < 1+5*12+1)
				goto Rescue2;
			fontname = l+1+5*12;
			ndumped = -1;
			dumpid = atoi(l+1+1*12);
			break;
		default:
			goto Rescue2;
		}
		l[Blinelen(b)-1] = 0;
		fontr = nil;
		nfontr = 0;
		if(*fontname)
			fontr = bytetorune(fontname, &nfontr);
		i = atoi(l+1+0*12);
		j = atoi(l+1+1*12);
		q0 = atoi(l+1+2*12);
		q1 = atoi(l+1+3*12);
		percent = atof(l+1+4*12);
		if(i<0 || i>10)
			goto Rescue2;
		if(i > row->ncol)
			i = row->ncol;
		c = row->col[i];
		y = c->r.min.y+(percent*Dy(c->r))/100+0.5;
		if(y<c->r.min.y || y>=c->r.max.y)
			y = -1;
		if(dumpid == 0)
			w = coladd(c, nil, nil, y);
		else
			w = coladd(c, nil, lookid(dumpid, TRUE), y);
		if(w == nil)
			goto Nextline;
		w->dumpid = j;
		l = rdline(b, &line);
		if(l == nil)
			goto Rescue2;
		l[Blinelen(b)-1] = 0;
		/* convert 0xff in multiline tag back to \n */
		for(i = 0; l[i] != 0; i++)
			if((uchar)l[i] == 0xff)
				l[i] = '\n';
		r = bytetorune(l+5*12, &nr);
		ns = -1;
		for(n=0; n<nr; n++){
			if(r[n] == '/')
				ns = n;
			if(r[n] == ' ')
				break;
		}
		if(dumpid == 0)
			winsetname(w, r, n);
		for(; n<nr; n++)
			if(r[n] == '|')
				break;
		wincleartag(w);
		textinsert(&w->tag, w->tag.file->b.nc, r+n+1, nr-(n+1), TRUE);
		if(ndumped >= 0){
			/* simplest thing is to put it in a file and load that */
			sprint(buf, "/tmp/d%d.%.4sacme", getpid(), getuser());
			fd = create(buf, OWRITE, 0600);
			if(fd < 0){
				free(r);
				warning(nil, "can't create temp file: %r\n");
				goto Rescue2;
			}
			bout = emalloc(sizeof(Biobuf));
			Binit(bout, fd, OWRITE);
			for(n=0; n<ndumped; n++){
				rune = Bgetrune(b);
				if(rune == '\n')
					line++;
				if(rune == Beof){
					free(r);
					Bterm(bout);
					free(bout);
					close(fd);
					remove(buf);
					goto Rescue2;
				}
				Bputrune(bout, rune);
			}
			Bterm(bout);
			free(bout);
			textload(&w->body, 0, buf, 1);
			remove(buf);
			close(fd);
			w->body.file->mod = TRUE;
			for(n=0; n<w->body.file->ntext; n++)
				w->body.file->text[n]->w->dirty = TRUE;
			winsettag(w);
		}else if(dumpid==0 && r[ns+1]!='+' && r[ns+1]!='-')
			get(&w->body, nil, nil, FALSE, XXX, nil, 0);
		if(fontr){
			fontx(&w->body, nil, nil, 0, 0, fontr, nfontr);
			free(fontr);
		}
		free(r);
		if(q0>w->body.file->b.nc || q1>w->body.file->b.nc || q0>q1)
			q0 = q1 = 0;
		textshow(&w->body, q0, q1, 1);
		w->maxlines = min(w->body.fr.nlines, max(w->maxlines, w->body.fr.maxlines));
		xfidlog(w, "new");
Nextline:
		l = rdline(b, &line);
	}
	Bterm(b);
	fbuffree(buf);
	return TRUE;

Rescue2:
	warning(nil, "bad load file %s:%d\n", file, line);
	Bterm(b);
Rescue1:
	fbuffree(buf);
	return FALSE;
}

void
allwindows(void (*f)(Window*, void*), void *arg)
{
	int i, j;
	Column *c;

	for(i=0; i<row.ncol; i++){
		c = row.col[i];
		for(j=0; j<c->nw; j++)
			(*f)(c->w[j], arg);
	}
}
