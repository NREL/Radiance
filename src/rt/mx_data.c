/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 *  mx_data.c - routine for stored mixtures.
 *
 *     11/2/88
 */

#include  "ray.h"

#include  "data.h"

#include  "func.h"

/*
 *	A stored mixture is specified:
 *
 *	modifier mixdata name
 *	6+ foremod backmod func dfname vfname v0 v1 .. xf
 *	0
 *	n A1 A2 ..
 *
 *  Vfname is the name of the file where the variable definitions
 *  can be found.  The list of real arguments can be accessed by
 *  definitions in the file.  Dfname is the data file.
 *  The dimensions of the data files and the number
 *  of variables must match.  The func is a single argument
 *  function which returns the corrected data value given the
 *  interpolated value from the file.  The xf is a transformation
 *  to get from the original coordinates to the current coordinates.
 */


mx_data(m, r)			/* interpolate mixture data */
register OBJREC  *m;
RAY  *r;
{
	OBJECT	obj;
	double  coef;
	double  pt[MAXDIM];
	DATARRAY  *dp;
	OBJECT  mod[2];
	register MFUNC  *mf;
	register int  i;

	if (m->oargs.nsargs < 6)
		objerror(m, USER, "bad # arguments");
	obj = objndx(m);
	for (i = 0; i < 2; i++)
		if (!strcmp(m->oargs.sarg[i], VOIDID))
			mod[i] = OVOID;
		else if ((mod[i] = lastmod(obj, m->oargs.sarg[i])) == OVOID) {
			sprintf(errmsg, "undefined modifier \"%s\"",
					m->oargs.sarg[i]);
			objerror(m, USER, errmsg);
		}
	dp = getdata(m->oargs.sarg[3]);
	i = (1 << dp->nd) - 1;
	mf = getfunc(m, 4, i<<5, 0);
	setfunc(m, r);
	errno = 0;
	for (i = 0; i < dp->nd; i++) {
		pt[i] = evalue(mf->ep[i]);
		if (errno)
			goto computerr;
	}
	coef = datavalue(dp, pt);
	errno = 0;
	coef = funvalue(m->oargs.sarg[2], 1, &coef);
	if (errno)
		goto computerr;
	if (raymixture(r, mod[0], mod[1], coef)) {
		if (m->omod != OVOID)
			objerror(m, USER, "inappropriate modifier");
		return(1);
	}
	return(0);
computerr:
	objerror(m, WARNING, "compute error");
	return(0);
}
