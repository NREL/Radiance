/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  fvect.c - routines for float vector calculations
 *
 *     8/14/85
 */

#include  "fvect.h"

#define  FTINY		1e-7


double
fdot(v1, v2)			/* return the dot product of two vectors */
register FVECT  v1, v2;
{
	return(DOT(v1,v2));
}


double
dist2(p1, p2)			/* return square of distance between points */
register FVECT  p1, p2;
{
	static FVECT  delta;

	delta[0] = p2[0] - p1[0];
	delta[1] = p2[1] - p1[1];
	delta[2] = p2[2] - p1[2];
	return(DOT(delta, delta));
}


double
dist2line(p, ep1, ep2)		/* return square of distance to line */
FVECT  p;		/* the point */
FVECT  ep1, ep2;	/* points on the line */
{
	static double  d, d1, d2;

	d = dist2(ep1, ep2);
	d1 = dist2(ep1, p);
	d2 = dist2(ep2, p);

	return(d1 - (d+d1-d2)*(d+d1-d2)/d/4);
}


double
dist2lseg(p, ep1, ep2)		/* return square of distance to line segment */
FVECT  p;		/* the point */
FVECT  ep1, ep2;	/* the end points */
{
	static double  d, d1, d2;

	d = dist2(ep1, ep2);
	d1 = dist2(ep1, p);
	d2 = dist2(ep2, p);

	if (d2 > d1) {			/* check if past endpoints */
		if (d2 - d1 > d)
			return(d1);
	} else {
		if (d1 - d2 > d)
			return(d2);
	}

	return(d1 - (d+d1-d2)*(d+d1-d2)/d/4);	/* distance to line */
}


fcross(vres, v1, v2)		/* vres = v1 X v2 */
register FVECT  vres, v1, v2;
{
	vres[0] = v1[1]*v2[2] - v1[2]*v2[1];
	vres[1] = v1[2]*v2[0] - v1[0]*v2[2];
	vres[2] = v1[0]*v2[1] - v1[1]*v2[0];
}


double
normalize(v)			/* normalize a vector, return old magnitude */
register FVECT  v;
{
	static double  len;
	
	len = DOT(v, v);
	
	if (len <= FTINY*FTINY)
		return(0.0);
	
	if (len >= (1.0-FTINY)*(1.0-FTINY) &&
			len <= (1.0+FTINY)*(1.0+FTINY))
		return(1.0);

	len = sqrt(len);
	v[0] /= len;
	v[1] /= len;
	v[2] /= len;
	return(len);
}
