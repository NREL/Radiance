/* Copyright (c) 1997 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * X11 driver for holodeck display.
 * Based on rview driver.
 */

#include "standard.h"
#include "rhd_qtree.h"

#include  <X11/Xlib.h>
#include  <X11/cursorfont.h>
#include  <X11/Xutil.h>

#include  "x11icon.h"

#define GAMMA		2.2		/* default gamma correction */

#define MINWIDTH	480		/* minimum graphics window width */
#define MINHEIGHT	400		/* minimum graphics window height */

#define VIEWDIST	356		/* assumed viewing distance (mm) */

#define BORWIDTH	5		/* border width */

#define  ourscreen	DefaultScreen(ourdisplay)
#define  ourroot	RootWindow(ourdisplay,ourscreen)
#define  ourmask	(StructureNotifyMask|ExposureMask|KeyPressMask|\
			ButtonPressMask)

#define  levptr(etype)	((etype *)&currentevent)

struct driver	odev;			/* global device driver structure */

static XEvent  currentevent;		/* current event */

static int  ncolors = 0;		/* color table size */
static int  mapped = 0;			/* window is mapped? */
static unsigned long  *pixval = NULL;	/* allocated pixels */
static unsigned long  ourblack=0, ourwhite=1;

static Display  *ourdisplay = NULL;	/* our display */

static XVisualInfo  ourvinfo;		/* our visual information */

static Window  gwind = 0;		/* our graphics window */

static GC  ourgc = 0;			/* our graphics context for drawing */

static Colormap ourmap = 0;		/* our color map */

static double	pwidth, pheight;	/* pixel dimensions (mm) */

static int	inpresflags;		/* input result flags */

static int	heightlocked = 0;	/* lock vertical motion */

static int  getpixels(), xnewcolr(), freepixels(), resizewindow(),
		getevent(), getkey(), fixwindow();
static unsigned long  true_pixel();


static int
mytmflags()			/* figure out tone mapping flags */
{
	extern char	*progname;
	register char	*cp, *tail;
					/* find basic name */
	for (cp = tail = progname; *cp; cp++)
		if (*cp == '/')
			tail = cp+1;
	for (cp = tail; *cp && *cp != '.'; cp++)
		;
	if (cp-tail == 3 && !strncmp(tail, "x11", 3))
		return(TM_F_CAMERA);
	if (cp-tail == 4 && !strncmp(tail, "x11h", 4))
		return(TM_F_HUMAN);
	error(USER, "illegal driver name");
}


