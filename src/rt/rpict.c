/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  rpict.c - routines and variables for picture generation.
 *
 *     8/14/85
 */

#include  "ray.h"

#ifdef BSD
#include  <sys/time.h>
#include  <sys/resource.h>
#endif

#include  "view.h"

#include  "random.h"

VIEW  ourview = STDVIEW(512);		/* view parameters */

int  psample = 4;			/* pixel sample size */
double  maxdiff = .05;			/* max. difference for interpolation */
double  dstrpix = 0.67;			/* square pixel distribution */

double  dstrsrc = 0.0;			/* square source distribution */
double  shadthresh = .05;		/* shadow threshold */
double  shadcert = .5;			/* shadow certainty */

int  maxdepth = 6;			/* maximum recursion depth */
double  minweight = 5e-3;		/* minimum ray weight */

COLOR  ambval = BLKCOLOR;		/* ambient value */
double  ambacc = 0.2;			/* ambient accuracy */
int  ambres = 32;			/* ambient resolution */
int  ambdiv = 128;			/* ambient divisions */
int  ambssamp = 0;			/* ambient super-samples */
int  ambounce = 0;			/* ambient bounces */
char  *amblist[128];			/* ambient include/exclude list */
int  ambincl = -1;			/* include == 1, exclude == 0 */

int  ralrm = 0;				/* seconds between reports */

double  pctdone = 0.0;			/* percentage done */

extern long  nrays;			/* number of rays traced */

#define  MAXDIV		32		/* maximum sample size */

#define  pixjitter()	(.5+dstrpix*(.5-frandom()))


quit(code)			/* quit program */
int  code;
{
	if (code || ralrm > 0)		/* report status */
		report();

	exit(code);
}


report()		/* report progress */
{
#ifdef BSD
	struct rusage  rubuf;
	double  t;

	getrusage(RUSAGE_SELF, &rubuf);
	t = (rubuf.ru_utime.tv_usec + rubuf.ru_stime.tv_usec) / 1e6;
	t += rubuf.ru_utime.tv_sec + rubuf.ru_stime.tv_sec;
	getrusage(RUSAGE_CHILDREN, &rubuf);
	t += (rubuf.ru_utime.tv_usec + rubuf.ru_stime.tv_usec) / 1e6;
	t += rubuf.ru_utime.tv_sec + rubuf.ru_stime.tv_sec;

	sprintf(errmsg, "%ld rays, %4.2f%% done after %5.4f CPU hours\n",
			nrays, pctdone, t/3600.0);
#else
	sprintf(errmsg, "%ld rays, %4.2f%% done\n", nrays, pctdone);
#endif
	eputs(errmsg);

	if (ralrm > 0)
		alarm(ralrm);
}


render(oldfile)				/* render the scene */
char  *oldfile;
{
	COLOR  *scanbar[MAXDIV+1];	/* scanline arrays of pixel values */
	int  ypos;			/* current scanline */
	int  xres, yres;		/* rendered x and y resolution */
	COLOR  *colptr;
	register int  i;
					/* set rendered resolution */
	xres = ourview.hresolu;
	yres = ourview.vresolu;
					/* adjust for sampling */
	if (psample <= 1)
		psample = 1;
	else {
		if (psample > MAXDIV)
			psample = MAXDIV;
					/* rendered resolution may be larger */
		xres += psample-1 - ((ourview.hresolu-2)%psample);
		yres += psample-1 - ((ourview.vresolu-2)%psample);
	}
					/* allocate scanlines */
	for (i = 0; i <= psample; i++) {
		scanbar[i] = (COLOR *)malloc(xres*sizeof(COLOR));
		if (scanbar[i] == NULL)
			error(SYSTEM, "out of memory in render");
	}
					/* write out boundaries */
	fputresolu(YMAJOR|YDECR, ourview.hresolu, ourview.vresolu, stdout);

	ypos = ourview.vresolu-1 - salvage(oldfile);	/* find top line */
	fillscanline(scanbar[0], xres, ypos, psample);	/* top scan */

	for (ypos -= psample; ypos > -psample; ypos -= psample) {
	
		pctdone = 100.0*(ourview.vresolu-ypos+psample)/ourview.vresolu;

		colptr = scanbar[psample];		/* move base to top */
		scanbar[psample] = scanbar[0];
		scanbar[0] = colptr;

		fillscanline(scanbar[0], xres, ypos, psample);	/* fill base */
	
		fillscanbar(scanbar, xres, ypos, psample);	/* fill bar */
		
		for (i = psample; (ypos>0) ? i > 0 : ypos+i >= 0; i--)
			if (fwritescan(scanbar[i], ourview.hresolu, stdout) < 0)
				goto writerr;
		if (fflush(stdout) == EOF)
			goto writerr;
	}
	pctdone = 100.0;
		
	for (i = 0; i <= psample; i++)
		free((char *)scanbar[i]);
	return;
writerr:
	error(SYSTEM, "write error in render");
}


