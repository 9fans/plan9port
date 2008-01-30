/* Copyright (c) 1994-1996 David Hogan, see README for licence details */

#ifdef	DEBUG
#define	trace(s, c, e)	dotrace((s), (c), (e))
#else
#define	trace(s, c, e)
#endif

#define setstate setstaterio


/* color.c */
unsigned long colorpixel(Display*, ScreenInfo*, int, unsigned long, unsigned long);

/* main.c */
void	usage();
void	initscreen();
ScreenInfo *getscreen();
Time	timestamp();
void	sendcmessage();
void	sendconfig();
void	sighandler();
void	getevent();
void	cleanup();

/* event.c */
void	mainloop();
void	configurereq();
void	mapreq();
void	circulatereq();
void	unmap();
void	newwindow();
void	destroy();
void	clientmesg();
void	cmap();
void	property();
void	shapenotify();
void	enter();
void	leave();
void	focusin();
void	reparent();
void 	motionnotify();
BorderOrient borderorient();

/* manage.c */
int 	manage();
void	scanwins();
void	setshape();
void	withdraw();
void	gravitate();
void	cmapfocus();
void	cmapnofocus();
void	getcmaps();
int 	_getprop();
char	*getprop();
Window	getwprop();
int 	getiprop();
int 	getstate();
void	setstate();
void	setlabel();
void	getproto();
void	gettrans();

/* key.c */
void keypress();
void keyrelease();
void keysetup();

/* menu.c */
void	button();
void	spawn();
void	reshape();
void	move();
void	delete();
void	hide();
void	unhide();
void	unhidec();
void	renamec();
void	button2();
void	initb2menu();
void	switch_to();
void	switch_to_c();



/* client.c */
void	setactive();
void	draw_border();
void	active();
void	nofocus();
void	top();
Client	*getclient();
void	rmclient();
void	dump_revert();
void	dump_clients();
void shuffle(int);

/* grab.c */
int 	menuhit();
Client	*selectwin();
int 	sweep();
int 	drag();
int 	pull();
void	getmouse();
void	setmouse();

/* error.c */
int 	handler();
void	fatal();
void	graberror();
void	showhints();
void	dotrace();

/* cursor.c */
void	initcurs();

void ShowEvent(XEvent*);
