#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <cursor.h>
#include <keyboard.h>
#include <frame.h>
#include <plumb.h>
#include <termios.h>
#include <sys/termios.h>
#ifdef __linux__
#include <pty.h>
#endif

extern int getchildwd(int, char*, int);
extern int getpts(int[], char*);

