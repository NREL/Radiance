/* Copyright (c) 1986 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *  face.h - header for routines using polygonal faces.
 *
 *     8/30/85
 */

#define  VERTEX(f,n)	((f)->va + 3*(n))

typedef struct {	/* a polygonal face */
	FVECT  norm;		/* the plane's unit normal */
	double  const;		/* plane equation:  DOT(norm, v) == const */
	double  area;		/* area of face */
	double  *va;		/* vertex array (o->oargs.farg) */
	short  nv;		/* # of vertices */
	short  ax;		/* axis closest to normal */
} FACE;

extern FACE  *getface();
