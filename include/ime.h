#ifndef _IME_H_
#define _IME_H_ 1
#if defined(__cplusplus)
extern "C" { 
#endif

struct Ime
{
	int x;
	int y;
};

extern	int	moveimespot(Point p);

#if defined(__cplusplus)
}
#endif
#endif
