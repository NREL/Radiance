/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  color.c - routines for color calculations.
 *
 *     10/10/85
 */

#include  <stdio.h>

#include  "color.h"

#ifdef  SPEC_RGB
/*
 *	The following table contains the CIE tristimulus integrals
 *  for X, Y, and Z.  The table is cumulative, so that
 *  each color coordinate integrates to 1.
 */

#define  STARTWL	380		/* starting wavelength (nanometers) */
#define  INCWL		10		/* wavelength increment */
#define  NINC		40		/* # of values */

static BYTE  chroma[3][NINC] = {
	{							/* X */
		0,   0,   0,   2,   6,   13,  22,  30,  36,  41,
		42,  43,  43,  44,  46,  52,  60,  71,  87,  106,
		128, 153, 178, 200, 219, 233, 243, 249, 252, 254,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255
	}, {							/* Y */
		0,   0,   0,   0,   0,   1,   2,   4,   7,   11,
		17,  24,  34,  48,  64,  84,  105, 127, 148, 169,
		188, 205, 220, 232, 240, 246, 250, 253, 254, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255
	}, {							/* Z */
		0,   0,   2,   10,  32,  66,  118, 153, 191, 220,
		237, 246, 251, 253, 254, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255
	}
};


spec_rgb(col, s, e)		/* comput RGB color from spectral range */
COLOR  col;
int  s, e;
{
	COLOR  ciecolor;

	spec_cie(ciecolor, s, e);
	cie_rgb(col, ciecolor);
}


spec_cie(col, s, e)		/* compute a color from a spectral range */
COLOR  col;		/* returned color */
int  s, e;		/* starting and ending wavelengths */
{
	register int  i, d, r;
	
	s -= STARTWL;
	if (s < 0)
		s = 0;

	e -= STARTWL;
	if (e >= INCWL*(NINC - 1))
		e = INCWL*(NINC - 1) - 1;

	d = e / INCWL;			/* interpolate values */
	r = e % INCWL;
	for (i = 0; i < 3; i++)
		col[i] = chroma[i][d]*(INCWL - r) + chroma[i][d + 1]*r;

	d = s / INCWL;
	r = s % INCWL;
	for (i = 0; i < 3; i++)
		col[i] -= chroma[i][d]*(INCWL - r) - chroma[i][d + 1]*r;

	col[RED] = (col[RED] + 0.5) / (256*INCWL);
	col[GRN] = (col[GRN] + 0.5) / (256*INCWL);
	col[BLU] = (col[BLU] + 0.5) / (256*INCWL);
}


cie_rgb(rgbcolor, ciecolor)		/* convert CIE to RGB (NTSC) */
register COLOR  rgbcolor, ciecolor;
{
	static float  cmat[3][3] = {
		1.73, -.48, -.26,
		-.81, 1.65, -.02,
		 .08, -.17, 1.28,
	};
	register int  i;

	for (i = 0; i < 3; i++) {
		rgbcolor[i] =	cmat[i][0]*ciecolor[0] +
				cmat[i][1]*ciecolor[1] +
				cmat[i][2]*ciecolor[2] ;
		if (rgbcolor[i] < 0.0)
			rgbcolor[i] = 0.0;
	}
}
#endif


fputresolu(ord, xres, yres, fp)		/* put x and y resolution */
register int  ord;
int  xres, yres;
FILE  *fp;
{
	if (ord&YMAJOR)
		fprintf(fp, "%cY %d %cX %d\n",
				ord&YDECR ? '-' : '+', yres,
				ord&XDECR ? '-' : '+', xres);
	else
		fprintf(fp, "%cX %d %cY %d\n",
				ord&XDECR ? '-' : '+', xres,
				ord&YDECR ? '-' : '+', yres);
}


