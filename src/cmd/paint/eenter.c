/*
This code was taken from 9front repository (https://code.9front.org/hg/plan9front).
It is subject to license from 9front, below is a reproduction of the license.

Copyright (c) 20XX 9front

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>

/* additional keyboard codes needed - defined here to avoid API change */
enum {
	Spec=   0xF800,
	Knack=  0x15,
	Ksoh=   0x01,
	Kenq=   0x05,
	Ketb=   0x17
};

int
eenter(char *ask, char *buf, int len, Mouse *m)
{
	int done, down, tick, n, h, w, l, i;
	Image *b, *save, *backcol, *bordcol;
	Point p, o, t;
	Rectangle r, sc;
	Event ev;
	Rune k;

	o = screen->r.min;
	backcol = allocimagemix(display, DPurpleblue, DWhite);
	bordcol = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DPurpleblue);
	if(backcol == nil || bordcol == nil)
		return -1;

	while(ecankbd())
		ekbd();

	if(m) o = m->xy;

	if(buf && len > 0)
		n = strlen(buf);
	else {
		buf = nil;
		len = 0;
		n = 0;
	}

	k = -1;
	tick = n;
	save = nil;
	done = down = 0;

	p = stringsize(font, " ");
	h = p.y;
	w = p.x;

	b = screen;
	sc = b->clipr;
	replclipr(b, 0, b->r);
	t = ZP;

	while(!done){
		p = stringsize(font, buf ? buf : "");
		if(ask && ask[0]){
			if(buf) p.x += w;
			p.x += stringwidth(font, ask);
		}
		r = rectaddpt(insetrect(Rpt(ZP, p), -4), o);
		p.x = 0;
		r = rectsubpt(r, p);

		p = ZP;
		if(r.min.x < screen->r.min.x)
			p.x = screen->r.min.x - r.min.x;
		if(r.min.y < screen->r.min.y)
			p.y = screen->r.min.y - r.min.y;
		r = rectaddpt(r, p);
		p = ZP;
		if(r.max.x > screen->r.max.x)
			p.x = r.max.x - screen->r.max.x;
		if(r.max.y > screen->r.max.y)
			p.y = r.max.y - screen->r.max.y;
		r = rectsubpt(r, p);

		r = insetrect(r, -2);
		if(save == nil){
			save = allocimage(display, r, b->chan, 0, DNofill);
			if(save == nil){
				n = -1;
				break;
			}
			draw(save, r, b, nil, r.min);
		}
		draw(b, r, backcol, nil, ZP);
		border(b, r, 2, bordcol, ZP);
		p = addpt(r.min, Pt(6, 6));
		if(ask && ask[0]){
			p = string(b, p, bordcol, ZP, font, ask);
			if(buf) p.x += w;
		}
		if(buf){
			t = p;
			p = stringn(b, p, display->black, ZP, font, buf, utfnlen(buf, tick));
			draw(b, Rect(p.x-1, p.y, p.x+2, p.y+3), display->black, nil, ZP);
			draw(b, Rect(p.x, p.y, p.x+1, p.y+h), display->black, nil, ZP);
			draw(b, Rect(p.x-1, p.y+h-3, p.x+2, p.y+h), display->black, nil, ZP);
			p = string(b, p, display->black, ZP, font, buf+tick);
		}
		flushimage(display, 1);

nodraw:
		i = Ekeyboard;
		if(m != nil)
			i |= Emouse;

		replclipr(b, 0, sc);
		i = eread(i, &ev);

		/* screen might have been resized */
		if(b != screen || !eqrect(screen->clipr, sc)){
			freeimage(save);
			save = nil;
		}
		b = screen;
		sc = b->clipr;
		replclipr(b, 0, b->r);

		switch(i){
		default:
			done = 1;
			n = -1;
			break;
		case Ekeyboard:
			k = ev.kbdc;
			if(buf == nil || k == Keof || k == '\n'){
				done = 1;
				break;
			}
			if(k == Knack || k == Kesc){
				done = !n;
				buf[n = tick = 0] = 0;
				break;
			}
			if(k == Ksoh || k == Khome){
				tick = 0;
				continue;
			}
			if(k == Kenq || k == Kend){
				tick = n;
				continue;
			}
			if(k == Kright){
				if(tick < n)
					tick += chartorune(&k, buf+tick);
				continue;
			}
			if(k == Kleft){
				for(i = 0; i < n; i += l){
					l = chartorune(&k, buf+tick);
					if(i+l >= tick){
						tick = i;
						break;
					}
				}
				continue;
			}
			if(k == Ketb){
				while(tick > 0){
					tick--;
					if(tick == 0 ||
					   strchr(" !\"#$%&'()*+,-./:;<=>?@`[\\]^{|}~", buf[tick-1]))
						break;
				}
				buf[n = tick] = 0;
				break;
			}
			if(k == Kbs){
				if(tick <= 0)
					continue;
				for(i = 0; i < n; i += l){
					l = chartorune(&k, buf+i);
					if(i+l >= tick){
						memmove(buf+i, buf+i+l, n - (i+l));
						buf[n -= l] = 0;
						tick -= l;
						break;
					}
				}
				break;
			}
			if(k < 0x20 || k == Kdel || (k & 0xFF00) == KF || (k & 0xFF00) == Spec)
				continue;
			if((len-n) <= (l = runelen(k)))
				continue;
			memmove(buf+tick+l, buf+tick, n - tick);
			runetochar(buf+tick, &k);
			buf[n += l] = 0;
			tick += l;
			break;
		case Emouse:
			*m = ev.mouse;
			if(!ptinrect(m->xy, r)){
				down = 0;
				goto nodraw;
			}
			if(m->buttons & 7){
				down = 1;
				if(buf && m->xy.x >= (t.x - w)){
					down = 0;
					for(i = 0; i < n; i += l){
						l = chartorune(&k, buf+i);
						t.x += stringnwidth(font, buf+i, 1);
						if(t.x > m->xy.x)
							break;
					}
					tick = i;
				}
				continue;
			}
			done = down;
			break;
		}
		if(save){
			draw(b, save->r, save, nil, save->r.min);
			freeimage(save);
			save = nil;
		}
	}

	replclipr(b, 0, sc);

	freeimage(backcol);
	freeimage(bordcol);
	flushimage(display, 1);

	return n;
}
