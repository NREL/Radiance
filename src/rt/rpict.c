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
int  ambres = 128;			/* ambient resolution */
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
	COLOR  *colptr;
	register int  i;

	if (psample < 1)
		psample = 1;
	else if (psample > MAXDIV)
		psample = MAXDIV;

	ourview.hresolu -= ourview.hresolu % psample;
	ourview.vresolu -= ourview.vresolu % psample;
		
	for (i = 0; i <= psample; i++) {
		scanbar[i] = (COLOR *)malloc((ourview.hresolu+1)*sizeof(COLOR));
		if (scanbar[i] == NULL)
			error(SYSTEM, "out of memory in render");
	}
	
					/* write out boundaries */
	printf("-Y %d +X %d\n", ourview.vresolu, ourview.hresolu);

	ypos = ourview.vresolu - salvage(oldfile);	/* find top line */
	fillscanline(scanbar[0], ypos, psample);	/* top scan */

	for (ypos -= psample; ypos > -psample; ypos -= psample) {
	
		colptr = scanbar[psample];		/* get last scanline */
		scanbar[psample] = scanbar[0];
		scanbar[0] = colptr;

		fillscanline(scanbar[0], ypos, psample);	/* base scan */
	
		fillscanbar(scanbar, ypos, psample);
		
		for (i = psample-1; i >= 0; i--)
			if (fwritescan(scanbar[i], ourview.hresolu, stdout) < 0)
				goto writerr;
		if (fflush(stdout) == EOF)
			goto writerr;
		pctdone = 100.0*(ourview.vresolu-ypos)/ourview.vresolu;
	}
		
	for (i = 0; i <= psample; i++)
		free((char *)scanbar[i]);
	return;
writerr:
	error(SYSTEM, "write error in render");
}


fillscanline(scanline, y, xstep)		/* fill scan line at y */
register COLOR  *scanline;
int  y, xstep;
{
	int  b = xstep;
	register int  i;
	
	pixvalue(scanline[0], 0, y);

	for (i = xstep; i <= ourview.hresolu; i += xstep) {
	
		pixvalue(scanline[i], i, y);
		
		b = fillsample(scanline+i-xstep, i-xstep, y, xstep, 0, b/2);
	}
}


fillscanbar(scanbar, y, ysize)		/* fill interior */
register COLOR  *scanbar[];
int  y, ysize;
{
	COLOR  vline[MAXDIV+1];
	int  b = ysize;
	register int  i, j;
	
	for (i = 0; i < ourview.hresolu; i++) {
		
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
	else if ((fp = fopen(oldfile, "r")) == NULL) {
		sprintf(errmsg, "cannot open recover file \"%s\"", oldfile);
		error(WARNING, errmsg);
		return(0);
	}
				/* discard header */
	getheader(fp, NULL);
				/* get picture size */
	if (fscanf(fp, "-Y %d +X %d\n", &y, &x) != 2) {
		sprintf(errmsg, "bad recover file \"%s\"", oldfile);
		error(WARNING, errmsg);
		fclose(fp);
		return(0);
	}

	if (x != ourview.hresolu || y != ourview.vresolu) {
		sprintf(errmsg, "resolution mismatch in recover file \"%s\"",
				oldfile);
		error(USER, errmsg);
		return(0);
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
