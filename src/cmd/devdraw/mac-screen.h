#define setcursor dsetcursor

Memimage *attachscreen(Client*, char*, char*);
void	setmouse(Point);
void	setcursor(Cursor*, Cursor2*);
void	setlabel(char*);
char*	getsnarf(void);
void	putsnarf(char*);
void	topwin(void);

void	mousetrack(Client*, int, int, int, uint);
void	keystroke(Client*, int);
void	kicklabel(char*);

void	servep9p(Client*);

void resizeimg(Client*);

void resizewindow(Rectangle);
