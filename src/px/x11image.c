/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  x11image.c - driver for X-windows
 *
 *     3/1/90
 */

/*
 *  Modified for X11
 *
 *  January 1990
 *
 *  Anat Grynberg  and Greg Ward
 */


#include  "standard.h"

#include  <X11/Xlib.h>
#include  <X11/cursorfont.h>
#include  <X11/Xutil.h>

#include  "color.h"
#include  "view.h"
#include  "pic.h"
#include  "x11raster.h"
#include  "random.h"
#include  "resolu.h"

#define  FONTNAME	"8x13"		/* text font we'll use */

#define  CTRL(c)	((c)-'@')

#define  BORWIDTH	5		/* border width */

#define  ICONSIZ	(8*10)		/* maximum icon dimension (even 8) */

#define  ourscreen	DefaultScreen(thedisplay)
#define  ourroot	RootWindow(thedisplay,ourscreen)

#define  revline(x0,y0,x1,y1)	XDrawLine(thedisplay,wind,revgc,x0,y0,x1,y1)

#define  redraw(x,y,w,h) patch_raster(wind,(x)-xoff,(y)-yoff,x,y,w,h,ourras)

double  gamcor = 2.2;			/* gamma correction */

int  dither = 1;			/* dither colors? */
int  fast = 0;				/* keep picture in Pixmap? */

char	*dispname = NULL;		/* our display name */

Window  wind = 0;			/* our output window */
unsigned long  ourblack=1, ourwhite=0;	/* black and white for this visual */
Font  fontid;				/* our font */

int  maxcolors = 0;			/* maximum colors */
int  greyscale = 0;			/* in grey */

int  scale = 0;				/* scalefactor; power of two */

int  xoff = 0;				/* x image offset */
int  yoff = 0;				/* y image offset */

VIEW  ourview = STDVIEW;		/* image view parameters */
int  gotview = 0;			/* got parameters from file */

COLR  *scanline;			/* scan line buffer */

RESOLU  inpres;				/* input resolution and ordering */
int  xmax, ymax;			/* picture dimensions */
int  width, height;			/* window size */
char  *fname = NULL;			/* input file name */
FILE  *fin = stdin;			/* input file */
long  *scanpos = NULL;			/* scan line positions in file */
int  cury = 0;				/* current scan location */

double  exposure = 1.0;			/* exposure compensation used */

int  wrongformat = 0;			/* input in another format? */

GC	ourgc;				/* standard graphics context */
GC	revgc;				/* graphics context with GXinvert */

int		*ourrank;		/* our visual class ranking */
XVisualInfo	ourvis;			/* our visual */
XRASTER		*ourras;		/* our stored image */
unsigned char	*ourdata;		/* our image data */

struct {
	int  xmin, ymin, xsiz, ysiz;
}  box = {0, 0, 0, 0};			/* current box */

char  *geometry = NULL;			/* geometry specification */

char  icondata[ICONSIZ*ICONSIZ/8];	/* icon bitmap data */
int  iconwidth = 0, iconheight = 0;

char  *progname;

char  errmsg[128];

extern long  ftell();

extern char  *malloc(), *calloc();

extern double  pow(), log();

Display  *thedisplay;

