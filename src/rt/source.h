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

typedef struct {
	FVECT  dir;		/* source direction */
	float  dom;		/* domega for source */
	COLOR  val;		/* contribution */
}  CONTRIB;		/* direct contribution */

typedef struct {
	int  sno;		/* source number */
	float  brt;		/* brightness (for comparison) */
}  CNTPTR;		/* contribution pointer */

extern SRCREC  *source;			/* our source list */
extern int  nsources;			/* the number of sources */

extern double  srcray();                /* ray to source */

extern SPOT  *makespot();		/* make spotlight */

extern SRCREC  *newsource();		/* allocate new source */
