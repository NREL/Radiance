/* Copyright (c) 1991 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 * Common definitions for interreflection routines.
 */

/*
 * Since we've defined our vectors as float below to save space,
 * watch out for changes in the definitions of VCOPY() and DOT()
 * and don't pass these vectors to fvect routines.
 */
typedef struct ambrec {
	unsigned long  latick;	/* last accessed tick */
	float  pos[3];		/* position in space */
	float  dir[3];		/* normal direction */
	int  lvl;		/* recursion level of parent ray */
	float  weight;		/* weight of parent ray */
	float  rad;		/* validity radius */
	COLOR  val;		/* computed ambient value */
	float  gpos[3];		/* gradient wrt. position */
	float  gdir[3];		/* gradient wrt. direction */
	struct ambrec  *next;	/* next in list */
}  AMBVAL;			/* ambient value */

extern COLOR  ambval;		/* global ambient component */
extern int  ambvwt;		/* initial weight for ambient value */
extern double  ambacc;		/* ambient accuracy */
extern int  ambres;		/* ambient resolution */
extern int  ambdiv;		/* number of divisions for calculation */
extern int  ambssamp;		/* number of super-samples */
extern int  ambounce;		/* number of ambient bounces */
extern char  *amblist[];	/* ambient include/exclude list */
extern int  ambincl;		/* include == 1, exclude == 0 */
extern double  maxarad;		/* maximum ambient radius */
extern double  minarad;		/* minimum ambient radius */

extern double  sumambient(), doambient(), makeambient();

#define  AVGREFL	0.5	/* assumed average reflectance */

#define  AMBVALSIZ	75	/* number of bytes in portable AMBVAL struct */
#define  AMBMAGIC	557	/* magic number for ambient value files */
#define  AMBFMT		"Radiance_ambval"	/* format id string */