main(argc, argv)
int  argc;
char  *argv[];
{
	int  headline();
	int  i;
	
	progname = argv[0];

	for (i = 1; i < argc; i++)
		if (argv[i][0] == '-')
			switch (argv[i][1]) {
			case 'c':
				maxcolors = atoi(argv[++i]);
				break;
			case 'b':
				greyscale = !greyscale;
				break;
			case 'm':
				maxcolors = 2;
				break;
			case 'd':
				if (argv[i][2] == 'i')
					dispname = argv[++i];
				else
					dither = !dither;
				break;
			case 'f':
				fast = !fast;
				break;
			case 'e':
				if (argv[i+1][0] != '+' && argv[i+1][0] != '-')
					goto userr;
				scale = atoi(argv[++i]);
				break;
			case 'g':
				if (argv[i][2] == 'e')
					geometry = argv[++i];
				else
					gamcor = atof(argv[++i]);
				break;
			default:
				goto userr;
			}
		else if (argv[i][0] == '=')
			geometry = argv[i];
		else
			break;

	if (i == argc-1) {
		fname = argv[i];
		fin = fopen(fname, "r");
		if (fin == NULL) {
			sprintf(errmsg, "cannot open file \"%s\"", fname);
			quiterr(errmsg);
		}
	} else if (i != argc)
		goto userr;
				/* get header */
	getheader(fin, headline, NULL);
				/* get picture dimensions */
	if (wrongformat || !fgetsresolu(&inpres, fin))
		quiterr("bad picture format");
	xmax = scanlen(&inpres);
	ymax = numscans(&inpres);
				/* set view parameters */
	if (gotview && setview(&ourview) != NULL)
		gotview = 0;
	if ((scanline = (COLR *)malloc(xmax*sizeof(COLR))) == NULL)
		quiterr("out of memory");

	init();			/* get file and open window */

	for ( ; ; )
		getevent();		/* main loop */
userr:
	fprintf(stderr,
	"Usage: %s [-display disp][-geometry spec][-b][-m][-d][-f][-c ncolors][-e +/-stops] file\n",
			progname);
	quit(1);
}


headline(s)		/* get relevant info from header */
char  *s;
{
	char  fmt[32];

	if (isexpos(s))
		exposure *= exposval(s);
	else if (isformat(s)) {
		formatval(fmt, s);
		wrongformat = strcmp(fmt, COLRFMT);
	} else if (isview(s) && sscanview(&ourview, s) > 0)
		gotview++;
}


init()			/* get data and open window */
{
	XWMHints	ourxwmhints;
	XSetWindowAttributes	ourwinattr;
	XSizeHints	oursizhints;
	register int	i;
	
	if (fname != NULL) {
		scanpos = (long *)malloc(ymax*sizeof(long));
		if (scanpos == NULL)
			goto memerr;
		for (i = 0; i < ymax; i++)
			scanpos[i] = -1;
	}
	if ((thedisplay = XOpenDisplay(dispname)) == NULL)
		quiterr("cannot open display");
				/* get best visual for default screen */
	getbestvis();
				/* store image */
	getras();
				/* open window */
	ourwinattr.border_pixel = ourblack;
	ourwinattr.background_pixel = ourwhite;
	ourwinattr.colormap = XCreateColormap(thedisplay, ourroot,
			ourvis.visual, AllocNone);
	wind = XCreateWindow(thedisplay, ourroot, 0, 0, xmax, ymax, BORWIDTH,
			ourvis.depth, InputOutput, ourvis.visual, 
			CWBackPixel|CWBorderPixel|CWColormap, &ourwinattr);
	if (wind == 0)
		quiterr("cannot create window");
	width = xmax;
	height = ymax;
	ourgc = XCreateGC(thedisplay, wind, 0, 0);
	XSetState(thedisplay, ourgc, ourblack, ourwhite, GXcopy, AllPlanes);
	revgc = XCreateGC(thedisplay, wind, 0, 0);
	XSetFunction(thedisplay, revgc, GXinvert);
	fontid = XLoadFont(thedisplay, FONTNAME);
	if (fontid == 0)
		quiterr("cannot get font");
	XSetFont(thedisplay, ourgc, fontid);
	XDefineCursor(thedisplay, wind, XCreateFontCursor(thedisplay, 
			XC_diamond_cross));
	XStoreName(thedisplay, wind, fname == NULL ? progname : fname);
	if (geometry != NULL) {
		bzero((char *)&oursizhints, sizeof(oursizhints));
		i = XParseGeometry(geometry, &oursizhints.x, &oursizhints.y,
				(unsigned *)&oursizhints.width,
				(unsigned *)&oursizhints.height);
		if ((i&(WidthValue|HeightValue)) == (WidthValue|HeightValue))
			oursizhints.flags |= USSize;
		else {
			oursizhints.width = xmax;
			oursizhints.height = ymax;
			oursizhints.flags |= PSize;
		}
		if ((i&(XValue|YValue)) == (XValue|YValue)) {
			oursizhints.flags |= USPosition;
			if (i & XNegative)
				oursizhints.x += DisplayWidth(thedisplay,
				ourscreen)-1-oursizhints.width-2*BORWIDTH;
			if (i & YNegative)
				oursizhints.y += DisplayHeight(thedisplay,
				ourscreen)-1-oursizhints.height-2*BORWIDTH;
		}
		XSetNormalHints(thedisplay, wind, &oursizhints);
	}
	ourxwmhints.flags = InputHint|IconPixmapHint;
	ourxwmhints.input = True;
	ourxwmhints.icon_pixmap = XCreateBitmapFromData(thedisplay,
			wind, icondata, iconwidth, iconheight);
	XSetWMHints(thedisplay, wind, &ourxwmhints);
	XSelectInput(thedisplay, wind, ButtonPressMask|ButtonReleaseMask
			|ButtonMotionMask|StructureNotifyMask
			|KeyPressMask|ExposureMask);
	XMapWindow(thedisplay, wind);
	return;
memerr:
	quiterr("out of memory");
} /* end of init */


