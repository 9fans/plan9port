#if 1
#define Display	XDisplay
#define Window	XWindow
#endif

int imeinit(Display *, Window);
void imecleanup();

Rune *imestr(XKeyPressedEvent *e);
int imesetspot(int x, int y);

#if 1
#undef Display
#undef Window
#endif
