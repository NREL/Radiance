/* Copyright (c) 1994 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Neural-Net quantization algorithm based on work of Anthony Dekker
 */

#include "standard.h"

#include "color.h"

#include "random.h"

#ifdef COMPAT_MODE
#define neu_init	new_histo
#define neu_pixel	cnt_pixel
#define neu_colrs	cnt_colrs
#define neu_clrtab	new_clrtab
#define neu_map_pixel	map_pixel
#define neu_map_colrs	map_colrs
#define neu_dith_colrs	dith_colrs
#endif
				/* our color table (global) */
extern BYTE	clrtab[256][3];
static int	clrtabsiz;

#ifndef DEFSMPFAC
#ifdef SPEED
#define DEFSMPFAC	(240/SPEED+3)
#else
#define DEFSMPFAC	30
#endif
#endif

int	samplefac = DEFSMPFAC;	/* sampling factor */

		/* Samples array starts off holding spacing between adjacent
		 * samples, and ends up holding actual BGR sample values.
		 */
static BYTE	*thesamples;
static int	nsamples;
static BYTE	*cursamp;
static long	skipcount;

#define MAXSKIP		(1<<24-1)

#define nskip(sp)	((long)(sp)[0]<<16|(long)(sp)[1]<<8|(long)(sp)[2])

#define setskip(sp,n)	((sp)[0]=(n)>>16,(sp)[1]=((n)>>8)&255,(sp)[2]=(n)&255)


neu_init(npixels)		/* initialize our sample array */
long	npixels;
{
	register int	nsleft;
	register long	sv;
	double	rval, cumprob;
	long	npleft;

	nsamples = npixels/samplefac;
	if (nsamples < 600)
		return(-1);
	thesamples = (BYTE *)malloc(nsamples*3);
	if (thesamples == NULL)
		return(-1);
	cursamp = thesamples;
	npleft = npixels;
	nsleft = nsamples;
	while (nsleft) {
		rval = frandom();	/* random distance to next sample */
		sv = 0;
		cumprob = 0.;
		while ((cumprob += (1.-cumprob)*nsleft/(npleft-sv)) < rval)
			sv++;
		if (nsleft == nsamples)
			skipcount = sv;
		else {
			setskip(cursamp, sv);
			cursamp += 3;
		}
		npleft -= sv+1;
		nsleft--;
	}
	setskip(cursamp, npleft);	/* tag on end to skip the rest */
	cursamp = thesamples;
	return(0);
}


neu_pixel(col)			/* add pixel to our samples */
register BYTE	col[];
{
	if (!skipcount--) {
		skipcount = nskip(cursamp);
		cursamp[0] = col[BLU];
		cursamp[1] = col[GRN];
		cursamp[2] = col[RED];
		cursamp += 3;
	}
}


neu_colrs(cs, n)		/* add a scanline to our samples */
register COLR	*cs;
register int	n;
{
	while (n > skipcount) {
		cs += skipcount;
		n -= skipcount+1;
		skipcount = nskip(cursamp);
		cursamp[0] = cs[0][BLU];
		cursamp[1] = cs[0][GRN];
		cursamp[2] = cs[0][RED];
		cs++;
		cursamp += 3;
	}
	skipcount -= n;
}


neu_clrtab(ncolors)		/* make new color table using ncolors */
int	ncolors;
{
	clrtabsiz = ncolors;
	if (clrtabsiz > 256) clrtabsiz = 256;
	initnet();
	learn();
	unbiasnet();
	cpyclrtab();
	inxbuild();
				/* we're done with our samples */
	free((char *)thesamples);
				/* reset dithering function */
	neu_dith_colrs((BYTE *)NULL, (COLR *)NULL, 0);
				/* return new color table size */
	return(clrtabsiz);
}


int
neu_map_pixel(col)		/* get pixel for color */
register BYTE	col[];
{
	return(inxsearch(col[BLU],col[GRN],col[RED]));
}


neu_map_colrs(bs, cs, n)	/* convert a scanline to color index values */
register BYTE	*bs;
register COLR	*cs;
register int	n;
{
	while (n-- > 0) {
		*bs++ = inxsearch(cs[0][BLU],cs[0][GRN],cs[0][RED]);
		cs++;
	}
}


