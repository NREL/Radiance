/* Copyright (c) 1987 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  x10.c - driver for X-windows version 10.4
 *
 *     5/7/87
 */

#include  <stdio.h>

#include  <sys/ioctl.h>

#include  <X/Xlib.h>
#include  <X/cursors/bcross.cursor>
#include  <X/cursors/bcross_mask.cursor>

#include  "color.h"

#include  "driver.h"

#include  "xtwind.h"

#define GAMMA		2.0		/* exponent for color correction */

#define BORWIDTH	5		/* border width */
#define BARHEIGHT	25		/* menu bar size */
#define COMHEIGHT	(COMLH*COMCH)	/* command line height (pixels) */

#define COMFN		"8x13"		/* command line font name */
#define COMLH		3		/* number of command lines */
#define COMCW		8		/* approx. character width (pixels) */
#define COMCH		13		/* approx. character height (pixels) */

#ifndef WFLUSH
#define WFLUSH		30		/* flush after this many rays */
#endif

#define  hashcolr(c)	((67*(c)[RED]+59*(c)[GRN]+71*(c)[BLU])%ncolors)

#define  checkinp()	while (XPending() > 0) getevent()

#define  levptr(etype)	((etype *)&thisevent)

static char  *clientname;		/* calling client's name */

static XEvent  thisevent;		/* current event */

static int  colres = 128;		/* color resolution */
static COLR  colrmap[256];		/* our color mapping */

static int  ncolors = 0;		/* color table size */
static COLR  *colrtbl;			/* our color table */
static int  *pixval;			/* associated pixel values */

static Display  *ourdisplay = NULL;	/* our display */

static Window  gwind = 0;		/* our graphics window */

static Cursor  pickcursor = 0;		/* cursor used for picking */

static int  gwidth = 0;			/* graphics window width */
static int  gheight = 0;		/* graphics window height */

static TEXTWIND  *comline = NULL;	/* our command line */

static char  c_queue[64];		/* input queue */
static int  c_first = 0;		/* first character in queue */
static int  c_last = 0;			/* last character in queue */

extern char  *malloc();

int  x_close(), x_clear(), x_paintr(), x_errout(),
		x_getcur(), x_comout(), x_comin();

static struct driver  x_driver = {
	x_close, x_clear, x_paintr, x_getcur,
	x_comout, x_comin,
	MAXRES, MAXRES
};


struct driver *
x_init(name)			/* initialize driver */
char  *name;
{
	ourdisplay = XOpenDisplay(NULL);
	if (ourdisplay == NULL) {
		stderr_v("cannot open X-windows; DISPLAY variable set?\n");
		return(NULL);
	}
	if (DisplayPlanes() < 4) {
		stderr_v("not enough colors\n");
		return(NULL);
	}
	if (getmap() < 0)			/* not fatal */
		stderr_v("cannot allocate colors\n");

	pickcursor = XCreateCursor(bcross_width, bcross_height,
			bcross_bits, bcross_mask_bits,
			bcross_x_hot, bcross_y_hot,
			BlackPixel, WhitePixel, GXcopy);
	clientname = name;
	x_driver.inpready = 0;
	cmdvec = x_comout;			/* set error vectors */
	if (wrnvec != NULL)
		wrnvec = x_errout;
	return(&x_driver);
}


static
x_close()			/* close our display */
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
		XDestroyWindow(gwind);
		gwind = 0;
		gwidth = gheight = 0;
	}
	XFreeCursor(pickcursor);
	freemap();
	XCloseDisplay(ourdisplay);
	ourdisplay = NULL;
}


static
x_clear(xres, yres)			/* clear our display */
int  xres, yres;
{
	if (xres != gwidth || yres != gheight) {	/* new window */
		if (comline != NULL)
			xt_close(comline);
		if (gwind != 0)
			XDestroyWindow(gwind);
		gwind = XCreateWindow(RootWindow, 0, BARHEIGHT,
				xres, yres+COMHEIGHT, BORWIDTH,
				BlackPixmap, BlackPixmap);
		if (gwind == 0)
			goto fail;
		comline = xt_open(gwind, 0, yres, xres, COMHEIGHT, 0, COMFN);
		if (comline == NULL)
			goto fail;
		XMapWindow(gwind);
		XSelectInput(gwind,
	KeyPressed|ButtonPressed|ExposeWindow|ExposeRegion|UnmapWindow);
		XStoreName(gwind, clientname);
		gwidth = xres;
		gheight = yres;
	} else						/* just clear */
		XClear(gwind);
	newmap();
	checkinp();
	return;
fail:
	stderr_v("Failure opening window in x_clear\n");
	quit(1);
}


