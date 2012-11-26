int _drawmsgread(void*, int);
int _drawmsgwrite(void*, int);
void _initdisplaymemimage(Memimage*);
int _latin1(Rune*, int);
int parsewinsize(char*, Rectangle*, int*);
int mouseswap(int);
void abortcompose(void);

extern int displaydpi;
extern int forcedpi;