neu_dith_colrs(bs, cs, n)	/* convert scanline to dithered index values */
register BYTE	*bs;
register COLR	*cs;
int	n;
{
	static short	(*cerr)[3] = NULL;
	static int	N = 0;
	int	err[3], errp[3];
	register int	x, i;

	if (n != N) {		/* get error propogation array */
		if (N) {
			free((char *)cerr);
			cerr = NULL;
		}
		if (n)
			cerr = (short (*)[3])malloc(3*n*sizeof(short));
		if (cerr == NULL) {
			N = 0;
			map_colrs(bs, cs, n);
			return;
		}
		N = n;
		bzero((char *)cerr, 3*N*sizeof(short));
	}
	err[0] = err[1] = err[2] = 0;
	for (x = 0; x < n; x++) {
		for (i = 0; i < 3; i++) {	/* dither value */
			errp[i] = err[i];
			err[i] += cerr[x][i];
#ifdef MAXERR
			if (err[i] > MAXERR) err[i] = MAXERR;
			else if (err[i] < -MAXERR) err[i] = -MAXERR;
#endif
			err[i] += cs[x][i];
			if (err[i] < 0) err[i] = 0;
			else if (err[i] > 255) err[i] = 255;
		}
		bs[x] = inxsearch(err[BLU],err[GRN],err[RED]);
		for (i = 0; i < 3; i++) {	/* propagate error */
			err[i] -= clrtab[bs[x]][i];
			err[i] /= 3;
			cerr[x][i] = err[i] + errp[i];
		}
	}
}

/* The following was adapted and modified from the original (GW)        */
/*----------------------------------------------------------------------*/
/*									*/
/* 				NeuQuant				*/
/*				--------				*/
/*									*/
/* 		Copyright: Anthony Dekker, June 1994			*/
/*									*/
/* This program performs colour quantization of graphics images (SUN	*/
/* raster files).  It uses a Kohonen Neural Network.  It produces	*/
/* better results than existing methods and runs faster, using minimal	*/
/* space (8kB plus the image itself).  The algorithm is described in	*/
/* the paper "Kohonen Neural Networks for Optimal Colour Quantization"	*/
/* to appear in the journal "Network: Computation in Neural Systems".	*/
/* It is a significant improvement of an earlier algorithm.		*/
/*									*/
/* This program is distributed free for academic use or for evaluation	*/
/* by commercial organizations.						*/
/*									*/
/* 	Usage:	NeuQuant -n inputfile > outputfile			*/
/* 									*/
/* where n is a sampling factor for neural learning.			*/
/*									*/
/* Program performance compared with other methods is as follows:	*/
/*									*/
/* 	Algorithm		|  Av. CPU Time	|  Quantization Error	*/
/* 	-------------------------------------------------------------	*/
/* 	NeuQuant -3		|  314 		|  5.55			*/
/* 	NeuQuant -10		|  119 		|  5.97			*/
/* 	NeuQuant -30		|  65 		|  6.53			*/
/* 	Oct-Trees 		|  141 		|  8.96			*/
/* 	Median Cut (XV -best) 	|  420 		|  9.28			*/
/* 	Median Cut (XV -slow) 	|  72 		|  12.15		*/
/*									*/
/* Author's address:	Dept of ISCS, National University of Singapore	*/
/*			Kent Ridge, Singapore 0511			*/
/* Email:	tdekker@iscs.nus.sg					*/
/*----------------------------------------------------------------------*/

#define bool    int
#define false   0
#define true    1

#define initrad			32
#define radiusdec		30
#define alphadec		30

/* defs for freq and bias */
#define gammashift  	10
#define betashift  	gammashift
#define intbiasshift    16
#define intbias		(1<<intbiasshift)
#define gamma   	(1<<gammashift)
#define beta		(intbias>>betashift)
#define betagamma	(intbias<<(gammashift-betashift))
#define gammaphi	(intbias<<(gammashift-8))

/* defs for rad and alpha */
#define maxrad	 	(initrad+1)
#define radiusbiasshift	6
#define radiusbias	(1<<radiusbiasshift)
#define initradius	((int) (initrad*radiusbias))
#define alphabiasshift	10
#define initalpha	(1<<alphabiasshift)
#define radbiasshift	8
#define radbias		(1<<radbiasshift)
#define alpharadbshift  (alphabiasshift+radbiasshift)
#define alpharadbias    (1<<alpharadbshift)