static
x_paintr(col, xmin, ymin, xmax, ymax)		/* fill a rectangle */
COLOR  col;
int  xmin, ymin, xmax, ymax;
{
	extern long  nrays;		/* global ray count */
	static long  lastflush = 0;	/* ray count at last flush */
	int  ndx;

	if (ncolors > 0) {
		if ((ndx = colindex(col)) < 0) {
			colres >>= 1;
			redraw();
			return;
		}
		XPixSet(gwind, xmin, gheight-ymax, xmax-xmin, ymax-ymin,
				pixval[ndx]);
	}
	if (nrays - lastflush >= WFLUSH) {
		if (ncolors <= 0)	/* output necessary for death */
			XPixSet(gwind,0,0,1,1,BlackPixel);
		checkinp();
		lastflush = nrays;
	}
}


static
x_comin(inp)			/* read in a command line */
char  *inp;
{
	int  x_getc(), x_comout();

	xt_cursor(comline, TBLKCURS);
	editline(inp, x_getc, x_comout);
	xt_cursor(comline, TNOCURS);
}


static
x_comout(out)			/* output a string to command line */
char  *out;
{
	if (comline != NULL)
		xt_puts(out, comline);
	XFlush();
}


static
x_errout(msg)			/* output an error message */
char  *msg;
{
	x_comout(msg);
	stderr_v(msg);		/* send to stderr also! */
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


static int
colindex(col)			/* return index for color */
COLOR  col;
{
	static COLR  clr;
	int  hval;
	register int  ndx, i;

	mapcolor(clr, col);

	hval = ndx = hashcolr(clr);
	
	for (i = 1; i < ncolors; i++) {
		if (colrtbl[ndx][EXP] == 0) {
			colrtbl[ndx][RED] = clr[RED];
			colrtbl[ndx][GRN] = clr[GRN];
			colrtbl[ndx][BLU] = clr[BLU];
			colrtbl[ndx][EXP] = COLXS;
			newcolr(ndx, clr);
			return(ndx);
		}
		if (		colrtbl[ndx][RED] == clr[RED] &&
				colrtbl[ndx][GRN] == clr[GRN] &&
				colrtbl[ndx][BLU] == clr[BLU]	)
			return(ndx);
		ndx = (hval + i*i) % ncolors;
	}
	return(-1);
}


static
newcolr(ndx, clr)		/* enter a color into hardware table */
int  ndx;
COLR  clr;
{
	Color  xcolor;

	xcolor.pixel = pixval[ndx];
	xcolor.red = clr[RED] << 8;
	xcolor.green = clr[GRN] << 8;
	xcolor.blue = clr[BLU] << 8;

	XStoreColor(&xcolor);
}


static
mapcolor(clr, col)			/* map to our color space */
COLR  clr;
COLOR  col;
{
	register int  i, p;
					/* compute color table value */
	for (i = 0; i < 3; i++) {
		p = colval(col,i) * 255.0 + 0.5;
		if (p < 0) p = 0;
		else if (p > 255) p = 255;
		clr[i] = colrmap[p][i];
	}
	clr[EXP] = COLXS;
}


static
getmap()				/* get the color map */
{
	int  planes;

	for (ncolors=(1<<DisplayPlanes())-3; ncolors>12; ncolors=ncolors*.937){
		colrtbl = (COLR *)malloc(ncolors*sizeof(COLR));
		pixval = (int *)malloc(ncolors*sizeof(int));
		if (colrtbl == NULL || pixval == NULL)
			return(-1);
		if (XGetColorCells(0,ncolors,0,&planes,pixval) != 0)
			return(0);
		free((char *)colrtbl);
		free((char *)pixval);
	}
	ncolors = 0;
	return(-1);
}


static
freemap()				/* free our color map */
{
	if (ncolors == 0)
		return;
	XFreeColors(pixval, ncolors, 0);
	free((char *)colrtbl);
	free((char *)pixval);
	ncolors = 0;
}


static
newmap()				/* initialize the color map */
{
	double  pow();
	int  val;
	register int  i;

	for (i = 0; i < 256; i++) {
		val = pow(i/256.0, 1.0/GAMMA) * colres;
		val = (val*256 + 128) / colres;
		colrmap[i][RED] = colrmap[i][GRN] = colrmap[i][BLU] = val;
		colrmap[i][EXP] = COLXS;
	}
	for (i = 0; i < ncolors; i++)
		colrtbl[i][EXP] = 0;
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
		if (ncolors == 0 && levptr(XExposeEvent)->subwindow == 0)
			if (getmap() < 0)
				stderr_v("cannot grab colors\n");
			else
				newmap();
		/* fall through */
	case ExposeRegion:
		fixwindow(levptr(XExposeEvent));
		break;
	case UnmapWindow:
		if (levptr(XUnmapEvent)->subwindow == 0)
			freemap();
		break;
	case ButtonPressed:		/* handled in x_getcur() */
		break;
	}
}


static
getkey(ekey)				/* get input key */
register XKeyPressedEvent  *ekey;
{
	int  n;
	register char  *str;

	str = XLookupMapping(ekey, &n);
	while (n-- > 0 && c_last < sizeof(c_queue))
		c_queue[c_last++] = *str++;
	x_driver.inpready = c_last - c_first;
}


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