quiterr(err)		/* print message and exit */
char  *err;
{
	if (err != NULL) {
		fprintf(stderr, "%s: %s\n", progname, err);
		exit(1);
	}
	exit(0);
}


eputs(s)
char	*s;
{
	fputs(s, stderr);
}


quit(code)
int  code;
{
	exit(code);
}


static int
viscmp(v1,v2)		/* compare visual to see which is better, descending */
register XVisualInfo	*v1, *v2;
{
	int	bad1 = 0, bad2 = 0;
	register int  *rp;

	if (v1->class == v2->class) {
		if (v1->class == TrueColor || v1->class == DirectColor) {
					/* prefer 24-bit to 32-bit */
			if (v1->depth == 24 && v2->depth == 32)
				return(-1);
			if (v1->depth == 32 && v2->depth == 24)
				return(1);
			return(0);
		}
					/* don't be too greedy */
		if (maxcolors <= 1<<v1->depth && maxcolors <= 1<<v2->depth)
			return(v1->depth - v2->depth);
		return(v2->depth - v1->depth);
	}
					/* prefer Pseudo when < 24-bit */
	if ((v1->class == TrueColor || v1->class == DirectColor) &&
			v1->depth < 24)
		bad1 = 1;
	if ((v2->class == TrueColor || v2->class == DirectColor) &&
			v2->depth < 24)
		bad2 = -1;
	if (bad1 | bad2)
		return(bad1+bad2);
					/* otherwise, use class ranking */
	for (rp = ourrank; *rp != -1; rp++) {
		if (v1->class == *rp)
			return(-1);
		if (v2->class == *rp)
			return(1);
	}
	return(0);
}