fgetresolu(xrp, yrp, fp)		/* get x and y resolution */
int  *xrp, *yrp;
FILE  *fp;
{
	char  buf[64], *xndx, *yndx;
	register char  *cp;
	register int  ord;

	if (fgets(buf, sizeof(buf), fp) == NULL)
		return(-1);
	xndx = yndx = NULL;
	for (cp = buf+1; *cp; cp++)
		if (*cp == 'X')
			xndx = cp;
		else if (*cp == 'Y')
			yndx = cp;
	if (xndx == NULL || yndx == NULL)
		return(-1);
	ord = 0;
	if (xndx > yndx) ord |= YMAJOR;
	if (xndx[-1] == '-') ord |= XDECR;
	if (yndx[-1] == '-') ord |= YDECR;
	if ((*xrp = atoi(xndx+1)) <= 0)
		return(-1);
	if ((*yrp = atoi(yndx+1)) <= 0)
		return(-1);
	return(ord);
}


fwritecolrs(scanline, len, fp)		/* write out a colr scanline */
register COLR  *scanline;
int  len;
register FILE  *fp;
{
	COLR  lastcolr;
	int  rept;
	
	lastcolr[RED] = lastcolr[GRN] = lastcolr[BLU] = 1;
	lastcolr[EXP] = 0;
	rept = 0;
	
	while (len > 0) {
		if (scanline[0][EXP] == lastcolr[EXP] &&
				scanline[0][RED] == lastcolr[RED] &&
				scanline[0][GRN] == lastcolr[GRN] &&
				scanline[0][BLU] == lastcolr[BLU])
			rept++;
		else {
			while (rept) {		/* write out count */
				putc(1, fp);
				putc(1, fp);
				putc(1, fp);
				putc(rept & 255, fp);
				rept >>= 8;
			}
			putc(scanline[0][RED], fp);	/* new color */
			putc(scanline[0][GRN], fp);
			putc(scanline[0][BLU], fp);
			putc(scanline[0][EXP], fp);
			copycolr(lastcolr, scanline[0]);
			rept = 0;
		}
		scanline++;
		len--;
	}
	while (rept) {		/* write out count */
		putc(1, fp);
		putc(1, fp);
		putc(1, fp);
		putc(rept & 255, fp);
		rept >>= 8;
	}
	return(ferror(fp) ? -1 : 0);
}


freadcolrs(scanline, len, fp)		/* read in a colr scanline */
register COLR  *scanline;
int  len;
register FILE  *fp;
{
	int  rshift;
	register int  i;
	
	rshift = 0;
	
	while (len > 0) {
		scanline[0][RED] = getc(fp);
		scanline[0][GRN] = getc(fp);
		scanline[0][BLU] = getc(fp);
		scanline[0][EXP] = getc(fp);
		if (feof(fp) || ferror(fp))
			return(-1);
		if (scanline[0][RED] == 1 &&
				scanline[0][GRN] == 1 &&
				scanline[0][BLU] == 1) {
			for (i = scanline[0][EXP] << rshift; i > 0; i--) {
				copycolr(scanline[0], scanline[-1]);
				scanline++;
				len--;
			}
			rshift += 8;
		} else {
			scanline++;
			len--;
			rshift = 0;
		}
	}
	return(0);
}


fwritescan(scanline, len, fp)		/* write out a scanline */
register COLOR  *scanline;
int  len;
register FILE  *fp;
{
	COLR  lastcolr, thiscolr;
	int  rept;
	
	lastcolr[RED] = lastcolr[GRN] = lastcolr[BLU] = 1;
	lastcolr[EXP] = 0;
	rept = 0;
	
	while (len > 0) {
		setcolr(thiscolr, scanline[0][RED],
				  scanline[0][GRN],
				  scanline[0][BLU]);
		if (thiscolr[EXP] == lastcolr[EXP] &&
				thiscolr[RED] == lastcolr[RED] &&
				thiscolr[GRN] == lastcolr[GRN] &&
				thiscolr[BLU] == lastcolr[BLU])
			rept++;
		else {
			while (rept) {		/* write out count */
				putc(1, fp);
				putc(1, fp);
				putc(1, fp);
				putc(rept & 255, fp);
				rept >>= 8;
			}
			putc(thiscolr[RED], fp);	/* new color */
			putc(thiscolr[GRN], fp);
			putc(thiscolr[BLU], fp);
			putc(thiscolr[EXP], fp);
			copycolr(lastcolr, thiscolr);
			rept = 0;
		}
		scanline++;
		len--;
	}
	while (rept) {		/* write out count */
		putc(1, fp);
		putc(1, fp);
		putc(1, fp);
		putc(rept & 255, fp);
		rept >>= 8;
	}
	return(ferror(fp) ? -1 : 0);
}


