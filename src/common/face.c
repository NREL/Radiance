/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  face.c - routines dealing with polygonal faces.
 *
 *     8/30/85
 */

#include  "standard.h"

#include  "object.h"

#include  "face.h"

/*
 *	A face is given as a list of 3D vertices.  The normal
 *  direction and therefore the surface orientation is determined
 *  by the ordering of the vertices.  Looking in the direction opposite
 *  the normal (at the front of the face), the vertices will be
 *  listed in counter-clockwise order.
 *	There is no checking done to insure that the edges do not cross
 *  one another.  This was considered too expensive and should be unnecessary.
 *  The last vertex is automatically connected to the first.
 */

#define  VERTEPS	1e-4		/* allowed vertex error */


FACE *
getface(o)			/* get arguments for a face */
OBJREC  *o;
{
	double  fabs();
	double  d1;
	int  badvert;
	FVECT  v1, v2, v3;
	register FACE  *f;
	register int  i;

	if ((f = (FACE *)o->os) != NULL)
		return(f);			/* already done */

	f = (FACE *)malloc(sizeof(FACE));
	if (f == NULL)
		error(SYSTEM, "out of memory in makeface");

	if (o->oargs.nfargs < 9 || o->oargs.nfargs % 3)
		objerror(o, USER, "bad # arguments");

	f->va = o->oargs.farg;
	f->nv = o->oargs.nfargs / 3;
						/* compute area and normal */
	f->norm[0] = f->norm[1] = f->norm[2] = 0.0;
	v1[0] = v1[1] = v1[2] = 0.0;
	for (i = 1; i < f->nv; i++) {
		v2[0] = VERTEX(f,i)[0] - VERTEX(f,0)[0];
		v2[1] = VERTEX(f,i)[1] - VERTEX(f,0)[1];
		v2[2] = VERTEX(f,i)[2] - VERTEX(f,0)[2];
		fcross(v3, v1, v2);
		f->norm[0] += v3[0];
		f->norm[1] += v3[1];
		f->norm[2] += v3[2];
		VCOPY(v1, v2);
	}
	f->area = normalize(f->norm);
	if (f->area == 0.0) {
		objerror(o, WARNING, "zero area");	/* used to be fatal */
		f->const = 0.0;
		f->ax = 0;
		return(f);
	}
	f->area *= 0.5;
						/* compute constant */
	badvert = 0;
	f->const = DOT(f->norm, VERTEX(f,0));
	for (i = 1; i < f->nv; i++) {
		d1 = DOT(f->norm, VERTEX(f,i));
		badvert += fabs(d1 - f->const/i) > VERTEPS;
		f->const += d1;
	}
	f->const /= f->nv;
	if (badvert)
		objerror(o, WARNING, "non-planar vertex");
						/* find axis */
	f->ax = fabs(f->norm[0]) > fabs(f->norm[1]) ? 0 : 1;
	if (fabs(f->norm[2]) > fabs(f->norm[f->ax]))
		f->ax = 2;

	(FACE *)o->os = f;			/* save face */
	return(f);
}


freeface(o)			/* free memory associated with face */
OBJREC  *o;
{
	free(o->os);
	o->os = NULL;
}


inface(p, f)			/* determine if point is in face */
FVECT  p;
FACE  *f;
{
	int  ncross, n;
	double  x, y;
	register int  xi, yi;
	register double  *p0, *p1;

	xi = (f->ax+1)%3;
	yi = (f->ax+2)%3;
	x = p[xi];
	y = p[yi];
	n = f->nv;
	p0 = f->va + 3*(n-1);		/* connect last to first */
	p1 = f->va;
	ncross = 0;
					/* positive x axis cross test */
	while (n--) {
		if ((p0[yi] > y) ^ (p1[yi] > y))
			if (p0[xi] > x && p1[xi] > x)
				ncross++;
			else if (p0[xi] > x || p1[xi] > x)
				ncross += (p1[yi] > p0[yi]) ^
						((p0[yi]-y)*(p1[xi]-x) >
						(p0[xi]-x)*(p1[yi]-y));
		p0 = p1;
		p1 += 3;
	}
	return(ncross & 01);
}