getbestvis()			/* get the best visual for this screen */
{
#ifdef DEBUG
static char  vistype[][12] = {
		"StaticGray",
		"GrayScale",
		"StaticColor",
		"PseudoColor",
		"TrueColor",
		"DirectColor"
};
#endif
	static int	rankings[3][6] = {
		{TrueColor,DirectColor,PseudoColor,GrayScale,StaticGray,-1},
		{PseudoColor,GrayScale,StaticGray,-1},
		{PseudoColor,GrayScale,StaticGray,-1}
	};
	XVisualInfo	*xvi;
	int	vismatched;
	register int	i, j;

	if (greyscale) {
		ourrank = rankings[2];
		if (maxcolors < 2) maxcolors = 256;
	} else if (maxcolors >= 2 && maxcolors <= 256)
		ourrank = rankings[1];
	else {
		ourrank = rankings[0];
		maxcolors = 256;
	}
					/* find best visual */
	ourvis.screen = ourscreen;
	xvi = XGetVisualInfo(thedisplay,VisualScreenMask,&ourvis,&vismatched);
	if (xvi == NULL)
		quiterr("no visuals for this screen!");
#ifdef DEBUG
	fprintf(stderr, "Supported visuals:\n");
	for (i = 0; i < vismatched; i++)
		fprintf(stderr, "\ttype %s, depth %d\n",
				vistype[xvi[i].class], xvi[i].depth);
#endif
	for (i = 0, j = 1; j < vismatched; j++)
		if (viscmp(&xvi[i],&xvi[j]) > 0)
			i = j;
					/* compare to least acceptable */
	for (j = 0; ourrank[j++] != -1; )
		;
	ourvis.class = ourrank[--j];
	ourvis.depth = 1;
	if (viscmp(&xvi[i],&ourvis) > 0)
		quiterr("inadequate visuals on this screen");
					/* OK, we'll use it */
	copystruct(&ourvis, &xvi[i]);
#ifdef DEBUG
	fprintf(stderr, "Selected visual type %s, depth %d\n",
			vistype[ourvis.class], ourvis.depth);
#endif
					/* make appropriate adjustments */
	if (ourvis.class == GrayScale || ourvis.class == StaticGray)
		greyscale = 1;
	if (ourvis.depth <= 8 && ourvis.colormap_size < maxcolors)
		maxcolors = ourvis.colormap_size;
	if (maxcolors > 4)
		maxcolors -= 2;
	if (ourvis.class == StaticGray) {
		ourblack = 0;
		ourwhite = 255;
	} else if (ourvis.class == PseudoColor) {
		ourblack = BlackPixel(thedisplay,ourscreen);
		ourwhite = WhitePixel(thedisplay,ourscreen);
	} else {
		ourblack = 0;
		ourwhite = ~0;
	}
	XFree((char *)xvi);
}


getras()				/* get raster file */
{
	colormap	ourmap;
	XVisualInfo	vinfo;

	if (maxcolors <= 2) {		/* monochrome */
		ourdata = (unsigned char *)malloc(ymax*((xmax+7)/8));
		if (ourdata == NULL)
			goto fail;
		ourras = make_raster(thedisplay, &ourvis, 1, ourdata,
				xmax, ymax, 8);
		if (ourras == NULL)
			goto fail;
		getmono();
	} else if (ourvis.class == TrueColor || ourvis.class == DirectColor) {
		ourdata = (unsigned char *)malloc(4*xmax*ymax);
		if (ourdata == NULL)
			goto fail;
		ourras = make_raster(thedisplay, &ourvis, 32,
				ourdata, xmax, ymax, 32);
		if (ourras == NULL)
			goto fail;
		getfull();
	} else {
		ourdata = (unsigned char *)malloc(xmax*ymax);
		if (ourdata == NULL)
			goto fail;
		ourras = make_raster(thedisplay, &ourvis, 8, ourdata,
				xmax, ymax, 8);
		if (ourras == NULL)
			goto fail;
		if (ourvis.class == StaticGray)
			getgrey();
		else {
			if (greyscale)
				biq(dither,maxcolors,1,ourmap);
			else
				ciq(dither,maxcolors,1,ourmap);
			if (init_rcolors(ourras, ourmap[0],
					ourmap[1], ourmap[2]) == 0)
				goto fail;
		}
	}
		return;
fail:
	quiterr("could not create raster image");
}


