#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0){
		fprint(2, "colors: can't reattach to window: %r\n");
		exits("resized");
	}
	draw(screen, screen->r, display->white, nil, ZP);
	flushimage(display, 1);
}

char *buttons[] =
{
	"exit",
	0
};

Menu menu =
{
	buttons
};

void
main(int argc, char *argv[])
{
	Mouse m;

	initdraw(0,0,0);
	eresized(0);
	einit(Emouse);
	for(;;){
		m = emouse();
		if(m.buttons == 4)
			switch(emenuhit(3, &m, &menu)){
			case 0:
				exits(0);
			}
	}
}
