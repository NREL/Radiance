/* Copyright (c) 1995 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *  ray.h - header file for routines using rays.
 *
 *     8/7/85
 */

#include  "standard.h"

#include  "object.h"

#include  "color.h"

#define  MAXDIM		32	/* maximum number of dimensions */

#define  MAXSLIST	32	/* maximum sources to check */

				/* ray type flags */
#define  PRIMARY	01		/* original ray */
#define  SHADOW		02		/* ray to light source */
#define  REFLECTED	04		/* reflected ray */
#define  REFRACTED	010		/* refracted (bent) ray */
#define  TRANS		020		/* transmitted/transferred ray */
#define  AMBIENT	040		/* ray scattered for interreflection */
#define  SPECULAR	0100		/* ray scattered for specular */

				/* reflected ray types */
#define  RAYREFL	(SHADOW|REFLECTED|AMBIENT|SPECULAR)

typedef struct ray {
	unsigned long  rno;	/* unique ray number */
	int  rlvl;		/* number of reflections for this ray */
	float  rweight;		/* cumulative weight of this ray */
	short  rtype;		/* ray type */
	short  crtype;		/* cumulative ray type */
	struct ray  *parent;	/* ray this originated from */
	FVECT  rorg;		/* origin of ray */
	FVECT  rdir;		/* normalized direction of ray */
	double  rmax;		/* maximum distance (aft clipping plane) */
	int  rsrc;		/* source we're aiming for */
	OBJECT  *clipset;	/* set of objects currently clipped */
	OBJECT  *newcset;	/* next clipset, used for transmission */
	int  (*revf)();		/* evaluation function for this ray */
 	OBJREC  *ro;		/* intersected object */
	double  rot;		/* distance to object */
	FVECT  rop;		/* intersection point */
	FVECT  ron;		/* intersection surface normal */
	double  rod;		/* -DOT(rdir, ron) */
	FULLXF  *rox;		/* object transformation */
	FVECT  pert;		/* surface normal perturbation */
	COLOR  pcol;		/* pattern color */
	COLOR  rcol;		/* returned ray value */
	double  rt;		/* returned effective ray length */
	COLOR  cext;		/* medium extinction coefficient */
	COLOR  albedo;		/* medium scattering albedo */
	float  gecc;		/* scattering eccentricity coefficient */
	int  *slights;		/* list of lights to test for scattering */
}  RAY;

extern int  raytrace();

extern double  raynormal();

extern double  raydist();

extern int  dimlist[];		/* dimension list for distribution */
extern int  ndims;		/* number of dimensions so far */
extern int  samplendx;		/* index for this sample */

#define  rayvalue(r)	(*(r)->revf)(r)
