/* Copyright (c) 1987 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  x10.c - driver for X-windows version 10.4
 *
 *     5/7/87
 */

#include  "standard.h"

#include  <X/Xlib.h>
#include  <X/cursors/bcross.cursor>
#include  <X/cursors/bcross_mask.cursor>

#include  "color.h"

#include  "driver.h"

#include  "xtwind.h"

#define GAMMA		2.2		/* exponent for color correction */

#define BORWIDTH	5		/* border width */
#define COMHEIGHT	(COMLH*COMCH)	/* command line height (pixels) */
#define MINWIDTH	(32*COMCW)	/* minimum graphics window width */
#define MINHEIGHT	MINWIDTH		/* minimum graphics window height */

#define COMFN		"8x13"		/* command line font name */
#define COMLH		3		/* number of command lines */
#define COMCW		8		/* approx. character width (pixels) */
#define COMCH		13		/* approx. character height (pixels) */

#define  levptr(etype)	((etype *)&thisevent)

static XEvent  thisevent;		/* current event */

static int  ncolors = 0;		/* color table size */
static int  *pixval;			/* allocated pixel values */

static Display  *ourdisplay = NULL;	/* our display */

static Window  gwind = 0;		/* our graphics window */

static Cursor  pickcursor = 0;		/* cursor used for picking */

static int  gwidth, gheight;		/* graphics window size */

static TEXTWIND  *comline = NULL;	/* our command line */

static char  c_queue[64];		/* input queue */
static int  c_first = 0;		/* first character in queue */
static int  c_last = 0;			/* last character in queue */

extern char  *malloc(), *getcombuf();

extern char  *progname;

static int  x_close(), x_clear(), x_paintr(), x_errout(),
		x_getcur(), x_comout(), x_comin(), x_flush();

static struct driver  x_driver = {
	x_close, x_clear, x_paintr, x_getcur,
	x_comout, x_comin, x_flush, 1.0
};


struct driver *
x_init(name, id)		/* initialize driver */
char  *name, *id;
{
	extern char  *getenv();
	char  *gv;
	char  defgeom[32];
	OpaqueFrame  mainframe;

	ourdisplay = XOpenDisplay(NULL);
	if (ourdisplay == NULL) {
		eputs("cannot open X-windows; DISPLAY variable set?\n");
		return(NULL);
	}
	if (DisplayPlanes() < 4) {
		eputs("not enough colors\n");
		return(NULL);
	}
				/* make color map */
	if ((gv = getenv("DISPLAY_GAMMA")) != NULL)
		make_gmap(atof(gv));
	else
		make_gmap(GAMMA);

	pickcursor = XCreateCursor(bcross_width, bcross_height,
			bcross_bits, bcross_mask_bits,
			bcross_x_hot, bcross_y_hot,
			BlackPixel, WhitePixel, GXcopy);
	mainframe.bdrwidth = BORWIDTH;
	mainframe.border = BlackPixmap;
	mainframe.background = BlackPixmap;
	sprintf(defgeom, "=%dx%d+0+22", DisplayWidth()-(2*BORWIDTH),
			DisplayHeight()-(2*BORWIDTH+22));
	gwind = XCreate("X10 display driver", progname, NULL, defgeom,
			&mainframe, MINWIDTH, MINHEIGHT+COMHEIGHT);
	if (gwind == 0) {
		eputs("can't create window\n");
		return(NULL);
	}
	XStoreName(gwind, id);
	XSelectInput(gwind, KeyPressed|ButtonPressed|
			ExposeWindow|ExposeRegion|UnmapWindow);
	gwidth = mainframe.width;
	gheight = mainframe.height-COMHEIGHT;
	x_driver.xsiz = gwidth < MINWIDTH ? MINWIDTH : gwidth;
	x_driver.ysiz = gheight < MINHEIGHT ? MINHEIGHT : gheight;
	x_driver.inpready = 0;
	erract[COMMAND].pf = x_comout;		/* set error vectors */
	if (erract[WARNING].pf != NULL)
		erract[WARNING].pf = x_errout;
	return(&x_driver);
}


static
x_close()			/* close our display */
{
	erract[COMMAND].pf = NULL;		/* reset error vectors */
	if (erract[WARNING].pf != NULL)
		erract[WARNING].pf = wputs;
	if (ourdisplay == NULL)
		return;
	if (comline != NULL) {
		xt_close(comline);
		comline = NULL;
	}
	if (gwind != 0) {
		XDestroyWindow(gwind);
		gwind = 0;
	}
	XFreeCursor(pickcursor);
	freepixels();
	XCloseDisplay(ourdisplay);
	ourdisplay = NULL;
}


static
x_clear(xres, yres)			/* clear our display */
int  xres, yres;
{
	if (xres != gwidth || yres != gheight) {	/* new window */
		XChangeWindow(gwind, xres, yres+COMHEIGHT);
		gwidth = xres;
		gheight = yres;
	} else						/* just clear */
		XClear(gwind);
						/* reinitialize color table */
	if (getpixels() == 0)
		eputs("cannot allocate colors\n");
	else
		new_ctab(ncolors);
						/* open new command line */
	if (comline != NULL)
		xt_close(comline);
	comline = xt_open(gwind, 0, yres, xres, COMHEIGHT, 0, COMFN);
	if (comline == NULL) {
		eputs("Cannot open command line window\n");
		quit(1);
	}
	XMapWindow(gwind);			/* make sure it's mapped */
	XSync(1);				/* discard input */
	return;
}


