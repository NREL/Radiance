#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/* Copyright (c) 1989 Regents of the University of California */

/*
 *  x11.c - driver for X-windows version 11.3
 *
 *     Jan 1990
 */

#include  <stdio.h>

#include  <sys/ioctl.h>

#include  <X11/Xlib.h>
#include  <X11/cursorfont.h>
#include  <X11/Xutil.h>

#include  "color.h"
#include  "driver.h"
#include  "x11twind.h"
#include  "x11icon.h"

#define GAMMA		2.2		/* exponent for color correction */

#define MINWIDTH	(32*COMCW)	/* minimum graphics window width */
#define MINHEIGHT	MINWIDTH	/* minimum graphics window height */

#define BORWIDTH	5		/* border width */
#define COMHEIGHT	(COMLH*COMCH)	/* command line height (pixels) */

#define COMFN		"8x13"		/* command line font name */
#define COMLH		3		/* number of command lines */
#define COMCW		8		/* approx. character width (pixels) */
#define COMCH		14		/* approx. character height (pixels) */

#define  ourscreen	DefaultScreen(ourdisplay)
#define  ourroot	RootWindow(ourdisplay,ourscreen)

#define  levptr(etype)	((etype *)&currentevent)

static XEvent  currentevent;		/* current event */

static int  ncolors = 0;		/* color table size */
static unsigned long  *pixval = NULL;	/* allocated pixels */
static unsigned long  ourblack=0, ourwhite=1;

static Display  *ourdisplay = NULL;	/* our display */

static XVisualInfo  ourvinfo;		/* our visual information */

static Window  gwind = 0;		/* our graphics window */

static Cursor  pickcursor = 0;		/* cursor used for picking */

static int  gwidth, gheight;		/* graphics window size */

static TEXTWIND  *comline = NULL;	/* our command line */

static char  c_queue[64];		/* input queue */
static int  c_first = 0;		/* first character in queue */
static int  c_last = 0;			/* last character in queue */

static GC  ourgc = 0;			/* our graphics context for drawing */

static Colormap ourmap = 0;		/* our color map */

extern char  *malloc(), *getcombuf();

static int  x11_close(), x11_clear(), x11_paintr(), x11_errout(),
		x11_getcur(), x11_comout(), x11_comin(), x11_flush();

static struct driver  x11_driver = {
	x11_close, x11_clear, x11_paintr, x11_getcur,
	x11_comout, x11_comin, x11_flush, 1.0
};


struct driver *
x11_init(name, id)		/* initialize driver */
char  *name, *id;
{
	int  nplanes;
	XSetWindowAttributes	ourwinattr;
	XWMHints  ourxwmhints;
	XSizeHints	oursizhints;

	ourdisplay = XOpenDisplay(NULL);
	if (ourdisplay == NULL) {
		stderr_v("cannot open X-windows; DISPLAY variable set?\n");
		return(NULL);
	}
					/* find a usable visual */
	nplanes = DisplayPlanes(ourdisplay, ourscreen);
	if (	!XMatchVisualInfo(ourdisplay,ourscreen,
			24,TrueColor,&ourvinfo) &&
		!XMatchVisualInfo(ourdisplay,ourscreen,
			24,DirectColor,&ourvinfo)	) {
		if (nplanes < 4) {
			stderr_v("not enough colors\n");
			return(NULL);
		} else if (!XMatchVisualInfo(ourdisplay,ourscreen,
					nplanes,PseudoColor,&ourvinfo) &&
				!XMatchVisualInfo(ourdisplay,ourscreen,
					nplanes,GrayScale,&ourvinfo)) {
			stderr_v("unsupported visual type\n");
			return(NULL);
		}
		ourblack = BlackPixel(ourdisplay,ourscreen);
		ourwhite = WhitePixel(ourdisplay,ourscreen);
	} else {
		ourblack = 0;
		ourwhite = ~0;
	}
	make_gmap(GAMMA);
	/* open window */
	ourwinattr.background_pixel = ourblack;
	ourwinattr.border_pixel = ourblack;
					/* this is a waste! */
	ourwinattr.colormap = XCreateColormap(ourdisplay, ourroot,
				ourvinfo.visual, AllocNone);
	gwind = XCreateWindow(ourdisplay, ourroot, 0, 0,
		DisplayWidth(ourdisplay,ourscreen)-2*BORWIDTH,
		DisplayHeight(ourdisplay,ourscreen)-2*BORWIDTH,
		BORWIDTH, ourvinfo.depth, InputOutput, ourvinfo.visual,
		CWBackPixel|CWBorderPixel|CWColormap, &ourwinattr);
	if (gwind == 0) {
		stderr_v("cannot create window\n");
		return(NULL);
	}
	XStoreName(ourdisplay, gwind, id);
	/* create a cursor */
	pickcursor = XCreateFontCursor(ourdisplay, XC_diamond_cross);
	ourgc = XCreateGC(ourdisplay, gwind, 0, NULL);
	ourxwmhints.flags = InputHint|IconPixmapHint;
	ourxwmhints.input = True;
	ourxwmhints.icon_pixmap = XCreateBitmapFromData(ourdisplay,
			gwind, x11icon_bits, x11icon_width, x11icon_height);
	XSetWMHints(ourdisplay, gwind, &ourxwmhints);
	oursizhints.min_width = MINWIDTH;
	oursizhints.min_height = MINHEIGHT+COMHEIGHT;
	oursizhints.flags = PMinSize;
	XSetNormalHints(ourdisplay, gwind, &oursizhints);
	XSelectInput(ourdisplay, gwind, ExposureMask);
	XMapWindow(ourdisplay, gwind);
	XWindowEvent(ourdisplay, gwind, ExposureMask, levptr(XEvent));
	gwidth = levptr(XExposeEvent)->width;
	gheight = levptr(XExposeEvent)->height - COMHEIGHT;
	x11_driver.xsiz = gwidth < MINWIDTH ? MINWIDTH : gwidth;
	x11_driver.ysiz = gheight < MINHEIGHT ? MINHEIGHT : gheight;
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
	freepixels();
	XFreeGC(ourdisplay, ourgc);
	XDestroyWindow(ourdisplay, gwind);
	gwind = 0;
	ourgc = 0;
	XFreeCursor(ourdisplay, pickcursor);
	XCloseDisplay(ourdisplay);
	ourdisplay = NULL;
}