getevent()				/* process the next event */
{
	union {
		XEvent  u;
		XConfigureEvent  c;
		XExposeEvent  e;
		XButtonPressedEvent  b;
		XKeyPressedEvent  k;
	} e;

	XNextEvent(thedisplay, &e.u);
	switch (e.u.type) {
	case KeyPress:
		docom(&e.k);
		break;
	case ConfigureNotify:
		width = e.c.width;
		height = e.c.height;
		break;
	case MapNotify:
		map_rcolors(ourras, wind);
		if (fast)
			make_rpixmap(ourras, wind);
		break;
	case UnmapNotify:
		if (!fast)
			unmap_rcolors(ourras);
		break;
	case Expose:
		redraw(e.e.x, e.e.y, e.e.width, e.e.height);
		break;
	case ButtonPress:
		if (e.b.state & (ShiftMask|ControlMask))
			moveimage(&e.b);
		else
			getbox(&e.b);
		break;
	}
}


docom(ekey)					/* execute command */
XKeyPressedEvent  *ekey;
{
	char  buf[80];
	COLOR  cval;
	XColor  cvx;
	int  com, n;
	double  comp;
	FLOAT  hv[2];
	FVECT  rorg, rdir;

	n = XLookupString(ekey, buf, sizeof(buf), NULL, NULL); 
	if (n == 0)
		return(0);
	com = buf[0];
	switch (com) {			/* interpret command */
	case 'q':
	case CTRL('D'):				/* quit */
		quit(0);
	case '\n':
	case '\r':
	case 'l':
	case 'c':				/* value */
		if (avgbox(cval) == -1)
			return(-1);
		switch (com) {
		case '\n':
		case '\r':				/* radiance */
			sprintf(buf, "%.3f", intens(cval)/exposure);
			break;
		case 'l':				/* luminance */
			sprintf(buf, "%.0fL", luminance(cval)/exposure);
			break;
		case 'c':				/* color */
			comp = pow(2.0, (double)scale);
			sprintf(buf, "(%.2f,%.2f,%.2f)",
					colval(cval,RED)*comp,
					colval(cval,GRN)*comp,
					colval(cval,BLU)*comp);
			break;
		}
		XDrawImageString(thedisplay, wind, ourgc,
				box.xmin, box.ymin+box.ysiz, buf, strlen(buf)); 
		return(0);
	case 'i':				/* identify (contour) */
		if (ourras->pixels == NULL)
			return(-1);
		n = ourdata[ekey->x-xoff+xmax*(ekey->y-yoff)];
		n = ourras->pmap[n];
		cvx.pixel = ourras->cdefs[n].pixel;
		cvx.red = random() & 65535;
		cvx.green = random() & 65535;
		cvx.blue = random() & 65535;
		cvx.flags = DoRed|DoGreen|DoBlue;
		XStoreColor(thedisplay, ourras->cmap, &cvx);
		return(0);
	case 'p':				/* position */
		pix2loc(hv, &inpres, ekey->x-xoff, ekey->y-yoff);
		sprintf(buf, "(%d,%d)", (int)(hv[0]*inpres.xr),
				(int)(hv[1]*inpres.yr));
		XDrawImageString(thedisplay, wind, ourgc, ekey->x, ekey->y,
					buf, strlen(buf));
		return(0);
	case 't':				/* trace */
		if (!gotview) {
			XBell(thedisplay, 0);
			return(-1);
		}
		pix2loc(hv, &inpres, ekey->x-xoff, ekey->y-yoff);
		if (viewray(rorg, rdir, &ourview, hv[0], hv[1]) < 0)
			return(-1);
		printf("%e %e %e ", rorg[0], rorg[1], rorg[2]);
		printf("%e %e %e\n", rdir[0], rdir[1], rdir[2]);
		fflush(stdout);
		return(0);
	case '=':				/* adjust exposure */
		if (avgbox(cval) == -1)
			return(-1);
		n = log(.5/bright(cval))/.69315 - scale;	/* truncate */
		if (n == 0)
			return(0);
		scale_rcolors(ourras, pow(2.0, (double)n));
		scale += n;
		sprintf(buf, "%+d", scale);
		XDrawImageString(thedisplay, wind, ourgc,
				box.xmin, box.ymin+box.ysiz, buf, strlen(buf));
		XFlush(thedisplay);
		free(ourdata);
		free_raster(ourras);
		getras();
	/* fall through */
	case CTRL('R'):				/* redraw */
	case CTRL('L'):
		unmap_rcolors(ourras);
		XClearWindow(thedisplay, wind);
		map_rcolors(ourras, wind);
		if (fast)
			make_rpixmap(ourras);
		redraw(0, 0, width, height);
		return(0);
	case ' ':				/* clear */
		redraw(box.xmin, box.ymin, box.xsiz, box.ysiz);
		return(0);
	default:
		XBell(thedisplay, 0);
		return(-1);
	}
}


