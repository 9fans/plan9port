#include <X11/Intrinsic.h>

/*
 * Disclaimer: No I don't actually code like this but this is a simple,
 * "Quick-n-Dirty", plain, vanilla, "No ups, No extras" piece of code.
 */

main(argc, argv)
int argc;
char **argv;
{
    Display *dpy;
    int screen;
    Window window;
    XEvent event;
    extern Boolean use_separate_lines;

    if (!(dpy = XOpenDisplay(""))) {
	printf("Failed to open display...\n");
	exit(1);
    }
    screen = DefaultScreen(dpy);

    window = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 100, 100,
      300, 200, 2, BlackPixel(dpy, screen), WhitePixel(dpy, screen));

    XSelectInput(dpy, window, KeyPressMask | KeyReleaseMask | ButtonPressMask |
      ButtonReleaseMask | EnterWindowMask | LeaveWindowMask |
      PointerMotionMask | PointerMotionHintMask | Button1MotionMask |
      Button2MotionMask | Button3MotionMask | Button4MotionMask |
      Button5MotionMask | ButtonMotionMask | KeymapStateMask |
      ExposureMask | VisibilityChangeMask | StructureNotifyMask |
      SubstructureNotifyMask | SubstructureRedirectMask | FocusChangeMask |
      PropertyChangeMask | ColormapChangeMask | OwnerGrabButtonMask);

    XMapWindow(dpy, window);

    /* set this to false to make ShowEvent take up less vertival space */
    use_separate_lines = True;

    while (1) {
	XNextEvent(dpy, &event);
	printf("Detail of %s event:\n", GetType(&event));
	ShowEvent(&event);
	printf("\n\n");
    }
}
