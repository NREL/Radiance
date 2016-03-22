#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 *  fvect.c - routines for floating-point vector calculations
 */

#include "copyright.h"

#define _USE_MATH_DEFINES
#include  <math.h>
#include  "fvect.h"
#include  "random.h"

double
Acos(double x)			/* insurance for touchy math library */
{
	if (x <= -1.+FTINY*FTINY)
		return(M_PI);
	if (x >= 1.-FTINY*FTINY)
		return(.0);
	return(acos(x));
}

double
Asin(double x)			/* insurance for touchy math library */
{
	if (x <= -1.+FTINY*FTINY)
		return(-M_PI/2.);
	if (x >= 1.-FTINY*FTINY)
		return(M_PI/2);
	return(asin(x));
}

double
fdot(				/* return the dot product of two vectors */
const FVECT v1,
const FVECT v2
)
{
	return(DOT(v1,v2));
}


double
dist2(				/* return square of distance between points */
const FVECT p1,
const FVECT p2
)
{
	FVECT  delta;

	VSUB(delta, p2, p1);

	return(DOT(delta, delta));
}


double
dist2line(			/* return square of distance to line */
const FVECT p,		/* the point */
const FVECT ep1,
const FVECT ep2		/* points on the line */
)
{
	double  d, d1, d2;

	d = dist2(ep1, ep2);
	d1 = dist2(ep1, p);
	d2 = d + d1 - dist2(ep2, p);

	return(d1 - 0.25*d2*d2/d);
}


double
dist2lseg(			/* return square of distance to line segment */
const FVECT p,		/* the point */
const FVECT ep1,
const FVECT ep2		/* the end points */
)
{
	double  d, d1, d2;

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
	d2 = d + d1 - d2;

	return(d1 - 0.25*d2*d2/d);	/* distance to line */
}


void
fcross(				/* vres = v1 X v2 */
FVECT vres,
const FVECT v1,
const FVECT v2
)
{
	if ((vres == v1) | (vres == v2)) {
		FVECT	vtmp;
		VCROSS(vtmp, v1, v2);
		VCOPY(vres, vtmp);
		return;
	}
	VCROSS(vres, v1, v2);
}


void
fvsum(				/* vres = v0 + f*v1 */
FVECT vres,
const FVECT v0,
const FVECT v1,
double f
)
{
	VSUM(vres, v0, v1, f);
}


double
normalize(			/* normalize a vector, return old magnitude */
FVECT  v
)
{
	double  len, d;
	
	d = DOT(v, v);
	
	if (d == 0.0)
		return(0.0);
	
	if ((d <= 1.0+FTINY) & (d >= 1.0-FTINY)) {
		len = 0.5 + 0.5*d;	/* first order approximation */
		d = 2.0 - len;
	} else {
		len = sqrt(d);
		d = 1.0/len;
	}
	v[0] *= d;
	v[1] *= d;
	v[2] *= d;

	return(len);
}


int
getperpendicular(		/* choose perpedicular direction */
FVECT vp,				/* returns normalized */
const FVECT v,				/* input vector must be normalized */
int randomize				/* randomize orientation */
)
{
	int	ord[3];
	FVECT	v1;
	int	i;

	if (randomize) {		/* randomize coordinates? */
		v1[0] = 0.5 - frandom();
		v1[1] = 0.5 - frandom();
		v1[2] = 0.5 - frandom();
		switch ((int)(frandom()*6.)) {
		case 0: ord[0] = 0; ord[1] = 1; ord[2] = 2; break;
		case 1: ord[0] = 0; ord[1] = 2; ord[2] = 1; break;
		case 2: ord[0] = 1; ord[1] = 0; ord[2] = 2; break;
		case 3: ord[0] = 1; ord[1] = 2; ord[2] = 0; break;
		case 4: ord[0] = 2; ord[1] = 0; ord[2] = 1; break;
		default: ord[0] = 2; ord[1] = 1; ord[2] = 0; break;
		}
	} else {
		v1[0] = v1[1] = v1[2] = 0.0;
		ord[0] = 0; ord[1] = 1; ord[2] = 2;
	}

	for (i = 3; i--; )
		if ((-0.6 < v[ord[i]]) & (v[ord[i]] < 0.6))
			break;
	if (i < 0)
		return(0);

	v1[ord[i]] = 1.0;
	fcross(vp, v1, v);

	return(normalize(vp) > 0.0);
}


