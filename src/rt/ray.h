/* Copyright (c) 1990 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *  ray.h - header file for routines using rays.
 *
 *     8/7/85
 */

#include  "standard.h"

#include  "object.h"

#include  "color.h"

				/* ray type flags */
#define  PRIMARY	01		/* original ray */
#define  SHADOW		02		/* ray to light source */
#define  REFLECTED	04		/* reflected ray */
#define  REFRACTED	010		/* refracted (bent) ray */
#define  TRANS		020		/* transmitted/transferred ray */
#define  AMBIENT	040		/* ray scattered for interreflection */

				/* reflected ray types */
#define  RAYREFL	(SHADOW|REFLECTED|AMBIENT)

typedef struct ray {
	long  rno;		/* unique ray number */
	int  rlvl;		/* number of reflections for this ray */
	float  rweight;		/* cumulative weight of this ray */
	short  rtype;		/* ray type */
	short  crtype;		/* cumulative ray type */
	struct ray  *parent;	/* ray this originated from */
	FVECT  rorg;		/* origin of ray */
	FVECT  rdir;		/* normalized direction of ray */
	int  rsrc;		/* source we're aiming for */
	OBJECT  *clipset;	/* set of objects currently clipped */
	OBJECT  *newcset;	/* next clipset, used for transmission */
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
}  RAY;

extern double  raynormal();