static
x11_clear(xres, yres)			/* clear our display */
int  xres, yres;
{
						/* check limits */
	if (xres < MINWIDTH)
		xres = MINWIDTH;
	if (yres < MINHEIGHT)
		yres = MINHEIGHT;
						/* resize window */
	if (xres != gwidth || yres != gheight) {
		XSelectInput(ourdisplay, gwind, 0);
		XResizeWindow(ourdisplay, gwind, xres, yres+COMHEIGHT);
		gwidth = xres;
		gheight = yres;
		XFlush(ourdisplay);
		sleep(2);			/* wait for window manager */
		XSync(ourdisplay, 1);		/* discard input */
	}
	XClearWindow(ourdisplay, gwind);
						/* reinitialize color table */
	if (ourvinfo.class == PseudoColor || ourvinfo.class == GrayScale)
		if (getpixels() == 0)
			stderr_v("cannot allocate colors\n");
		else
			new_ctab(ncolors);
						/* get new command line */
	if (comline != NULL)
		xt_close(comline);
	comline = xt_open(ourdisplay, gwind, 0, gheight,
			gwidth, COMHEIGHT, 0, ourblack, ourwhite, COMFN);
	if (comline == NULL) {
		stderr_v("Cannot open command line window\n");
		quit(1);
	}
	XSelectInput(ourdisplay, comline->w, ExposureMask);
						/* remove earmuffs */
	XSelectInput(ourdisplay, gwind,
		StructureNotifyMask|ExposureMask|KeyPressMask|ButtonPressMask);
}


static
x11_paintr(col, xmin, ymin, xmax, ymax)		/* fill a rectangle */
COLOR  col;
int  xmin, ymin, xmax, ymax;
{
	extern int  xnewcolr();		/* pixel assignment routine */
	extern unsigned long  true_pixel();
	unsigned long  pixel;

	if (ncolors > 0)
		pixel = pixval[get_pixel(col, xnewcolr)];
	else if (ourvinfo.class == TrueColor || ourvinfo.class == DirectColor)
		pixel = true_pixel(col);
	else
		return;
	XSetForeground(ourdisplay, ourgc, pixel);
	XFillRectangle(ourdisplay, gwind, 
		ourgc, xmin, gheight-ymax, xmax-xmin, ymax-ymin);
}


static
x11_flush()			/* flush output */
{
	XNoOp(ourdisplay);
	while (XPending(ourdisplay) > 0)
		getevent();
}


static
x11_comin(inp, prompt)		/* read in a command line */
char  *inp, *prompt;
{
	extern int  x11_getc();

	if (prompt != NULL)
		if (fromcombuf(inp, &x11_driver))
			return;
		else
			xt_puts(prompt, comline);
	xt_cursor(comline, TBLKCURS);
	editline(inp, x11_getc, x11_comout);
	xt_cursor(comline, TNOCURS);
}


static
x11_comout(out)			/* output a string to command line */
char  *out;
{
	if (comline == NULL)
		return;
	xt_puts(out, comline);
	if (out[strlen(out)-1] == '\n')
		XFlush(ourdisplay);
}