moveimage(ebut)				/* shift the image */
XButtonPressedEvent  *ebut;
{
	union {
		XEvent  u;
		XButtonReleasedEvent  b;
		XPointerMovedEvent  m;
	}  e;
	int	mxo, myo;

	XMaskEvent(thedisplay, ButtonReleaseMask|ButtonMotionMask, &e.u);
	while (e.u.type == MotionNotify) {
		mxo = e.m.x;
		myo = e.m.y;
		revline(ebut->x, ebut->y, mxo, myo);
		revbox(xoff+mxo-ebut->x, yoff+myo-ebut->y,
				xoff+mxo-ebut->x+xmax, yoff+myo-ebut->y+ymax);
		XMaskEvent(thedisplay,ButtonReleaseMask|ButtonMotionMask,&e.u);
		revline(ebut->x, ebut->y, mxo, myo);
		revbox(xoff+mxo-ebut->x, yoff+myo-ebut->y,
				xoff+mxo-ebut->x+xmax, yoff+myo-ebut->y+ymax);
	}
	xoff += e.b.x - ebut->x;
	yoff += e.b.y - ebut->y;
	XClearWindow(thedisplay, wind);
	redraw(0, 0, width, height);
}


getbox(ebut)				/* get new box */
XButtonPressedEvent  *ebut;
{
	union {
		XEvent  u;
		XButtonReleasedEvent  b;
		XPointerMovedEvent  m;
	}  e;

	XMaskEvent(thedisplay, ButtonReleaseMask|ButtonMotionMask, &e.u);
	while (e.u.type == MotionNotify) {
		revbox(ebut->x, ebut->y, box.xmin = e.m.x, box.ymin = e.m.y);
		XMaskEvent(thedisplay,ButtonReleaseMask|ButtonMotionMask,&e.u);
		revbox(ebut->x, ebut->y, box.xmin, box.ymin);
	}
	box.xmin = e.b.x<0 ? 0 : (e.b.x>=width ? width-1 : e.b.x);
	box.ymin = e.b.y<0 ? 0 : (e.b.y>=height ? height-1 : e.b.y);
	if (box.xmin > ebut->x) {
		box.xsiz = box.xmin - ebut->x + 1;
		box.xmin = ebut->x;
	} else {
		box.xsiz = ebut->x - box.xmin + 1;
	}
	if (box.ymin > ebut->y) {
		box.ysiz = box.ymin - ebut->y + 1;
		box.ymin = ebut->y;
	} else {
		box.ysiz = ebut->y - box.ymin + 1;
	}
}


revbox(x0, y0, x1, y1)			/* draw box with reversed lines */
int  x0, y0, x1, y1;
{
	revline(x0, y0, x1, y0);
	revline(x0, y1, x1, y1);
	revline(x0, y0, x0, y1);
	revline(x1, y0, x1, y1);
}