/* other defs */
#define netbiasshift	4
#define funnyshift	(intbiasshift-netbiasshift)
#define maxnetval	((256<<netbiasshift)-1)
#define ncycles		100
#define jump1		499	/* prime */
#define jump2		491	/* prime */
#define jump3		487	/* any pic whose size was divisible by all */
#define jump4		503	/* four primes would be simply enormous */

/* cheater definitions (GW) */
#define thepicture	thesamples
#define lengthcount	(nsamples*3)
#define samplefac	1

typedef int pixel[4];  /* BGRc */

static pixel network[256];

static int netindex[256];

static int bias [256];
static int freq [256];
static int radpower[256];	/* actually need only go up to maxrad */

/* fixed space overhead 256*4+256+256+256+256 words = 256*8 = 8kB */


static
initnet()
{
	register int i;
	register int *p;
	
	for (i=0; i<clrtabsiz; i++) {
		p = network[i];
		p[0] =
		p[1] =
		p[2] = (i<<8) / clrtabsiz;
		freq[i] = intbias/clrtabsiz;  /* 1/256 */
		bias[i] = 0;
	}
}


static
inxbuild()
{
	register int i,j,smallpos,smallval;
	register int *p,*q;
	int start,previous;

	previous = 0;
	start = 0;
	for (i=0; i<clrtabsiz; i++) {
		p = network[i];
		smallpos = i;
		smallval = p[1];	/* index on g */
		/* find smallest in i+1..clrtabsiz-1 */
		for (j=i+1; j<clrtabsiz; j++) {
			q = network[j];
			if (q[1] < smallval) {	/* index on g */
				smallpos = j;
				smallval = q[1]; /* index on g */
			}
		}
		q = network[smallpos];
		if (i != smallpos) {
			j = q[0];   q[0] = p[0];   p[0] = j;
			j = q[1];   q[1] = p[1];   p[1] = j;
			j = q[2];   q[2] = p[2];   p[2] = j;
			j = q[3];   q[3] = p[3];   p[3] = j;
		}
		/* smallval entry is now in position i */
		if (smallval != previous) {
			netindex[previous] = (start+i)>>1;
			for (j=previous+1; j<smallval; j++) netindex[j] = i;
			previous = smallval;
			start = i;
		}
	}
	netindex[previous] = (start+clrtabsiz-1)>>1;
	for (j=previous+1; j<clrtabsiz; j++) netindex[j] = clrtabsiz-1;
}


static int
inxsearch(b,g,r)  /* accepts real BGR values after net is unbiased */
register int b,g,r;
{
	register int i,j,best,x,y,bestd;
	register int *p;

	bestd = 1000;	/* biggest possible dist is 256*3 */
	best = -1;
	i = netindex[g]; /* index on g */
	j = i-1;

	while ((i<clrtabsiz) || (j>=0)) {
		if (i<clrtabsiz) {
			p = network[i];
			x = p[1] - g;	/* inx key */
			if (x >= bestd) i = clrtabsiz; /* stop iter */
			else {
				i++;
				if (x<0) x = -x;
				y = p[0] - b;
				if (y<0) y = -y;
				x += y;
				if (x<bestd) {
					y = p[2] - r;	
					if (y<0) y = -y;
					x += y;	/* x holds distance */
					if (x<bestd) {bestd=x; best=p[3];}
				}
			}
		}
		if (j>=0) {
			p = network[j];
			x = g - p[1]; /* inx key - reverse dif */
			if (x >= bestd) j = -1; /* stop iter */
			else {
				j--;
				if (x<0) x = -x;
				y = p[0] - b;
				if (y<0) y = -y;
				x += y;
				if (x<bestd) {
					y = p[2] - r;	
					if (y<0) y = -y;
					x += y;	/* x holds distance */
					if (x<bestd) {bestd=x; best=p[3];}
				}
			}
		}
	}
	return(best);
}


