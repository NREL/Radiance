#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/* Copyright (c) 1989 Regents of the University of California */

/*
 *  x11.c - driver for X-windows version 11R4
 *
 *     1989
 */

#include  <stdio.h>

#include  <sys/ioctl.h>

#include  <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/Xutil.h>
#ifdef notdef
#include  "bcross.cursor"
#include  "bcross_mask.cu"
#endif

#include  "color.h"

#include  "driver.h"

#include  "x11twind.h"

#define GAMMA		2.2		/* exponent for color correction */

#define BORWIDTH	5		/* border width */
#define BARHEIGHT	25		/* menu bar size */
#define COMHEIGHT	(COMLH*COMCH)	/* command line height (pixels) */

#define COMFN		"8x13"		/* command line font name */
#define COMLH		3		/* number of command lines */
#define COMCW		8		/* approx. character width (pixels) */
#define COMCH		14		/* approx. character height (pixels) */

#ifndef WFLUSH
#define WFLUSH		30		/* flush after this many rays */
#endif

#define  checkinp()	while (XPending(ourdisplay) > 0) getevent()

#define  levptr(etype)	((etype *)&thisevent)

static char  *clientname;		/* calling client's name */

static XEvent  thisevent;		/* current event */

static int  ncolors = 0;		/* color table size */
static int  *pixval = NULL;		/* allocated pixels */

static Display  *ourdisplay = NULL;	/* our display */

static Window  gwind = 0;		/* our graphics window */

static Cursor  pickcursor = 0;		/* cursor used for picking */

static int  gwidth = 0;			/* graphics window width */
static int  gheight = 0;		/* graphics window height */

static TEXTWIND  *comline = NULL;	/* our command line */

static char  c_queue[64];		/* input queue */
static int  c_first = 0;		/* first character in queue */
static int  c_last = 0;			/* last character in queue */

static GC  ourgc = 0;			/* our graphics context for drawing */

static Colormap ourmap;			/* our color map */

extern char  *malloc();

int  x11_close(), x11_clear(), x11_paintr(), x11_errout(),
		x11_getcur(), x11_comout(), x11_comin();

static struct driver  x11_driver = {
	x11_close, x11_clear, x11_paintr, x11_getcur,
	x11_comout, x11_comin,
	MAXRES, MAXRES
};


struct driver *
x11_init(name, id)		/* initialize driver */
char  *name, *id;
{
	Pixmap  bmCursorSrc, bmCursorMsk;

	ourdisplay = XOpenDisplay(NULL);
	if (ourdisplay == NULL) {
		stderr_v("cannot open X-windows; DISPLAY variable set?\n");
		return(NULL);
	}
	if (DisplayPlanes(ourdisplay, DefaultScreen(ourdisplay)) < 4) {
		stderr_v("not enough colors\n");
		return(NULL);
	}
	ourmap = DefaultColormap(ourdisplay,DefaultScreen(ourdisplay));
	make_gmap(GAMMA);			/* make color map */
	/*
	bmCursorSrc = XCreateBitmapFromData(ourdisplay, 
			gwind, bcross_bits, 
			bcross_width, bcross_height);
	bmCursorMsk = XCreateBitmapFromData(ourdisplay, 
			gwind, bcross_mask_bits, 
			bcross_width, bcross_height);

	pickcursor = XCreatePixmapCursor(ourdisplay,
			bmCursorSrc, bmCursorMsk, 
			BlackPixel(ourdisplay, 
			DefaultScreen(ourdisplay)), 
			WhitePixel(ourdisplay, 
			DefaultScreen(ourdisplay)), 
			bcross_x_hot, bcross_y_hot);
	XFreePixmap(ourdisplay, bmCursorSrc);
	XFreePixmap(ourdisplay, bmCursorMsk);
	*/
	/* create a cursor */
	pickcursor = XCreateFontCursor (ourdisplay, XC_diamond_cross);
	/*  new */
	clientname = id; 
	x11_driver.inpready = 0;
	cmdvec = x11_comout;			/* set error vectors */
	if (wrnvec != NULL)
		wrnvec = x11_errout;
	return(&x11_driver);
}