avgbox(clr)				/* average color over current box */
COLOR  clr;
{
	static COLOR  lc;
	static int  ll, lr, lt, lb;
	int  left, right, top, bottom;
	int  y;
	double  d;
	COLOR  ctmp;
	register int  x;

	setcolor(clr, 0.0, 0.0, 0.0);
	left = box.xmin - xoff;
	right = left + box.xsiz;
	if (left < 0)
		left = 0;
	if (right > xmax)
		right = xmax;
	if (left >= right)
		return(-1);
	top = box.ymin - yoff;
	bottom = top + box.ysiz;
	if (top < 0)
		top = 0;
	if (bottom > ymax)
		bottom = ymax;
	if (top >= bottom)
		return(-1);
	if (left == ll && right == lr && top == lt && bottom == lb) {
		copycolor(clr, lc);
		return;
	}
	for (y = top; y < bottom; y++) {
		if (getscan(y) == -1)
			return(-1);
		for (x = left; x < right; x++) {
			colr_color(ctmp, scanline[x]);
			addcolor(clr, ctmp);
		}
	}
	d = 1.0/((right-left)*(bottom-top));
	scalecolor(clr, d);
	ll = left; lr = right; lt = top; lb = bottom;
	copycolor(lc, clr);
	return(0);
}


getmono()			/* get monochrome data */
{
	register unsigned char	*dp;
	register int	x, err;
	int	y, errp;
	short	*cerr;

	if ((cerr = (short *)calloc(xmax,sizeof(short))) == NULL)
		quiterr("out of memory in getmono");
	dp = ourdata - 1;
	for (y = 0; y < ymax; y++) {
		if (getscan(y) < 0)
			quiterr("seek error in getmono");
		normcolrs(scanline, xmax, scale);
		add2icon(y, scanline);
		err = 0;
		for (x = 0; x < xmax; x++) {
			if (!(x&7))
				*++dp = 0;
			errp = err;
			err += normbright(scanline[x]) + cerr[x];
			if (err > 127)
				err -= 255;
			else
				*dp |= 1<<(7-(x&07));
			err /= 3;
			cerr[x] = err + errp;
		}
	}
	free((char *)cerr);
}


add2icon(y, scan)		/* add a scanline to our icon data */
int  y;
COLR  *scan;
{
	static short  cerr[ICONSIZ];
	static int  ynext;
	static char  *dp;
	register int  err;
	register int	x, ti;
	int  errp;

	if (iconheight == 0) {		/* initialize */
		if (xmax <= ICONSIZ && ymax <= ICONSIZ) {
			iconwidth = xmax;
			iconheight = ymax;
		} else if (xmax > ymax) {
			iconwidth = ICONSIZ;
			iconheight = ICONSIZ*ymax/xmax;
			if (iconheight < 1)
				iconheight = 1;
		} else {
			iconwidth = ICONSIZ*xmax/ymax;
			if (iconwidth < 1)
				iconwidth = 1;
			iconheight = ICONSIZ;
		}
		ynext = 0;
		dp = icondata - 1;
	}
	if (y < ynext*ymax/iconheight)	/* skip this one */
		return;
	err = 0;
	for (x = 0; x < iconwidth; x++) {
		if (!(x&7))
			*++dp = 0;
		errp = err;
		ti = x*xmax/iconwidth;
		err += normbright(scan[ti]) + cerr[x];
		if (err > 127)
			err -= 255;
		else
			*dp |= 1<<(x&07);
		err /= 3;
		cerr[x] = err + errp;
	}
	ynext++;
}


getfull()			/* get full (24-bit) data */
{
	int	y;
	register unsigned long	*dp;
	register int	x;
					/* set gamma correction */
	setcolrgam(gamcor);
					/* read and convert file */
	dp = (unsigned long *)ourdata;
	for (y = 0; y < ymax; y++) {
		if (getscan(y) < 0)
			quiterr("seek error in getfull");
		if (scale)
			shiftcolrs(scanline, xmax, scale);
		colrs_gambs(scanline, xmax);
		add2icon(y, scanline);
		if (ourras->image->blue_mask & 1)
			for (x = 0; x < xmax; x++)
				*dp++ =	scanline[x][RED] << 16 |
					scanline[x][GRN] << 8 |
					scanline[x][BLU] ;
		else
			for (x = 0; x < xmax; x++)
				*dp++ =	scanline[x][RED] |
					scanline[x][GRN] << 8 |
					scanline[x][BLU] << 16 ;
	}
}


