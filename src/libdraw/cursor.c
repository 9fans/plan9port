#include <u.h>
#include <libc.h>
#include <draw.h>
#include <cursor.h>

static uint8 expand[16] = {
	0x00, 0x03, 0x0c, 0x0f,
	0x30, 0x33, 0x3c, 0x3f,
	0xc0, 0xc3, 0xcc, 0xcf,
	0xf0, 0xf3, 0xfc, 0xff,
};

void
scalecursor(Cursor2 *c2, Cursor *c)
{
	int y;

	c2->offset.x = 2*c->offset.x;
	c2->offset.y = 2*c->offset.y;
	memset(c2->clr, 0, sizeof c2->clr);
	memset(c2->set, 0, sizeof c2->set);
	for(y = 0; y < 16; y++) {
		c2->clr[8*y] = c2->clr[8*y+4] = expand[c->clr[2*y]>>4];
		c2->set[8*y] = c2->set[8*y+4] = expand[c->set[2*y]>>4];
		c2->clr[8*y+1] = c2->clr[8*y+5] = expand[c->clr[2*y]&15];
		c2->set[8*y+1] = c2->set[8*y+5] = expand[c->set[2*y]&15];
		c2->clr[8*y+2] = c2->clr[8*y+6] = expand[c->clr[2*y+1]>>4];
		c2->set[8*y+2] = c2->set[8*y+6] = expand[c->set[2*y+1]>>4];
		c2->clr[8*y+3] = c2->clr[8*y+7] = expand[c->clr[2*y+1]&15];
		c2->set[8*y+3] = c2->set[8*y+7] = expand[c->set[2*y+1]&15];
	}
}