static int
contest(b,g,r)	/* accepts biased BGR values */
register int b,g,r;
{
	register int i,best,bestbias,x,y,bestd,bestbiasd;
	register int *p,*q, *pp;

	bestd = ~(1<<31);
	bestbiasd = bestd;
	best = -1;
	bestbias = best;
	q = bias;
	p = freq;
	for (i=0; i<clrtabsiz; i++) {
		pp = network[i];
		x = pp[0] - b;
		if (x<0) x = -x;
		y = pp[1] - g;
		if (y<0) y = -y;
		x += y;
		y = pp[2] - r;	
		if (y<0) y = -y;
		x += y;	/* x holds distance */
			/* >> netbiasshift not needed if funnyshift used */
		if (x<bestd) {bestd=x; best=i;}
		y = x - ((*q)>>funnyshift);  /* y holds biasd */
		if (y<bestbiasd) {bestbiasd=y; bestbias=i;}
		y = (*p >> betashift);	     /* y holds beta*freq */
		*p -= y;
		*q += (y<<gammashift);
		p++;
		q++;
	}
	freq[best] += beta;
	bias[best] -= betagamma;
	return(bestbias);
}


static
alterneigh(rad,i,b,g,r)	/* accepts biased BGR values */
int rad,i;
register int b,g,r;
{
	register int j,k,lo,hi,a;
	register int *p, *q;

	lo = i-rad;
	if (lo<-1) lo= -1;
	hi = i+rad;
	if (hi>clrtabsiz) hi=clrtabsiz;

	j = i+1;
	k = i-1;
	q = radpower;
	while ((j<hi) || (k>lo)) {
		a = (*(++q));
		if (j<hi) {
			p = network[j];
			*p -= (a*(*p - b)) / alpharadbias;
			p++;
			*p -= (a*(*p - g)) / alpharadbias;
			p++;
			*p -= (a*(*p - r)) / alpharadbias;
			j++;
		}
		if (k>lo) {
			p = network[k];
			*p -= (a*(*p - b)) / alpharadbias;
			p++;
			*p -= (a*(*p - g)) / alpharadbias;
			p++;
			*p -= (a*(*p - r)) / alpharadbias;
			k--;
		}
	}
}


static
altersingle(alpha,j,b,g,r)	/* accepts biased BGR values */
register int alpha,j,b,g,r;
{
	register int *q;

	q = network[j];		/* alter hit neuron */
	*q -= (alpha*(*q - b)) / initalpha;
	q++;
	*q -= (alpha*(*q - g)) / initalpha;
	q++;
	*q -= (alpha*(*q - r)) / initalpha;
}


static
learn()
{
	register int i,j,b,g,r;
	int radius,rad,alpha,step,delta,upto;
	register unsigned char *p;
	unsigned char *lim;

	upto = lengthcount/(3*samplefac);
	delta = upto/ncycles;
	lim = thepicture + lengthcount;
	p = thepicture;
	alpha = initalpha;
	radius = initradius;
	rad = radius >> radiusbiasshift;
	if (rad <= 1) rad = 0;
	for (i=0; i<rad; i++) 
		radpower[i] = alpha*(((rad*rad - i*i)*radbias)/(rad*rad));

	if ((lengthcount%jump1) != 0) step = 3*jump1;
	else {
		if ((lengthcount%jump2) !=0) step = 3*jump2;
		else {
			if ((lengthcount%jump3) !=0) step = 3*jump3;
			else step = 3*jump4;
		}
	}
	i = 0;
	while (i < upto) {
		b = p[0] << netbiasshift;
		g = p[1] << netbiasshift;
		r = p[2] << netbiasshift;
		j = contest(b,g,r);

		altersingle(alpha,j,b,g,r);
		if (rad) alterneigh(rad,j,b,g,r);
						/* alter neighbours */

		p += step;
		if (p >= lim) p -= lengthcount;
	
		i++;
		if (i%delta == 0) {	
			alpha -= alpha / alphadec;
			radius -= radius / radiusdec;
			rad = radius >> radiusbiasshift;
			if (rad <= 1) rad = 0;
			for (j=0; j<rad; j++) 
				radpower[j] = alpha*(((rad*rad - j*j)*radbias)/(rad*rad));
		}
	}
}
	
static
unbiasnet()
{
	int i,j;

	for (i=0; i<clrtabsiz; i++) {
		for (j=0; j<3; j++)
			network[i][j] >>= netbiasshift;
		network[i][3] = i; /* record colour no */
	}
}

/* Don't do this until the network has been unbiased */
		
static
cpyclrtab()
{
	register int i,j,k;
	
	for (j=0; j<clrtabsiz; j++) {
		k = network[j][3];
		for (i = 0; i < 3; i++)
			clrtab[k][i] = network[j][2-i];
	}
}
