/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  o_face.c - routines for creating octrees for polygonal faces.
 *
 *     8/27/85
 */

#include  "standard.h"

#include  "octree.h"

#include  "object.h"

#include  "face.h"

#include  "plocate.h"

/*
 *	The algorithm for determining a face's intersection
 *  with a cube is relatively straightforward:
 *
 *	1) Check to see if any vertices are inside the cube
 *	   (intersection).
 *
 *	2) Check to see if all vertices are to one side of
 *	   cube (no intersection).
 *
 *	3) Check to see if any portion of any edge is inside
 *	   cube (intersection).
 *
 *	4) Check to see if the cube cuts the plane of the
 *	   face and one of its edges passes through
 *	   the face (intersection).
 *
 *	5) If test 4 fails, we have no intersection.
 */


o_face(o, cu)			/* determine if face intersects cube */
OBJREC  *o;
CUBE  *cu;
{
	FVECT  cumin, cumax;
	FVECT  v1, v2;
	double  d1, d2;
	int  vloc;
	register FACE  *f;
	register int  i, j;
				/* get face arguments */
	f = getface(o);
	if (f->area == 0.0)		/* empty face */
		return(0);
					/* compute cube boundaries */
	for (j = 0; j < 3; j++)
		cumax[j] = (cumin[j] = cu->cuorg[j]) + cu->cusize;

	vloc = ABOVE | BELOW;		/* check vertices */
	for (i = 0; i < f->nv; i++)
		if (j = plocate(VERTEX(f,i), cumin, cumax))
			vloc &= j;
		else
			return(1);	/* vertex inside */

	if (vloc)			/* all to one side */
		return(0);
	
	for (i = 0; i < f->nv; i++) {	/* check edges */
		if ((j = i + 1) >= f->nv)
			j = 0;			/* wrap around */
		VCOPY(v1, VERTEX(f,i));		/* clip modifies */
		VCOPY(v2, VERTEX(f,j));		/* the vertices! */
		if (clip(v1, v2, cumin, cumax))
			return(1);		/* edge inside */
	}
					/* see if cube cuts plane */
	for (j = 0; j < 3; j++)
		if (f->norm[j] > 0.0) {
			v1[j] = cumin[j];
			v2[j] = cumax[j];
		} else {
			v1[j] = cumax[j];
			v2[j] = cumin[j];
		}
	if ((d1 = DOT(v1, f->norm) - f->offset) > FTINY)
		return(0);
	if ((d2 = DOT(v2, f->norm) - f->offset) < -FTINY)
		return(0);
					/* intersect face */
	for (j = 0; j < 3; j++)
		v1[j] = (v1[j]*d2 - v2[j]*d1)/(d2 - d1);
	if (inface(v1, f))
		return(1);
	
	return(0);		/* no intersection */
}