freadscan(scanline, len, fp)		/* read in a scanline */
register COLOR  *scanline;
int  len;
register FILE  *fp;
{
	COLR  thiscolr;
	int  rshift;
	register int  i;
	
	rshift = 0;
	
	while (len > 0) {
		thiscolr[RED] = getc(fp);
		thiscolr[GRN] = getc(fp);
		thiscolr[BLU] = getc(fp);
		thiscolr[EXP] = getc(fp);
		if (feof(fp) || ferror(fp))
			return(-1);
		if (thiscolr[RED] == 1 &&
				thiscolr[GRN] == 1 &&
				thiscolr[BLU] == 1) {
			for (i = thiscolr[EXP] << rshift; i > 0; i--) {
				copycolor(scanline[0], scanline[-1]);
				scanline++;
				len--;
			}
			rshift += 8;
		} else {
			colr_color(scanline[0], thiscolr);
			scanline++;
			len--;
			rshift = 0;
		}
	}
	return(0);
}


setcolr(clr, r, g, b)		/* assign a short color value */
register COLR  clr;
double  r, g, b;
{
	double  frexp();
	double  d;
	int  e;
	
	d = r > g ? r : g;
	if (b > d) d = b;

	if (d <= 1e-32) {
		clr[RED] = clr[GRN] = clr[BLU] = 0;
		clr[EXP] = 0;
		return;
	}

	d = frexp(d, &e) * 256.0 / d;

	clr[RED] = r * d;
	clr[GRN] = g * d;
	clr[BLU] = b * d;
	clr[EXP] = e + COLXS;
}


colr_color(col, clr)		/* convert short to float color */
register COLOR  col;
register COLR  clr;
{
	double  f;
	
	if (clr[EXP] == 0)
		col[RED] = col[GRN] = col[BLU] = 0.0;
	else {
		f = ldexp(1.0, (int)clr[EXP]-(COLXS+8));
		col[RED] = (clr[RED] + 0.5)*f;
		col[GRN] = (clr[GRN] + 0.5)*f;
		col[BLU] = (clr[BLU] + 0.5)*f;
	}
}


normcolrs(scan, len, adjust)	/* normalize a scanline of colrs */
register COLR  *scan;
int  len;
int  adjust;
{
	register int  c;
	register int  shift;

	while (len-- > 0) {
		shift = scan[0][EXP] + adjust - COLXS;
		if (shift > 0) {
			if (shift > 8) {
				scan[0][RED] =
				scan[0][GRN] =
				scan[0][BLU] = 255;
			} else {
				shift--;
				c = (scan[0][RED]<<1 | 1) << shift;
				scan[0][RED] = c > 255 ? 255 : c;
				c = (scan[0][GRN]<<1 | 1) << shift;
				scan[0][GRN] = c > 255 ? 255 : c;
				c = (scan[0][BLU]<<1 | 1) << shift;
				scan[0][BLU] = c > 255 ? 255 : c;
			}
		} else if (shift < 0) {
			if (shift < -8) {
				scan[0][RED] =
				scan[0][GRN] =
				scan[0][BLU] = 0;
			} else {
				shift = -1-shift;
				scan[0][RED] = ((scan[0][RED]>>shift)+1)>>1;
				scan[0][GRN] = ((scan[0][GRN]>>shift)+1)>>1;
				scan[0][BLU] = ((scan[0][BLU]>>shift)+1)>>1;
			}
		}
		scan[0][EXP] = COLXS - adjust;
		scan++;
	}
}


bigdiff(c1, c2, md)			/* c1 delta c2 > md? */
register COLOR  c1, c2;
double  md;
{
	register int  i;

	for (i = 0; i < 3; i++)
		if (colval(c1,i)-colval(c2,i) > md*colval(c2,i) ||
			colval(c2,i)-colval(c1,i) > md*colval(c1,i))
			return(1);
	return(0);
}