dev_open(id)			/* initialize X11 driver */
char  *id;
{
	extern char  *getenv();
	char  *gv;
	double	gamval = GAMMA;
	int  nplanes;
	int  n;
	XSetWindowAttributes	ourwinattr;
	XWMHints  ourxwmhints;
	XSizeHints	oursizhints;
					/* open display server */
	ourdisplay = XOpenDisplay(NULL);
	if (ourdisplay == NULL)
		error(USER, "cannot open X-windows; DISPLAY variable set?\n");
					/* find a usable visual */
	nplanes = DisplayPlanes(ourdisplay, ourscreen);
	if (XMatchVisualInfo(ourdisplay,ourscreen,
				nplanes>12?nplanes:24,TrueColor,&ourvinfo) ||
			XMatchVisualInfo(ourdisplay,ourscreen,
				nplanes>12?nplanes:24,DirectColor,&ourvinfo)) {
		ourblack = 0;
		ourwhite = ourvinfo.red_mask |
				ourvinfo.green_mask |
				ourvinfo.blue_mask ;
	} else {
		if (nplanes < 4)
			error(INTERNAL, "not enough colors\n");
		if (!XMatchVisualInfo(ourdisplay,ourscreen,
					nplanes,PseudoColor,&ourvinfo) &&
				!XMatchVisualInfo(ourdisplay,ourscreen,
					nplanes,GrayScale,&ourvinfo))
			error(INTERNAL, "unsupported visual type\n");
		ourblack = BlackPixel(ourdisplay,ourscreen);
		ourwhite = WhitePixel(ourdisplay,ourscreen);
	}
					/* set gamma and tone mapping */
	if ((gv = XGetDefault(ourdisplay, "radiance", "gamma")) != NULL
			|| (gv = getenv("DISPLAY_GAMMA")) != NULL)
		gamval = atof(gv);
	if (tmInit(mytmflags(), stdprims, gamval) == NULL)
		error(SYSTEM, "not enough memory in dev_open");
					/* open window */
	ourwinattr.background_pixel = ourblack;
	ourwinattr.border_pixel = ourblack;
	ourwinattr.event_mask = ourmask;
					/* this is stupid */
	ourwinattr.colormap = XCreateColormap(ourdisplay, ourroot,
				ourvinfo.visual, AllocNone);
	gwind = XCreateWindow(ourdisplay, ourroot, 0, 0,
		DisplayWidth(ourdisplay,ourscreen)-2*BORWIDTH,
		DisplayHeight(ourdisplay,ourscreen)-2*BORWIDTH,
		BORWIDTH, ourvinfo.depth, InputOutput, ourvinfo.visual,
		CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &ourwinattr);
	if (gwind == 0)
		error(SYSTEM, "cannot create window\n");
   	XStoreName(ourdisplay, gwind, id);
					/* get graphics context */
	ourgc = XCreateGC(ourdisplay, gwind, 0, NULL);
					/* set window manager hints */
	ourxwmhints.flags = InputHint|IconPixmapHint;
	ourxwmhints.input = True;
	ourxwmhints.icon_pixmap = XCreateBitmapFromData(ourdisplay,
			gwind, x11icon_bits, x11icon_width, x11icon_height);
	XSetWMHints(ourdisplay, gwind, &ourxwmhints);
	oursizhints.min_width = MINWIDTH;
	oursizhints.min_height = MINHEIGHT;
	oursizhints.flags = PMinSize;
	XSetNormalHints(ourdisplay, gwind, &oursizhints);
					/* map the window and get its size */
	XMapWindow(ourdisplay, gwind);
	dev_input();
					/* figure out sensible view */
	pwidth = (double)DisplayWidthMM(ourdisplay, ourscreen) /
			DisplayWidth(ourdisplay, ourscreen);
	pheight = (double)DisplayHeightMM(ourdisplay, ourscreen) /
			DisplayHeight(ourdisplay, ourscreen);
	copystruct(&odev.v, &stdview);
	odev.name = id;
	odev.v.type = VT_PER;
	odev.v.horiz = 2.*180./PI * atan(0.5/VIEWDIST*pwidth*odev.hres);
	odev.v.vert = 2.*180./PI * atan(0.5/VIEWDIST*pheight*odev.vres);
	odev.ifd = ConnectionNumber(ourdisplay);
					/* allocate our leaf pile */
	qtDepthEps = 0.02;
	n = odev.hres < odev.vres ? odev.hres : odev.vres;
	qtMinNodesiz = 1;
	if (!qtAllocLeaves(n*n/(qtMinNodesiz*qtMinNodesiz)))
		error(SYSTEM, "insufficient memory for leaf storage");
}


dev_close()			/* close our display */
{
	freepixels();
	XFreeGC(ourdisplay, ourgc);
	XDestroyWindow(ourdisplay, gwind);
	gwind = 0;
	ourgc = 0;
	XCloseDisplay(ourdisplay);
	ourdisplay = NULL;
	qtFreeLeaves();
	tmDone(NULL);
	odev.v.type = 0;
	odev.hres = odev.vres = 0;
	odev.ifd = -1;
}


dev_view(nv)			/* assign new driver view */
VIEW	*nv;
{
	if (nv != &odev.v)
		copystruct(&odev.v, nv);
	qtReplant();
}


int
dev_input()			/* get X11 input */
{
	inpresflags = 0;
	do
		getevent();

	while (XQLength(ourdisplay) > 0);

	return(inpresflags);
}