getgrey()			/* get greyscale data */
{
	int	y;
	register unsigned char	*dp;
	register int	x;
					/* set gamma correction */
	setcolrgam(gamcor);
					/* read and convert file */
	dp = ourdata;
	for (y = 0; y < ymax; y++) {
		if (getscan(y) < 0)
			quiterr("seek error in getfull");
		if (scale)
			shiftcolrs(scanline, xmax, scale);
		colrs_gambs(scanline, xmax);
		add2icon(y, scanline);
		if (ourvis.colormap_size < 256)
			for (x = 0; x < xmax; x++)
				*dp++ =	((long)normbright(scanline[x]) *
					ourvis.colormap_size + 128) >> 8;
		else
			for (x = 0; x < xmax; x++)
				*dp++ =	normbright(scanline[x]);
	}
}


scale_rcolors(xr, sf)			/* scale color map */
register XRASTER	*xr;
double	sf;
{
	register int	i;
	long	maxv;

	if (xr->pixels == NULL)
		return;

	sf = pow(sf, 1.0/gamcor);
	maxv = 65535/sf;

	for (i = xr->ncolors; i--; ) {
		xr->cdefs[i].red = xr->cdefs[i].red > maxv ?
				65535 :
				xr->cdefs[i].red * sf;
		xr->cdefs[i].green = xr->cdefs[i].green > maxv ?
				65535 :
				xr->cdefs[i].green * sf;
		xr->cdefs[i].blue = xr->cdefs[i].blue > maxv ?
				65535 :
				xr->cdefs[i].blue * sf;
	}
	XStoreColors(thedisplay, xr->cmap, xr->cdefs, xr->ncolors);
}


getscan(y)
int  y;
{
	if (y != cury) {
		if (scanpos == NULL || scanpos[y] == -1)
			return(-1);
		if (fseek(fin, scanpos[y], 0) == -1)
			quiterr("fseek error");
		cury = y;
	} else if (scanpos != NULL)
		scanpos[y] = ftell(fin);

	if (freadcolrs(scanline, xmax, fin) < 0)
		quiterr("read error");

	cury++;
	return(0);
}


picreadline3(y, l3)			/* read in 3-byte scanline */
int  y;
register rgbpixel  *l3;
{
	register int	i;
							/* read scanline */
	if (getscan(y) < 0)
		quiterr("cannot seek for picreadline");
							/* convert scanline */
	normcolrs(scanline, xmax, scale);
	add2icon(y, scanline);
	for (i = 0; i < xmax; i++) {
		l3[i].r = scanline[i][RED];
		l3[i].g = scanline[i][GRN];
		l3[i].b = scanline[i][BLU];
	}
}


picwriteline(y, l)		/* add 8-bit scanline to image */
int  y;
pixel  *l;
{
	bcopy((char *)l, (char *)ourdata+y*xmax, xmax);
}


picreadcm(map)			/* do gamma correction */
colormap  map;
{
	extern double  pow();
	register int  i, val;

	for (i = 0; i < 256; i++) {
		val = pow((i+0.5)/256.0, 1.0/gamcor) * 256.0;
		map[0][i] = map[1][i] = map[2][i] = val;
	}
}


picwritecm(map)			/* handled elsewhere */
colormap  map;
{
#if 0
	register int i;

	for (i = 0; i < 256; i++)
		printf("%d %d %d\n", map[0][i],map[1][i],map[2][i]);
#endif
}
