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
#define  SCYL		0100		/* source cylindrical flag */
#define  SFOLLOW	0200		/* source follow path flag */

typedef struct {
	FVECT  aim;		/* aim direction or center */
	float  siz;		/* output solid angle or area */
	float  flen;		/* focal length */
} SPOT;			/* spotlight */

typedef struct {
	int  sflags;		/* source flags */
	FVECT  sloc;		/* direction or position of source */
	FVECT  ss[3];		/* source dimension vectors, U, V, and W */
	float  srad;		/* maximum source radius */
	float  ss2;		/* solid angle or projected area */
	struct {
		float  prox;		/* proximity */
		SPOT  *s;		/* spot */
	} sl;			/* localized source information */
	union {
		int  success;		/* successes - AIMREQT*failures */
		struct {
			short  pn;		/* projection number */
			short  sn;		/* next source to aim for */
		}  sv;			/* virtual source */
	} sa;			/* source aiming information */
	long  ntests, nhits;	/* shadow tests and hits */
	OBJREC  *so;		/* source destination object */
}  SRCREC;		/* light source */

#define MAXSPART	64		/* maximum partitions per source */

#define SU		0		/* U vector or partition */
#define SV		1		/* V vector or partition */
#define SW		2		/* W vector or partition */
#define S0		3		/* leaf partition */

#define snorm		ss[SW]		/* normal vector for flat source */

typedef struct {
	int  sn;				/* source number */
	short  np;				/* number of partitions */
	short  sp;				/* this partition number */
	double  dom;				/* solid angle of partition */
	unsigned char  spt[MAXSPART/2];		/* source partitioning */
}  SRCINDEX;		/* source index structure */

#define initsrcindex(s)	((s)->sn = (s)->sp = -1, (s)->np = 0)

#define clrpart(pt)	bzero((char *)(pt), MAXSPART/2)
#define setpart(pt,i,v)	((pt)[(i)>>2] |= (v)<<(((i)&3)<<1))
#define spart(pt,pi)	((pt)[(pi)>>2] >> (((pi)&3)<<1) & 3)

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
	int	(*partit)();	/* partition light source object */
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

extern int  srcvalue();			/* compute source value w/o shadows */
extern double  nextssamp();		/* get next source sample location */
extern double  scylform();		/* cosine to axis of cylinder */

#define  sflatform(sn,dir)	-DOT(source[sn].snorm, dir)

extern OBJREC  *vsmaterial();		/* virtual source material */

extern double  intercircle();		/* intersect two circles */
extern double  spotdisk();		/* intersecting disk for spot */
extern double  beamdisk();		/* intersecting disk for beam */

extern SPOT  *makespot();		/* make spotlight */

extern double  dstrsrc;			/* source distribution amount */
extern double  shadthresh;		/* relative shadow threshold */
extern double  shadcert;		/* shadow testing certainty */
extern double  srcsizerat;		/* max. ratio of source size/dist. */
extern int  directrelay;		/* maximum number of source relays */
extern int  vspretest;			/* virtual source pretest density */
extern int  directvis;			/* sources visible? */

#define  getplaneq(c,o)		(*sfun[(o)->otype].of->getpleq)(c,o)
#define  getmaxdisk(c,o)	(*sfun[(o)->otype].of->getdisk)(c,o)
#define  setsource(s,o)		(*sfun[(o)->otype].of->setsrc)(s,o)