static
x11_errout(msg)			/* output an error message */
char  *msg;
{
	stderr_v(msg);		/* send to stderr also! */
	x11_comout(msg);
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
	XColor  thiscolor;
	register int  i, j;

	if (ncolors > 0)
		return(ncolors);
	if (ourvinfo.visual == DefaultVisual(ourdisplay,ourscreen)) {
		ourmap = DefaultColormap(ourdisplay,ourscreen);
		goto loop;
	}
newmap:
	ourmap = XCreateColormap(ourdisplay,gwind,ourvinfo.visual,AllocNone);
loop:
	for (ncolors = ourvinfo.colormap_size;
			ncolors > ourvinfo.colormap_size/3;
			ncolors = ncolors*.937) {
		pixval = (unsigned long *)malloc(ncolors*sizeof(unsigned long));
		if (pixval == NULL)
			return(ncolors = 0);
		if (XAllocColorCells(ourdisplay,ourmap,0,NULL,0,
				pixval,ncolors) != 0)
			break;
		free((char *)pixval);
		pixval = NULL;
	}
	if (pixval == NULL) {
		if (ourmap == DefaultColormap(ourdisplay,ourscreen))
			goto newmap;		/* try it with our map */
		else
			return(ncolors = 0);	/* failed */
	}
	if (ourmap != DefaultColormap(ourdisplay,ourscreen))
		for (i = 0; i < ncolors; i++) {	/* reset black and white */
			if (pixval[i] != ourblack && pixval[i] != ourwhite)
				continue;
			thiscolor.pixel = pixval[i];
			thiscolor.flags = DoRed|DoGreen|DoBlue;
			XQueryColor(ourdisplay,
					DefaultColormap(ourdisplay,ourscreen),
					&thiscolor);
			XStoreColor(ourdisplay, ourmap, &thiscolor);
			for (j = i; j+1 < ncolors; j++)
				pixval[j] = pixval[j+1];
			ncolors--;
			i--;
		}
	XSetWindowColormap(ourdisplay, gwind, ourmap);
	return(ncolors);
}


static
freepixels()				/* free our pixels */
{
	if (ncolors == 0)
		return;
	XFreeColors(ourdisplay,ourmap,pixval,ncolors,0L);
	ncolors = 0;
	if (ourmap != DefaultColormap(ourdisplay,ourscreen))
		XFreeColormap(ourdisplay, ourmap);
	ourmap = 0;
}


static unsigned long
true_pixel(col)			/* return true pixel value for color */
COLOR  col;
{
	unsigned long  rval;
	BYTE  rgb[3];

	map_color(rgb, col);
	rval = ourvinfo.red_mask*rgb[RED]/255 & ourvinfo.red_mask;
	rval |= ourvinfo.green_mask*rgb[GRN]/255 & ourvinfo.green_mask;
	rval |= ourvinfo.blue_mask*rgb[BLU]/255 & ourvinfo.blue_mask;
	return(rval);
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
	case ConfigureNotify:
		resizewindow(levptr(XConfigureEvent));
		break;
	case UnmapNotify:
		freepixels();
		break;
	case MapNotify:
		if (ourvinfo.class == PseudoColor ||
				ourvinfo.class == GrayScale)
			if (getpixels() == 0)
				stderr_v("Cannot allocate colors\n");
			else
				new_ctab(ncolors);
		break;
	case Expose:
		fixwindow(levptr(XExposeEvent));
		break;
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
	register int  n;

	n = XLookupString(ekey, c_queue+c_last, sizeof(c_queue)-c_last,
				NULL, NULL);
	c_last += n;
	x11_driver.inpready += n;
}


static
fixwindow(eexp)				/* repair damage to window */
register XExposeEvent  *eexp;
{
	if (eexp->window == gwind) {
		sprintf(getcombuf(&x11_driver), "repaint %d %d %d %d\n",
			eexp->x, gheight - eexp->y - eexp->height,
			eexp->x + eexp->width, gheight - eexp->y);
	} else if (eexp->window == comline->w) {
		if (eexp->count == 0)
			xt_redraw(comline);
	}
}


static
resizewindow(ersz)			/* resize window */
register XConfigureEvent  *ersz;
{
	if (ersz->width == gwidth && ersz->height-COMHEIGHT == gheight)
		return;

	gwidth = ersz->width;
	gheight = ersz->height-COMHEIGHT;
	x11_driver.xsiz = gwidth < MINWIDTH ? MINWIDTH : gwidth;
	x11_driver.ysiz = gheight < MINHEIGHT ? MINHEIGHT : gheight;

	strcpy(getcombuf(&x11_driver), "new\n");
}
