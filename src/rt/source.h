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
	OBJREC  *so;		/* source object */
}  SOURCE;		/* light source */

#define  MAXSOURCE	512		/* maximum # of sources */

extern SOURCE  srcval[MAXSOURCE];	/* our source list */
extern int  nsources;			/* the number of sources */

extern double  srcray();		/* ray to source */

extern SPOT  *makespot();		/* make spotlight */
