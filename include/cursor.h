#ifndef _CURSOR_H_
#define _CURSOR_H_ 1
#if defined(__cplusplus)
extern "C" { 
#endif

typedef struct Cursor Cursor;
struct	Cursor
{
	Point	offset;
	uchar	clr[2*16];
	uchar	set[2*16];
};

typedef struct Cursor2 Cursor2;
struct	Cursor2
{
	Point	offset;
	uchar	clr[4*32];
	uchar	set[4*32];
};

void	scalecursor(Cursor2*, Cursor*);

#if defined(__cplusplus)
}
#endif
#endif