static
x11_close()			/* close our display */
{
	cmdvec = NULL;				/* reset error vectors */
	if (wrnvec != NULL)
		wrnvec = stderr_v;
	if (ourdisplay == NULL)
		return;
	if (comline != NULL) {
		xt_close(comline);
		comline = NULL;
	}
	if (gwind != 0) {
		XFreeGC(ourdisplay, ourgc);
		XDestroyWindow(ourdisplay, gwind);
		gwind = 0;
		gwidth = gheight = 0;
		ourgc = 0;
	}
	XFreeCursor(ourdisplay, pickcursor);
	freepixels();
	XCloseDisplay(ourdisplay);
	ourdisplay = NULL;
}


static
x11_clear(xres, yres)			/* clear our display */
int  xres, yres;
{
	XWMHints  ourxwmhints;
	XSetWindowAttributes ourwindowattr;

	if (xres != gwidth || yres != gheight) {	/* change window */
		if (comline != NULL)
			xt_close(comline);
		if (gwind == 0) {			/* new window */
			ourwindowattr.backing_store = Always;
			ourwindowattr.background_pixel =
			WhitePixel(ourdisplay, DefaultScreen(ourdisplay));
			ourwindowattr.border_pixel =
			BlackPixel(ourdisplay, DefaultScreen(ourdisplay));
			gwind = XCreateWindow(ourdisplay, 
				RootWindow(ourdisplay,
			  	DefaultScreen(ourdisplay)),
				0, 0, xres, yres+COMHEIGHT, BORWIDTH, 
				0, InputOutput, CopyFromParent,
				CWBackingStore|CWBackPixel|CWBorderPixel, 
				&ourwindowattr);
			if (gwind == 0)
				goto fail;
			XStoreName(ourdisplay, gwind, clientname);
			ourgc = XCreateGC(ourdisplay, gwind, 0, NULL);
			ourxwmhints.flags = InputHint;
			ourxwmhints.input = True;
			XSetWMHints(ourdisplay, gwind, &ourxwmhints);
			XSelectInput(ourdisplay, gwind, 
				     KeyPressMask|ButtonPressMask);
			XMapWindow(ourdisplay, gwind);
		} else					/* resize window */
			XResizeWindow(ourdisplay, gwind, xres, yres+COMHEIGHT);
		comline = xt_open(ourdisplay,
				DefaultGC(ourdisplay,DefaultScreen(ourdisplay)),
				gwind, 0, yres, xres, COMHEIGHT, 0, COMFN);
		if (comline == NULL)
			goto fail;
		gwidth = xres;
		gheight = yres;
		XFlush(ourdisplay);
		sleep(10);
	} else						/* just clear */
		XClearWindow(ourdisplay, gwind);
						/* reinitialize color table */
	if (ncolors == 0 && getpixels() == 0)
		stderr_v("cannot allocate colors\n");
	else
		new_ctab(ncolors);
	return;
fail:
	stderr_v("Failure opening window in x11_clear\n");
	quit(1);
}


static
x11_paintr(col, xmin, ymin, xmax, ymax)		/* fill a rectangle */
COLOR  col;
int  xmin, ymin, xmax, ymax;
{
	extern long  nrays;		/* global ray count */
	extern int  xnewcolr();		/* pixel assignment routine */
	static long  lastflush = 0;	/* ray count at last flush */

	if (ncolors > 0) {
		XSetForeground(ourdisplay, ourgc, 
				pixval[get_pixel(col, xnewcolr)]);
		XFillRectangle(ourdisplay, gwind, 
			ourgc, xmin, gheight-ymax, xmax-xmin, ymax-ymin);
	}
	if (nrays - lastflush >= WFLUSH) {
		if (ncolors <= 0)	/* output necessary for death */
			XFillRectangle(ourdisplay, gwind, 
					ourgc, 0, 0, 1 ,1);
		checkinp();
		lastflush = nrays;
	}
}


