/* Copyright (c) 1986 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *  source.h - header file for ray tracing sources.
 *
 *     8/20/85
 */

#define  SDISTANT	01		/* source distant flag */
#define  SSKIP		02		/* source skip flag */

typedef struct {
	short  sflags;		/* source flags */
	FVECT  sloc;		/* direction or position of source */
	float  ss;		/* tangent or disk radius */
	float  ss2;		/* domega or projected area */
	long  ntests, nhits;	/* shadow tests and hits */
	OBJREC  *so;		/* source object */
}  SRCREC;		/* light source */

typedef struct {
	int  sno;		/* source number */
	FVECT  dir;		/* source direction */
	float  dom;		/* domega for source */
	float  brt;		/* brightness (for comparison) */
	COLOR  val;		/* contribution */
}  CONTRIB;		/* direct contribution */

extern SRCREC  *source;			/* our source list */
extern int  nsources;			/* the number of sources */

extern double  srcray();                /* ray to source */
