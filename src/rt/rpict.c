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
#else
#include  <signal.h>
#endif
#include  <fcntl.h>

#include  "view.h"

#include  "random.h"

VIEW  ourview = STDVIEW;		/* view parameters */
int  hresolu = 512;			/* horizontal resolution */
int  vresolu = 512;			/* vertical resolution */
double  pixaspect = 1.0;		/* pixel aspect ratio */

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

double  pixvalue();


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
	signal(SIGALRM, report);
	sprintf(errmsg, "%ld rays, %4.2f%% done\n", nrays, pctdone);
#endif
	eputs(errmsg);

	if (ralrm > 0)
		alarm(ralrm);
}


render(zfile, oldfile)				/* render the scene */
char  *zfile, *oldfile;
{
	extern long  lseek();
	COLOR  *scanbar[MAXDIV+1];	/* scanline arrays of pixel values */
	float  *zbar[MAXDIV+1];		/* z values */
	int  ypos;			/* current scanline */
	int  ystep;			/* current y step size */
	int  zfd;
	COLOR  *colptr;
	float  *zptr;
	register int  i;
					/* check sampling */
	if (psample < 1)
		psample = 1;
	else if (psample > MAXDIV)
		psample = MAXDIV;
					/* allocate scanlines */
	for (i = 0; i <= psample; i++) {
		scanbar[i] = (COLOR *)malloc(hresolu*sizeof(COLOR));
		if (scanbar[i] == NULL)
			goto memerr;
	}
					/* open z file */
	if (zfile != NULL) {
		if ((zfd = open(zfile, O_WRONLY|O_CREAT, 0666)) == -1) {
			sprintf(errmsg, "cannot open z file \"%s\"", zfile);
			error(SYSTEM, errmsg);
		}
		for (i = 0; i <= psample; i++) {
			zbar[i] = (float *)malloc(hresolu*sizeof(float));
			if (zbar[i] == NULL)
				goto memerr;
		}
	} else {
		zfd = -1;
		for (i = 0; i <= psample; i++)
			zbar[i] = NULL;
	}
					/* write out boundaries */
	fputresolu(YMAJOR|YDECR, hresolu, vresolu, stdout);
					/* recover file and compute first */
	i = salvage(oldfile);
	if (zfd != -1 && lseek(zfd, (long)i*hresolu*sizeof(float), 0) == -1)
		error(SYSTEM, "z file seek error in render");
	ypos = vresolu-1 - i;
	fillscanline(scanbar[0], zbar[0], hresolu, ypos, psample);
	ystep = psample;
						/* compute scanlines */
	for (ypos -= ystep; ypos > -ystep; ypos -= ystep) {
							/* record progress */
		pctdone = 100.0*(vresolu-1-ypos-ystep)/vresolu;
							/* bottom adjust? */
		if (ypos < 0) {
			ystep += ypos;
			ypos = 0;
		}
		colptr = scanbar[ystep];		/* move base to top */
		scanbar[ystep] = scanbar[0];
		scanbar[0] = colptr;
		zptr = zbar[ystep];
		zbar[ystep] = zbar[0];
		zbar[0] = zptr;
							/* fill base line */
		fillscanline(scanbar[0], zbar[0], hresolu, ypos, psample);
							/* fill bar */
		fillscanbar(scanbar, zbar, hresolu, ypos, ystep);
							/* write it out */
		for (i = ystep; i > 0; i--) {
			if (zfd != -1 && write(zfd, (char *)zbar[i],
					hresolu*sizeof(float))
					< hresolu*sizeof(float))
				goto writerr;
			if (fwritescan(scanbar[i], hresolu, stdout) < 0)
				goto writerr;
		}
		if (fflush(stdout) == EOF)
			goto writerr;
	}
						/* clean up */
	if (zfd != -1) {
		if (write(zfd, (char *)zbar[0], hresolu*sizeof(float))
				< hresolu*sizeof(float))
			goto writerr;
		if (close(zfd) == -1)
			goto writerr;
		for (i = 0; i <= psample; i++)
			free((char *)zbar[i]);
	}
	fwritescan(scanbar[0], hresolu, stdout);
	if (fflush(stdout) == EOF)
		goto writerr;
	for (i = 0; i <= psample; i++)
		free((char *)scanbar[i]);
	pctdone = 100.0;
	return;
writerr:
	error(SYSTEM, "write error in render");
memerr:
	error(SYSTEM, "out of memory in render");
}


