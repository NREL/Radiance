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
