#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  Display an image and watch the rays get traced.
 *
 *	9/21/90	Greg Ward
 */

#include "standard.h"
#include "view.h"
#include <X11/Xlib.h>

#define MAXDEPTH	32		/* ridiculous ray tree depth */

char	rtcom[] = "rtrace -h -otp -fa -x 1";
char	xicom[] = "x11image -f";

VIEW	ourview = STDVIEW;		/* view for picture */
int	xres, yres;

char	*progname;			/* program name */

FILE	*pin;				/* input stream */

Display	*theDisplay = NULL;		/* connection to server */

struct node {				/* ray tree node */
	double	ipt[2];
	struct node	*sister;
	struct node	*daughter;
};

#define newnode()	(struct node *)calloc(1, sizeof(struct node))

int	slow = 0;		/* slow trace? */


main(argc, argv)		/* takes both the octree and the image */
int	argc;
char	*argv[];
{
	int	i;
	char	combuf[256];
	Cursor	curs;

	progname = argv[0];
	for (i = 1; i < argc-2; i++)
		if (!strcmp(argv[i], "-s"))
			slow++;
		else
			break;
	if (i > argc-2) {
		fprintf(stderr, "Usage: %s [-s] [rtrace args] octree picture\n",
				argv[0]);
		exit(1);
	}
					/* get the viewing parameters */
	if (viewfile(argv[argc-1], &ourview, &xres, &yres) <= 0 ||
			setview(&ourview) != NULL) {
		fprintf(stderr, "%s: cannot get view from \"%s\"\n",
				argv[0], argv[argc-1]);
		exit(1);
	}
					/* open the display */
	if ((theDisplay = XOpenDisplay(NULL)) == NULL) {
		fprintf(stderr,
		"%s: cannot open display; DISPLAY variable set?\n",
				argv[0]);
		exit(1);
	}
					/* build input command */
	sprintf(combuf, "%s %s | %s", xicom, argv[argc-1], rtcom);
	for ( ; i < argc-1; i++) {
		strcat(combuf, " ");
		strcat(combuf, argv[i]);
	}
					/* start the damn thing */
	if ((pin = popen(combuf, "r")) == NULL)
		exit(1);
					/* loop on input */
	mainloop();

	exit(0);
}


mainloop()				/* get and process input */
{
	static struct node	*sis[MAXDEPTH];
	register struct node	*newp;
	char	buf[128];
	int	level;
	register int	i;

	level = 0;
	while (fgets(buf, sizeof(buf), pin) != NULL) {
		if ((newp = newnode()) == NULL) {
			fprintf(stderr, "%s: memory error\n", progname);
			return;
		}
		for (i = 0; buf[i] == '\t'; i++)
			;
		if (strtoipt(newp->ipt, buf+i) < 0) {
			fprintf(stderr, "%s: bad read\n", progname);
			return;
		}
		newp->sister = sis[i];
		sis[i] = newp;
		if (i < level) {
			newp->daughter = sis[level];
			sis[level] = NULL;
		}
		level = i;
		if (i == 0) {
			setvec(sis[0]->ipt);
			tracerays(sis[0]);
			freetree(sis[0]);
			sis[0] = NULL;
			if (!slow)
				XFlush(theDisplay);
		}
	}
}


freetree(tp)				/* free a trace tree */
struct node	*tp;
{
	register struct node	*kid;

	for (kid = tp->daughter; kid != NULL; kid = kid->sister)
		freetree(kid);
	free((char *)tp);
}


tracerays(tp)				/* trace a ray tree */
struct node	*tp;
{
	register struct node	*kid;

	for (kid = tp->daughter; kid != NULL; kid = kid->sister) {
		vector(tp->ipt, kid->ipt);
		tracerays(kid);
	}
}


strtoipt(ipt, str)		/* convert string x y z to image point */
double	ipt[2];
char	*str;
{
	FVECT	pt;

	if (sscanf(str, "%lf %lf %lf", &pt[0], &pt[1], &pt[2]) != 3)
		return(-1);
	viewpixel(&ipt[0], &ipt[1], NULL, &ourview, pt);
	return(0);
}


#define rwind		RootWindow(theDisplay,ourScreen)
#define ourScreen	DefaultScreen(theDisplay)

GC	vecGC = 0;
Window	gwind = 0;
int	xoff, yoff;


setvec(ipt)			/* set up vector drawing for pick */
double	ipt[2];
{
	XWindowAttributes	wa;
	XColor	xc;
	XGCValues	gcv;
	int	pm, rx, ry, wx, wy, rw, cw;
					/* compute pointer location */
	if (gwind == 0) {
		XQueryPointer(theDisplay, rwind, &rw, &gwind,
				&rx, &ry, &wx, &wy, &pm);
	}
	XQueryPointer(theDisplay, gwind, &rw, &cw,
			&rx, &ry, &wx, &wy, &pm);
	xoff = wx - ipt[0]*xres;
	yoff = wy - (1.-ipt[1])*yres;
					/* set graphics context */
	if (vecGC == 0) {
		XGetWindowAttributes(theDisplay, gwind, &wa);
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
}


vector(ip1, ip2)		/* draw a vector */
double	ip1[2], ip2[2];
{
	XDrawLine(theDisplay, gwind, vecGC,
			(int)(ip1[0]*xres)+xoff, (int)((1.-ip1[1])*yres)+yoff,
			(int)(ip2[0]*xres)+xoff, (int)((1.-ip2[1])*yres)+yoff);
	if (slow) {
		XFlush(theDisplay);
		sleep(1);
	}
}