fillscanline(scanline, zline, xres, y, xstep)	/* fill scan line at y */
register COLOR  *scanline;
register float  *zline;
int  xres, y, xstep;
{
	int  b = xstep;
	double  z;
	register int  i;
	
	z = pixvalue(scanline[0], 0, y);
	if (zline) zline[0] = z;

	for (i = xstep; i < xres-1+xstep; i += xstep) {
		if (i >= xres) {
			xstep += xres-1-i;
			i = xres-1;
		}
		z = pixvalue(scanline[i], i, y);
		if (zline) zline[i] = z;
		
		b = fillsample(scanline+i-xstep, zline ? zline+i-xstep : NULL,
				i-xstep, y, xstep, 0, b/2);
	}
}


fillscanbar(scanbar, zbar, xres, y, ysize)	/* fill interior */
register COLOR  *scanbar[];
register float  *zbar[];
int  xres, y, ysize;
{
	COLOR  vline[MAXDIV+1];
	float  zline[MAXDIV+1];
	int  b = ysize;
	double  z;
	register int  i, j;
	
	for (i = 0; i < xres; i++) {
		
		copycolor(vline[0], scanbar[0][i]);
		copycolor(vline[ysize], scanbar[ysize][i]);
		if (zbar[0]) {
			zline[0] = zbar[0][i];
			zline[ysize] = zbar[ysize][i];
		}
		
		b = fillsample(vline, zbar[0] ? zline : NULL,
				i, y, 0, ysize, b/2);
		
		for (j = 1; j < ysize; j++)
			copycolor(scanbar[j][i], vline[j]);
		if (zbar[0])
			for (j = 1; j < ysize; j++)
				zbar[j][i] = zline[j];
	}
}


int
fillsample(colline, zline, x, y, xlen, ylen, b)	/* fill interior points */
register COLOR  *colline;
register float  *zline;
int  x, y;
int  xlen, ylen;
int  b;
{
	extern double  fabs();
	double  ratio;
	double  z;
	COLOR  ctmp;
	int  ncut;
	register int  len;
	
	if (xlen > 0)			/* x or y length is zero */
		len = xlen;
	else
		len = ylen;
		
	if (len <= 1)			/* limit recursion */
		return(0);
	
	if (b > 0
	|| (zline && 2.*fabs(zline[0]-zline[len]) > maxdiff*(zline[0]+zline[len]))
			|| bigdiff(colline[0], colline[len], maxdiff)) {
	
		z = pixvalue(colline[len>>1], x + (xlen>>1), y + (ylen>>1));
		if (zline) zline[len>>1] = z;
		ncut = 1;
		
	} else {					/* interpolate */
	
		copycolor(colline[len>>1], colline[len]);
		ratio = (double)(len>>1) / len;
		scalecolor(colline[len>>1], ratio);
		if (zline) zline[len>>1] = zline[len] * ratio;
		ratio = 1.0 - ratio;
		copycolor(ctmp, colline[0]);
		scalecolor(ctmp, ratio);
		addcolor(colline[len>>1], ctmp);
		if (zline) zline[len>>1] += zline[0] * ratio;
		ncut = 0;
	}
							/* recurse */
	ncut += fillsample(colline, zline, x, y, xlen>>1, ylen>>1, (b-1)/2);
	
	ncut += fillsample(colline+(len>>1), zline ? zline+(len>>1) : NULL,
			x+(xlen>>1), y+(ylen>>1),
			xlen-(xlen>>1), ylen-(ylen>>1), b/2);

	return(ncut);
}


double
pixvalue(col, x, y)		/* compute pixel value */
COLOR  col;			/* returned color */
int  x, y;			/* pixel position */
{
	static RAY  thisray;	/* our ray for this pixel */

	viewray(thisray.rorg, thisray.rdir, &ourview,
			(x+pixjitter())/hresolu, (y+pixjitter())/vresolu);

	rayorigin(&thisray, NULL, PRIMARY, 1.0);
	
	rayvalue(&thisray);			/* trace ray */

	copycolor(col, thisray.rcol);		/* return color */
	
	return(thisray.rt);			/* return distance */
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

	if (x != hresolu || y != vresolu) {
		sprintf(errmsg, "resolution mismatch in recover file \"%s\"",
				oldfile);
		error(USER, errmsg);
	}

	scanline = (COLR *)malloc(hresolu*sizeof(COLR));
	if (scanline == NULL)
		error(SYSTEM, "out of memory in salvage");
	for (y = 0; y < vresolu; y++) {
		if (freadcolrs(scanline, hresolu, fp) < 0)
			break;
		if (fwritecolrs(scanline, hresolu, stdout) < 0)
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
