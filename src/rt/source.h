/* Copyright (c) 1986 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *  source.h - header file for ray tracing sources.
 *
 *     8/20/85
 */

#define  AIMREQT	100		/* required aim success/failure */

#define  SDISTANT	01		/* source distant flag */
#define  SSKIP		02		/* source skip flag */
#define  SPROX		04		/* source proximity flag */
#define  SSPOT		010		/* source spotlight flag */
#define  SVIRTUAL	020		/* source virtual flag */
#define  SFLAT		040		/* source flat flag */
#define  SFOLLOW	0100		/* source follow path flag */

typedef struct {
	FVECT  aim;		/* aim direction or center */
	float  siz;		/* output solid angle or area */
	float  flen;		/* focal length */
} SPOT;			/* spotlight */

typedef struct {
	int  sflags;		/* source flags */
	FVECT  sloc;		/* direction or position of source */
	FVECT  snorm;		/* surface normal of flat source */
	float  ss;		/* tangent or disk radius */
	float  ss2;		/* domega or projected area */
	struct {
		float  prox;		/* proximity */
		SPOT  *s;		/* spot */
	} sl;			/* localized source information */
	union {
		int  success;		/* successes - AIMREQT*failures */
		int  svnext;		/* next source to aim for */
	} sa;			/* source aiming information */
	long  ntests, nhits;	/* shadow tests and hits */
	OBJREC  *so;		/* source destination object */
}  SRCREC;		/* light source */

/*
 * Special support functions for sources
 */

/*
 * Virtual source materials must define the following.
 *
 *	vproj(pm, op, sp, i)	Compute i'th virtual projection
 *				of source sp in object op and assign
 *				the 4x4 transformation matrix pm.
 *				Return 1 on success, 0 if no i'th projection.
 *
 *	nproj			The number of projections.  The value of
 *				i passed to vproj runs from 0 to nproj-1.
 */

typedef struct {
	int  (*vproj)();	/* project virtual sources */
	int  nproj;		/* number of possible projections */
} VSMATERIAL;		/* virtual source material functions */

typedef struct {
	int	(*setsrc)();	/* set light source for object */
	double  (*getpleq)();	/* plane equation for surface */
	double  (*getdisk)();	/* maximum disk for surface */
} SOBJECT;		/* source object functions */

typedef union {
	VSMATERIAL  *mf;	/* material functions */
	SOBJECT  *of;		/* object functions */
} SRCFUNC;		/* source functions */

extern SRCFUNC  sfun[];			/* source dispatch table */

extern SRCREC  *source;			/* our source list */
extern int  nsources;			/* the number of sources */

extern double  srcray();                /* ray to source */
extern int  srcvalue();			/* compute source value w/o shadows */

extern double  intercircle();		/* intersect two circles */

extern SPOT  *makespot();		/* make spotlight */

extern double  dstrsrc;			/* source distribution amount */
extern double  shadthresh;		/* relative shadow threshold */
extern double  shadcert;		/* shadow testing certainty */
extern int  directrelay;		/* maximum number of source relays */
extern int  vspretest;			/* virtual source pretest density */