fillscanline(scanline, xres, y, xstep)		/* fill scan line at y */
register COLOR  *scanline;
int  xres, y, xstep;
{
	int  b = xstep;
	register int  i;
	
	pixvalue(scanline[0], 0, y);

	for (i = xstep; i < xres; i += xstep) {
	
		pixvalue(scanline[i], i, y);
		
		b = fillsample(scanline+i-xstep, i-xstep, y, xstep, 0, b/2);
	}
}


fillscanbar(scanbar, xres, y, ysize)		/* fill interior */
register COLOR  *scanbar[];
int  xres, y, ysize;
{
	COLOR  vline[MAXDIV+1];
	int  b = ysize;
	register int  i, j;
	
	for (i = 0; i < xres; i++) {
		
		copycolor(vline[0], scanbar[0][i]);
		copycolor(vline[ysize], scanbar[ysize][i]);
		
		b = fillsample(vline, i, y, 0, ysize, b/2);
		
		for (j = 1; j < ysize; j++)
			copycolor(scanbar[j][i], vline[j]);
	}
}


int
fillsample(colline, x, y, xlen, ylen, b)	/* fill interior points */
register COLOR  *colline;
int  x, y;
int  xlen, ylen;
int  b;
{
	double  ratio;
	COLOR  ctmp;
	int  ncut;
	register int  len;
	
	if (xlen > 0)			/* x or y length is zero */
		len = xlen;
	else
		len = ylen;
		
	if (len <= 1)			/* limit recursion */
		return(0);
	
	if (b > 0 || bigdiff(colline[0], colline[len], maxdiff)) {
	
		pixvalue(colline[len>>1], x + (xlen>>1), y + (ylen>>1));
		ncut = 1;
		
	} else {					/* interpolate */
	
		copycolor(colline[len>>1], colline[len]);
		ratio = (double)(len>>1) / len;
		scalecolor(colline[len>>1], ratio);
		copycolor(ctmp, colline[0]);
		ratio = 1.0 - ratio;
		scalecolor(ctmp, ratio);
		addcolor(colline[len>>1], ctmp);
		ncut = 0;
	}
							/* recurse */
	ncut += fillsample(colline, x, y, xlen>>1, ylen>>1, (b-1)/2);
	
	ncut += fillsample(colline + (len>>1), x + (xlen>>1), y + (ylen>>1),
				xlen - (xlen>>1), ylen - (ylen>>1), b/2);

	return(ncut);
}


pixvalue(col, x, y)		/* compute pixel value */
COLOR  col;			/* returned color */
int  x, y;			/* pixel position */
{
	static RAY  thisray;	/* our ray for this pixel */

	rayview(thisray.rorg, thisray.rdir, &ourview,
			x + pixjitter(), y + pixjitter());

	rayorigin(&thisray, NULL, PRIMARY, 1.0);
	
	rayvalue(&thisray);			/* trace ray */

	copycolor(col, thisray.rcol);		/* return color */
}


int
salvage(oldfile)		/* salvage scanlines from killed program */
char  *oldfile;
{
	COLR  *scanline;
	FILE  *fp;
	int  x, y;

	if (oldfile == NULL)
		return(0);
	
	if ((fp = fopen(oldfile, "r")) == NULL) {
		sprintf(errmsg, "cannot open recover file \"%s\"", oldfile);
		error(WARNING, errmsg);
		return(0);
	}
				/* discard header */
	getheader(fp, NULL);
				/* get picture size */
	if (fgetresolu(&x, &y, fp) != (YMAJOR|YDECR)) {
		sprintf(errmsg, "bad recover file \"%s\"", oldfile);
		error(WARNING, errmsg);
		fclose(fp);
		return(0);
	}

	if (x != ourview.hresolu || y != ourview.vresolu) {
		sprintf(errmsg, "resolution mismatch in recover file \"%s\"",
				oldfile);
		error(USER, errmsg);
	}

	scanline = (COLR *)malloc(ourview.hresolu*sizeof(COLR));
	if (scanline == NULL)
		error(SYSTEM, "out of memory in salvage");
	for (y = 0; y < ourview.vresolu; y++) {
		if (freadcolrs(scanline, ourview.hresolu, fp) < 0)
			break;
		if (fwritecolrs(scanline, ourview.hresolu, stdout) < 0)
			goto writerr;
	}
	if (fflush(stdout) == EOF)
		goto writerr;
	free((char *)scanline);
	fclose(fp);
	unlink(oldfile);
	return(y);
writerr:
	error(SYSTEM, "write error in salvage");
}
