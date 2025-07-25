/* Copyright (c) 1994-1996 David Hogan, see README for licence details */

#ifdef	DEBUG
#define	trace(s, c, e)	dotrace((s), (c), (e))
#else
#define	trace(s, c, e)
#endif

#define setstate setstaterio
typedef struct Cursordata Cursordata;


/* color.c */
unsigned long colorpixel(Display*, ScreenInfo*, int, unsigned long, unsigned long);

/* main.c */
void	usage(void);
void	initscreen(ScreenInfo*, int, int);
ScreenInfo *getscreen(Window);
Time	timestamp(void);
void	sendcmessage(Window, Atom, long, int, int);
void	sendconfig(Client *);
void	sighandler(int);
void	getevent(XEvent *);
void	cleanup(void);

/* event.c */
void	mainloop(int);
void	configurereq(XConfigureRequestEvent*);
void	mapreq(XMapRequestEvent*);
void	circulatereq(XCirculateRequestEvent*);
void	unmap(XUnmapEvent*);
void	newwindow(XCreateWindowEvent*);
void	destroy(Window);
void	clientmesg(XClientMessageEvent*);
void	cmap(XColormapEvent*);
void	property(XPropertyEvent*);
void	shapenotify(XShapeEvent*);
void	enter(XCrossingEvent*);
void	leave(XCrossingEvent*);
void	focusin(XFocusChangeEvent*);
void	reparent(XReparentEvent*);
void 	motionnotify(XMotionEvent*);
BorderOrient borderorient(Client*, int, int);

/* manage.c */
int 	manage(Client*, int);
void	scanwins(ScreenInfo*);
void	setshape(Client*);
void	withdraw(Client*);
void	cmapfocus(Client*);
void	cmapnofocus(ScreenInfo*);
void	getcmaps(Client*);
int 	_getprop(Window, Atom, Atom, long, unsigned char **);
char	*getprop(Window, Atom);
Window	getwprop(Window, Atom);
int 	getiprop(Window, Atom);
int 	getstate(Window, int*);
void	setstate(Client*, int);
void	setlabel(Client*);
void	getproto(Client*);
void	gettrans(Client*);

/* key.c */
void keypress(XKeyEvent*);
void keyrelease(XKeyEvent*);
void keysetup(void);

/* menu.c */
void	button(XButtonEvent*);
void	spawn(ScreenInfo*);
void	reshape(Client*, int, int (*)(Client*, int, XButtonEvent*), XButtonEvent*);
void	move(Client*, int);
void	delete(Client*, int);
void	hide(Client*);
void	unhide(int, int);
void	unhidec(Client*, int);
void	renamec(Client*, char*);
void	button2(int);
void	initb2menu(int);
void	switch_to(int);
void	switch_to_c(int, Client*);



/* client.c */
void	setactive(Client*, int);
void	draw_border(Client*, int);
void	active(Client*);
void	nofocus(void);
void	top(Client*);
Client	*getclient(Window, int);
void	rmclient(Client*);
void	dump_revert(void);
void	dump_clients(void);
void	shuffle(int);

/* grab.c */
int 	menuhit(XButtonEvent*, Menu*);
Client	*selectwin(int, int*, ScreenInfo*);
int 	sweep(Client*, int, XButtonEvent*);
int 	drag(Client*, int);
int 	pull(Client*, int, XButtonEvent*);
void	getmouse(int*, int*, ScreenInfo*);
void	setmouse(int, int, ScreenInfo*);

/* error.c */
int 	handler(Display*, XErrorEvent*);
void	fatal(char*);
void	graberror(char*, int);
void	dotrace(char*, Client*, XEvent*);

/* cursor.c */
Cursor	getcursor(Cursordata*, ScreenInfo*);
void	initcurs(ScreenInfo*);

void ShowEvent(XEvent*);
