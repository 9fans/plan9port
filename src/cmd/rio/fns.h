/* Copyright (c) 1994-1996 David Hogan, see README for licence details */

#ifdef	DEBUG
#define	trace(s, c, e)	dotrace((s), (c), (e))
#else
#define	trace(s, c, e)
#endif

/* color.c */
unsigned long colorpixel(Display*, int, unsigned long);

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
void	focusin();
void	reparent();

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

/* grab.c */
int 	menuhit();
Client	*selectwin();
int 	sweep();
int 	drag();
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