int
closestapproach(			/* closest approach of two rays */
RREAL t[2],		/* returned distances along each ray */
const FVECT rorg0,		/* first origin */
const FVECT rdir0,		/* first direction (normalized) */
const FVECT rorg1,		/* second origin */
const FVECT rdir1		/* second direction (normalized) */
)
{
	double	dotprod = DOT(rdir0, rdir1);
	double	denom = 1. - dotprod*dotprod;
	double	o1o2_d1;
	FVECT	o0o1;

	if (denom <= FTINY) {		/* check if lines are parallel */
		t[0] = t[1] = 0.0;
		return(0);
	}
	VSUB(o0o1, rorg0, rorg1);
	o1o2_d1 = DOT(o0o1, rdir1);
	t[0] = (o1o2_d1*dotprod - DOT(o0o1,rdir0)) / denom;
	t[1] = o1o2_d1 + t[0]*dotprod;
	return(1);
}


void
spinvector(				/* rotate vector around normal */
FVECT vres,		/* returned vector (same magnitude as vorig) */
const FVECT vorig,		/* original vector */
const FVECT vnorm,		/* normalized vector for rotation */
double theta		/* right-hand radians */
)
{
	double  sint, cost, normprod;
	FVECT  vperp;
	int  i;
	
	if (theta == 0.0) {
		if (vres != vorig)
			VCOPY(vres, vorig);
		return;
	}
	cost = cos(theta);
	sint = sin(theta);
	normprod = DOT(vorig, vnorm)*(1.-cost);
	VCROSS(vperp, vnorm, vorig);
	for (i = 0; i < 3; i++)
		vres[i] = vorig[i]*cost + vnorm[i]*normprod + vperp[i]*sint;
}

double
geodesic(		/* rotate vector on great circle towards target */
FVECT vres,		/* returned vector (same magnitude as vorig) */
const FVECT vorig,	/* original vector */
const FVECT vtarg,	/* vector we are rotating towards */
double t,		/* amount along arc directed towards vtarg */
int meas		/* distance measure (radians, absolute, relative) */
)
{
	FVECT	normtarg;
	double	volen, dotprod, sintr, cost;
	int	i;

	VCOPY(normtarg, vtarg);		/* in case vtarg==vres */
	if (vres != vorig)
		VCOPY(vres, vorig);
	if (t == 0.0)
		return(VLEN(vres));	/* no rotation requested */
	if ((volen = normalize(vres)) == 0.0)
		return(0.0);
	if (normalize(normtarg) == 0.0)
		return(0.0);		/* target vector is zero */
	dotprod = DOT(vres, normtarg);
					/* check for colinear */
	if (dotprod >= 1.0-FTINY*FTINY) {
		if (meas != GEOD_REL)
			return(0.0);
		vres[0] *= volen; vres[1] *= volen; vres[2] *= volen;
		return(volen);
	}
	if (dotprod <= -1.0+FTINY*FTINY)
		return(0.0);
	if (meas == GEOD_ABS)
		t /= volen;
	else if (meas == GEOD_REL)
		t *= acos(dotprod);
	cost = cos(t);
	sintr = sin(t) / sqrt(1. - dotprod*dotprod);
	for (i = 0; i < 3; i++)
		vres[i] = volen*( cost*vres[i] +
				  sintr*(normtarg[i] - dotprod*vres[i]) );

	return(volen);			/* return vector length */
}
