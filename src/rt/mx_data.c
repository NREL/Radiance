/* Copyright (c) 1988 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  mx_data.c - routine for stored mixtures.
 *
 *     11/2/88
 */

#include  "ray.h"

#include  "data.h"

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
	extern double  varvalue(), funvalue(), datavalue();
	extern int  errno;
	register int  i;
	double  coef;
	double  pt[MAXDIM];
	DATARRAY  *dp;
	OBJECT  mod[2];
	register char  **sa;

	setfunc(m, r);

	sa = m->oargs.sarg;

	if (m->oargs.nsargs < 6)
		objerror(m, USER, "bad # arguments");
	for (i = 0; i < 2; i++)
		if (!strcmp(sa[i], VOIDID))
			mod[i] = OVOID;
		else if ((mod[i] = modifier(sa[i])) == OVOID) {
			sprintf(errmsg, "undefined modifier \"%s\"", sa[i]);
			objerror(m, USER, errmsg);
		}
	funcfile(sa[4]);
	for (i = 0; i+5 < m->oargs.nsargs &&
			sa[i+5][0] != '-'; i++) {
		if (i >= MAXDIM)
			objerror(m, USER, "dimension error");
		errno = 0;
		pt[i] = varvalue(sa[i+5]);
		if (errno)
			goto computerr;
	}
	dp = getdata(sa[3]);
	if (dp->nd != i)
		objerror(m, USER, "dimension error");
	coef = datavalue(dp, pt);
	errno = 0;
	coef = funvalue(sa[2], 1, &coef);
	if (errno)
		goto computerr;
	raymixture(r, mod[0], mod[1], coef);
	return;

computerr:
	objerror(m, WARNING, "compute error");
}
