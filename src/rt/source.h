/* Copyright (c) 1986 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *  source.h - header file for ray tracing sources.
 *
 *     8/20/85
 */

#define  SDISTANT	01		/* source distant flag */
#define  SSKIP		02		/* source skip flag */
#define  SPROX		04		/* source proximity flag */
#define  SSPOT		010		/* source spotlight flag */

#define  AIMREQT	100		/* required aim success/failure */

typedef struct {
	float  siz;		/* output solid angle */
	float  flen;		/* focal length */
	FVECT  aim;		/* aim direction */
} SPOT;			/* spotlight */

typedef struct {
	short  sflags;		/* source flags */
	FVECT  sloc;		/* direction or position of source */
	float  ss;		/* tangent or disk radius */
	float  ss2;		/* domega or projected area */
	union {
		float  prox;		/* proximity */
		SPOT  *s;		/* spot */
	} sl;			/* localized source information */
	int  aimsuccess;	/* aim successes - AIMREQT*failures */
	long  ntests, nhits;	/* shadow tests and hits */
	OBJREC  *so;		/* source object */
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
