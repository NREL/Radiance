/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  Circle sources in a displayed image.
 *
 *	18 Mar 1991	Greg Ward
 */

#include "standard.h"
#include "view.h"
#include <signal.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifndef BSD
#define vfork	fork
#endif

#define XIM		"ximage"

#define NSEG		30		/* number of segments per circle */

VIEW	ourview = STDVIEW;		/* view for picture */
int	xres, yres;			/* picture resolution */

char	*progname;			/* program name */

Display	*theDisplay = NULL;		/* connection to server */

#define rwind		RootWindow(theDisplay,ourScreen)
#define ourScreen	DefaultScreen(theDisplay)

GC	vecGC;
Window	gwind;
Cursor	pickcursor;


main(argc, argv)
int	argc;
char	*argv[];
{
	char	*windowname;
	FILE	*fp;

	progname = argv[0];
	if (argc > 2 && !strcmp(argv[1], "-n")) {
		windowname = argv[2];
		argv += 2;
		argc -= 2;
	} else
		windowname = argv[1];
	if (argc < 2 || argc > 3) {
		fprintf(stderr,
			"Usage: %s [-n windowname] picture [glaresrc]\n",
				progname);
		exit(1);
	}
	init(argv[1], windowname);
	if (argc < 3)
		fp = stdin;
	else if ((fp = fopen(argv[2], "r")) == NULL) {
		fprintf(stderr, "%s: cannot open \"%s\"\n",
				progname, argv[2]);
		exit(1);
	}
	circle_sources(fp);
	exit(0);
}


init(pname, wname)		/* get view and find window */
char	*pname, *wname;
{
	extern Window	xfindwind();
	XWindowAttributes	wa;
	XColor	xc;
	XGCValues	gcv;
					/* get the viewing parameters */
	if (viewfile(pname, &ourview, &xres, &yres) <= 0 ||
			setview(&ourview) != NULL) {
		fprintf(stderr, "%s: cannot get view from \"%s\"\n",
				progname, pname);
		exit(1);
	}
					/* open the display */
	if ((theDisplay = XOpenDisplay(NULL)) == NULL) {
		fprintf(stderr,
		"%s: cannot open display; DISPLAY variable set?\n",
				progname);
		exit(1);
	}
					/* find our window */
	gwind = xfindwind(theDisplay, rwind, wname, 2);
	if (gwind == None) {
		if (wname != pname) {
			fprintf(stderr, "%s: cannot find \"%s\" window\n",
					progname, wname);
			exit(2);
		}
					/* start ximage */
		if (vfork() == 0) {
			execlp(XIM, XIM, pname, 0);
			perror(XIM);
			fprintf(stderr, "%s: cannot start %s\n",
					progname, XIM);
			kill(getppid(), SIGPIPE);
			_exit(1);
		}
		do
			sleep(8);
		while ((gwind=xfindwind(theDisplay,rwind,pname,2)) == None);
	} else {
		XMapRaised(theDisplay, gwind);
		XFlush(theDisplay);
		sleep(4);
	}
	XGetWindowAttributes(theDisplay, gwind, &wa);
	if (wa.width != xres || wa.height != yres) {
		fprintf(stderr,
		"%s: warning -- window seems to be the wrong size!\n",
				progname);
		xres = wa.width;
		yres = wa.height;
	}
					/* set graphics context */
	xc.red = 65535; xc.green = 0; xc.blue = 0;
	xc.flags = DoRed|DoGreen|DoBlue;
	if (XAllocColor(theDisplay, wa.colormap, &xc)) {
		gcv.foreground = xc.pixel;
		vecGC = XCreateGC(theDisplay,gwind,GCForeground,&gcv);
	} else {
		gcv.function = GXinvert;
		vecGC = XCreateGC(theDisplay,gwind,GCFunction,&gcv);
	}
}


circle_sources(fp)		/* circle sources listed in fp */
FILE	*fp;
{
	char	linbuf[256];
	int	reading = 0;
	FVECT	dir;
	double	dom;

	while (fgets(linbuf, sizeof(linbuf), fp) != NULL)
		if (reading) {
			if (!strncmp(linbuf, "END", 3)) {
				XFlush(theDisplay);
				return;
			}
			if (sscanf(linbuf, "%lf %lf %lf %lf",
					&dir[0], &dir[1], &dir[2],
					&dom) != 4)
				break;
			circle(dir, dom);
		} else if (!strcmp(linbuf, "BEGIN glare source\n"))
			reading++;

	fprintf(stderr, "%s: error reading glare sources\n", progname);
	exit(1);
}


circle(dir, dom)		/* indicate a solid angle on image */
FVECT	dir;
double	dom;
{
	FVECT	start, cur;
	XPoint	pt[NSEG+1];
	double	px, py, pz;
	register int	i;

	fcross(cur, dir, ourview.vup);
	if (normalize(cur) == 0.0)
		goto fail;
	spinvector(start, dir, cur, acos(1.-dom/(2.*PI)));
	for (i = 0; i <= NSEG; i++) {
		spinvector(cur, start, dir, 2.*PI*i/NSEG);
		cur[0] += ourview.vp[0];
		cur[1] += ourview.vp[1];
		cur[2] += ourview.vp[2];
		viewpixel(&px, &py, &pz, &ourview, cur);
		if (pz <= 0.0)
			goto fail;
		pt[i].x = px*xres;
		pt[i].y = yres-1 - (int)(py*yres);
	}
	XDrawLines(theDisplay, gwind, vecGC, pt, NSEG+1, CoordModeOrigin);
	return;
fail:
	fprintf(stderr, "%s: cannot draw source at (%f,%f,%f)\n",
			progname, dir[0], dir[1], dir[2]);
}