static
x_paintr(col, xmin, ymin, xmax, ymax)		/* fill a rectangle */
COLOR  col;
int  xmin, ymin, xmax, ymax;
{
	extern int  xnewcolr();		/* pixel assignment routine */

	if (ncolors > 0) {
		XPixSet(gwind, xmin, gheight-ymax, xmax-xmin, ymax-ymin,
				pixval[get_pixel(col, xnewcolr)]);
	}
}


static
x_flush()			/* flush output */
{
	if (ncolors <= 0)	/* output necessary for death */
		XPixSet(gwind,0,0,1,1,BlackPixel);
	while (XPending() > 0)
		getevent();

}


static
x_comin(inp, prompt)		/* read in a command line */
char  *inp, *prompt;
{
	extern int  x_getc();

	if (prompt != NULL)
		if (fromcombuf(inp, &x_driver))
			return;
		else
			xt_puts(prompt, comline);
	xt_cursor(comline, TBLKCURS);
	editline(inp, x_getc, x_comout);
	xt_cursor(comline, TNOCURS);
}


static
x_comout(out)			/* output a string to command line */
char  *out;
{
	if (comline == NULL)
		return;
	xt_puts(out, comline);
	if (out[strlen(out)-1] == '\n')
		XFlush();
}


static
x_errout(msg)			/* output an error message */
char  *msg;
{
	eputs(msg);		/* send to stderr also! */
	x_comout(msg);
}


static int
x_getcur(xp, yp)		/* get cursor position */
int  *xp, *yp;
{
	while (XGrabMouse(gwind, pickcursor, ButtonPressed) == 0)
		sleep(1);
	XFocusKeyboard(gwind);
	do
		getevent();
	while (c_last <= c_first && levptr(XEvent)->type != ButtonPressed);
	*xp = levptr(XKeyOrButtonEvent)->x;
	*yp = gheight-1 - levptr(XKeyOrButtonEvent)->y;
	XFocusKeyboard(RootWindow);
	XUngrabMouse();
	XFlush();				/* insure release */
	if (c_last > c_first)			/* key pressed */
		return(x_getc());
						/* button pressed */
	switch (levptr(XKeyOrButtonEvent)->detail & 0377) {
	case LeftButton:
		return(MB1);
	case MiddleButton:
		return(MB2);
	case RightButton:
		return(MB3);
	}
	return(ABORT);
}


static
xnewcolr(ndx, r, g, b)		/* enter a color into hardware table */
int  ndx;
int  r, g, b;
{
	Color  xcolor;

	xcolor.pixel = pixval[ndx];
	xcolor.red = r << 8;
	xcolor.green = g << 8;
	xcolor.blue = b << 8;

	XStoreColor(&xcolor);
}


static int
getpixels()				/* get the color map */
{
	int  planes;

	if (ncolors > 0)
		return(ncolors);
	for (ncolors=(1<<DisplayPlanes())-3; ncolors>12; ncolors=ncolors*.937){
		pixval = (int *)malloc(ncolors*sizeof(int));
		if (pixval == NULL)
			break;
		if (XGetColorCells(0,ncolors,0,&planes,pixval) != 0)
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
	XFreeColors(pixval, ncolors, 0);
	free((char *)pixval);
	ncolors = 0;
}


static int
x_getc()			/* get a command character */
{
	while (c_last <= c_first) {
		c_first = c_last = 0;		/* reset */
		getevent();			/* wait for key */
	}
	x_driver.inpready--;
	return(c_queue[c_first++]);
}


static
getevent()			/* get next event */
{
	XNextEvent(levptr(XEvent));
	switch (levptr(XEvent)->type) {
	case KeyPressed:
		getkey(levptr(XKeyPressedEvent));
		break;
	case ExposeWindow:
		windowchange(levptr(XExposeEvent));
		break;
	case ExposeRegion:
		fixwindow(levptr(XExposeEvent));
		break;
	case UnmapWindow:
		if (levptr(XUnmapEvent)->subwindow == 0)
			freepixels();
		break;
	case ButtonPressed:		/* handled in x_getcur() */
		break;
	}
}


static
windowchange(eexp)			/* process window change event */
register XExposeEvent  *eexp;
{
	if (eexp->subwindow != 0) {
		fixwindow(eexp);
		return;
	}
					/* check for change in size */
	if (eexp->width != gwidth || eexp->height-COMHEIGHT != gheight) {
		gwidth = eexp->width;
		gheight = eexp->height-COMHEIGHT;
		x_driver.xsiz = gwidth < MINWIDTH ? MINWIDTH : gwidth;
		x_driver.ysiz = gheight < MINHEIGHT ? MINHEIGHT : gheight;
		strcpy(getcombuf(&x_driver), "new\n");
		return;
	}
					/* remap colors */
	if (getpixels() == 0) {
		eputs("cannot allocate colors\n");
		return;
	}
	new_ctab(ncolors);
					/* redraw */
	fixwindow(eexp);
}


static
getkey(ekey)				/* get input key */
register XKeyPressedEvent  *ekey;
{
	int  n;
	register char  *str;

	str = XLookupMapping(ekey, &n);
	while (n-- > 0 && c_last < sizeof(c_queue)) {
		c_queue[c_last++] = *str++;
		x_driver.inpready++;
	}
}


static
fixwindow(eexp)				/* repair damage to window */
register XExposeEvent  *eexp;
{
	if (eexp->subwindow == 0)
		sprintf(getcombuf(&x_driver), "repaint %d %d %d %d\n",
			eexp->x, gheight - eexp->y - eexp->height,
			eexp->x + eexp->width, gheight - eexp->y);
	else if (eexp->subwindow == comline->w)
		xt_redraw(comline);
}