dev_paintr(rgb, xmin, ymin, xmax, ymax)		/* fill a rectangle */
BYTE	rgb[3];
int  xmin, ymin, xmax, ymax;
{
	unsigned long  pixel;

	if (!mapped)
		return;
	if (ncolors > 0)
		pixel = pixval[get_pixel(rgb, xnewcolr)];
	else
		pixel = true_pixel(rgb);
	XSetForeground(ourdisplay, ourgc, pixel);
	XFillRectangle(ourdisplay, gwind, 
		ourgc, xmin, odev.vres-ymax, xmax-xmin, ymax-ymin);
}


int
dev_flush()			/* flush output */
{
	qtUpdate();
	return(XPending(ourdisplay));
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
		if (XAllocColorCells(ourdisplay,ourmap,0,NULL,0,pixval,ncolors))
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
	free((char *)pixval);
	pixval = NULL;
	ncolors = 0;
	if (ourmap != DefaultColormap(ourdisplay,ourscreen))
		XFreeColormap(ourdisplay, ourmap);
	ourmap = 0;
}


static unsigned long
true_pixel(rgb)			/* return true pixel value for color */
register BYTE	rgb[3];
{
	register unsigned long  rval;

	rval = ourvinfo.red_mask*rgb[RED]/255 & ourvinfo.red_mask;
	rval |= ourvinfo.green_mask*rgb[GRN]/255 & ourvinfo.green_mask;
	rval |= ourvinfo.blue_mask*rgb[BLU]/255 & ourvinfo.blue_mask;
	return(rval);
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
		mapped = 0;
		freepixels();
		break;
	case MapNotify:
		if (ourvinfo.class == PseudoColor ||
				ourvinfo.class == GrayScale) {
			if (getpixels() == 0)
				error(SYSTEM, "cannot allocate colors\n");
			new_ctab(ncolors);
		}
		mapped = 1;
		break;
	case Expose:
		fixwindow(levptr(XExposeEvent));
		break;
	case KeyPress:
		getkey(levptr(XKeyPressedEvent));
		break;
	case ButtonPress:
		/* getmove(levptr(XButtonPressedEvent)); */
		break;
	}
}


static
getkey(ekey)				/* get input key */
register XKeyPressedEvent  *ekey;
{
	int  n;
	char	buf[8];

	n = XLookupString(ekey, buf, sizeof(buf), NULL, NULL);
	if (n != 1)
		return;
	switch (buf[0]) {
	case 'h':			/* turn on height motion lock */
		heightlocked = 1;
		return;
	case 'H':			/* turn off height motion lock */
		heightlocked = 0;
		return;
	case 'Q':
	case 'q':			/* quit the program */
		inpresflags |= DEV_SHUTDOWN;
		return;
	default:
		XBell(ourdisplay, 0);
		return;
	}
}


static
fixwindow(eexp)				/* repair damage to window */
register XExposeEvent  *eexp;
{
	if (odev.hres == 0 || odev.vres == 0) {	/* first exposure */
eputs("Resizing window in fixwindow\n");
		odev.hres = eexp->width;
		odev.vres = eexp->height;
		inpresflags |= DEV_NEWSIZE;
	}
	qtRedraw(eexp->x, odev.vres - eexp->y - eexp->height,
			eexp->x + eexp->width, odev.vres - eexp->y);
}


static
resizewindow(ersz)			/* resize window */
register XConfigureEvent  *ersz;
{
	if (ersz->width == odev.hres && ersz->height == odev.vres)
		return;

	if (odev.hres != 0 && odev.vres != 0) {
		odev.v.horiz = 2.*180./PI * atan(
			tan(PI/180./2.*odev.v.horiz) * ersz->width/odev.hres );
		odev.v.vert = 2.*180./PI * atan(
			tan(PI/180./2.*odev.v.vert) * ersz->height/odev.vres );
		inpresflags |= DEV_NEWVIEW;
	}
	odev.hres = ersz->width;
	odev.vres = ersz->height;

	inpresflags |= DEV_NEWSIZE;
}