static
x11_comin(inp)			/* read in a command line */
char  *inp;
{
	int  x11_getc(), x11_comout();

	xt_cursor(comline, TBLKCURS);
	editline(inp, x11_getc, x11_comout);
	xt_cursor(comline, TNOCURS);
}


static
x11_comout(out)			/* output a string to command line */
char  *out;
{
	if (comline != NULL)
		xt_puts(out, comline);
	XFlush(ourdisplay);
}


static
x11_errout(msg)			/* output an error message */
char  *msg;
{
	x11_comout(msg);
	stderr_v(msg);		/* send to stderr also! */
}


static int
x11_getcur(xp, yp)		/* get cursor position */
int  *xp, *yp;
{
	while (XGrabPointer(ourdisplay, gwind, True, ButtonPressMask,
			GrabModeAsync, GrabModeAsync, None, pickcursor,
			CurrentTime) != GrabSuccess)
		sleep(2);

	do
		getevent();
	while (c_last <= c_first && levptr(XEvent)->type != ButtonPress);
	*xp = levptr(XButtonPressedEvent)->x;
	*yp = gheight-1 - levptr(XButtonPressedEvent)->y;
	XUngrabPointer(ourdisplay, CurrentTime);
	XFlush(ourdisplay);				/* insure release */
	if (c_last > c_first)			/* key pressed */
		return(x11_getc());
						/* button pressed */
	if (levptr(XButtonPressedEvent)->button & Button1)
		return(MB1);
	if (levptr(XButtonPressedEvent)->button & Button2)
		return(MB2);
	if (levptr(XButtonPressedEvent)->button & Button3)
		return(MB3);
	if (levptr(XButtonPressedEvent)->button & (Button4|Button5))
		return(MB1);
	return(ABORT);
}


static
xnewcolr(ndx, r, g, b)		/* enter a color into hardware table */
int  ndx;
int  r, g, b;
{
	XColor  xcolor;

	xcolor.pixel = pixval[ndx];
	xcolor.red = r << 8;
	xcolor.green = g << 8;
	xcolor.blue = b << 8;
	xcolor.flags = DoRed|DoGreen|DoBlue;

	XStoreColor(ourdisplay, ourmap, &xcolor);
}


static int
getpixels()				/* get the color map */
{
	Visual  *ourvis = DefaultVisual(ourdisplay,DefaultScreen(ourdisplay));

	freepixels();
	for (ncolors=(ourvis->map_entries)-3; ncolors>12; ncolors=ncolors*.937){
		pixval = (int *)malloc(ncolors*sizeof(int));
		if (pixval == NULL)
			break;
		if (XAllocColorCells(ourdisplay,ourmap,0,NULL,0,
				pixval,ncolors) != 0)
			return(ncolors);
		free((char *)pixval);
	}
	return(ncolors = 0);
}


static
freepixels()				/* free our pixels */
{
	if (ncolors == 0)
		return;
	XFreeColors(ourdisplay,ourmap,pixval,ncolors,0L);
	ncolors = 0;
}


static int
x11_getc()			/* get a command character */
{
	while (c_last <= c_first) {
		c_first = c_last = 0;		/* reset */
		getevent();			/* wait for key */
	}
	x11_driver.inpready--;
	return(c_queue[c_first++]);
}


static
getevent()			/* get next event */
{
	XNextEvent(ourdisplay, levptr(XEvent));
	switch (levptr(XEvent)->type) {
	case KeyPress:
		getkey(levptr(XKeyPressedEvent));
		break;
	case ButtonPress:
		break;
	}
}


static
getkey(ekey)				/* get input key */
register XKeyPressedEvent  *ekey;
{
	c_last += XLookupString(ekey, c_queue+c_last, sizeof(c_queue)-c_last,
				NULL, NULL);
	x11_driver.inpready = c_last-c_first;
}


#ifdef notdef
static
fixwindow(eexp)				/* repair damage to window */
register XExposeEvent  *eexp;
{
	if (eexp->subwindow == 0)
		repaint(eexp->x, gheight - eexp->y - eexp->height,
			eexp->x + eexp->width, gheight - eexp->y);
	else if (eexp->subwindow == comline->w)
		xt_redraw(comline);
}
#endif
