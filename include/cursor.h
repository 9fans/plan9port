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

#if defined(__cplusplus)
}
#endif
#endif
