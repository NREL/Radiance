/* Copyright (c) 1996 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 * Header file for picture file conditioning.
 */

#include "standard.h"

#include "color.h"

#include "view.h"

#include "resolu.h"


#define SWNORM		2.26		/* scotopic/photopic ratio for white */
#define WHTSEFFICACY	(SWNORM*WHTEFFICACY)

#define FOVDIA		(1.0*PI/180.)	/* foveal diameter (radians) */

#define	HISTRES		100		/* histogram resolution */

#define LMIN		1e-4		/* minimum visible world luminance */
#define LMAX		1e5		/* maximum visible world luminance */

#define Bl(Lw)		log(Lw)		/* brightness function */
#define Bl1(Lw)		(1.0/(Lw))	/* first derivative of Bl(Lw) */
#define Lb(Bw)		exp(Bw)		/* inverse of brightness function */
#define Lb1(Bw)		exp(Bw)		/* first derivative of Lb(Bw) */

				/* Flags of what to do */
#define	DO_ACUITY	01
#define DO_VEIL		02
#define DO_HSENS	04
#define DO_COLOR	010
#define DO_CWEIGHT	020
#define DO_LINEAR	040

#define DO_HUMAN	(DO_ACUITY|DO_VEIL|DO_HSENS|DO_COLOR)

extern int	what2do;		/* desired adjustments */

extern double	ldmax;			/* maximum output luminance */
extern double	ldmin;			/* minimum output luminance */
extern double	Bldmin, Bldmax;		/* Bl(ldmin) and Bl(ldmax) */

extern char	*progname;		/* global argv[0] */

extern char	*infn;			/* input file name */
extern FILE	*infp;			/* input stream */
extern double	rgblum(), cielum();	/* luminance functions */
extern double	(*lumf)();		/* input luminance function */
extern double	inpexp;			/* input exposure value */

#define	plum(clr)	((*lumf)(clr,0)/inpexp)
#define slum(clr)	((*lumf)(clr,1)/inpexp)

extern COLOR	*fovimg;		/* foveal (1 degree) averaged image */
extern short	fvxr, fvyr;		/* foveal image resolution */

#define fovscan(y)	(fovimg+(y)*fvxr)

extern int	bwhist[HISTRES];	/* luminance histogram */
extern long	histot;			/* total count of histogram */
extern double	bwmin, bwmax;		/* histogram limits */
extern double	bwavg;			/* mean brightness */

#define lwhc(L)		bwhc(Bl(L))
#define bwhc(B)		bwhist[(int)(HISTRES*((B)-bwmin)/(bwmax-bwmin))]

extern RGBPRIMP	inprims;		/* input primaries */
extern COLORMAT	inrgb2xyz;		/* convert input RGB to XYZ */

extern RGBPRIMP	outprims;		/* output primaries */

extern double	scalef;			/* linear scaling factor */

extern VIEW	ourview;		/* picture view */
extern double	pixaspect;		/* pixel aspect ratio */
extern RESOLU	inpres;			/* input picture resolution */

extern char	*mbcalfile;		/* macbethcal mapping file */

struct mbc {		/* data structure for macbethcal conditioning */
	float	xa[3][6], ya[3][6];
	COLORMAT	cmat;
};

extern double	hacuity();		/* human acuity func. (cycles/deg.) */
extern double	htcontrs();		/* human contrast sens. func. */

extern COLOR	*firstscan();		/* first processed scanline */
extern COLOR	*nextscan();		/* next processed scanline */
