/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  p_func.c - routine for procedural patterns.
 *
 *     4/8/86
 */

#include  "ray.h"

/*
 *	A procedural pattern can either be a brightness or a
 *  color function.  A brightness function is given as:
 *
 *	modifier brightfunc name
 *	2+ bvarname filename xf
 *	0
 *	n A1 A2 ..
 *
 *  A color function is given as:
 *
 *	modifier colorfunc name
 *	4+ rvarname gvarname bvarname filename xf
 *	0
 *	n A1 A2 ..
 *
 *  Filename is the name of the file where the variable definitions
 *  can be found.  The list of real arguments can be accessed by
 *  definitions in the file.  The xf is a transformation
 *  to get from the original coordinates to the current coordinates.
 */


p_bfunc(m, r)			/* compute brightness pattern */
register OBJREC  *m;
RAY  *r;
{
	extern double  varvalue();
	extern int  errno;
	double  bval;
	register char  **sa;

	setfunc(m, r);

	sa = m->oargs.sarg;

	if (m->oargs.nsargs < 2)
		objerror(m, USER, "bad # arguments");
	if (!vardefined(sa[0]))
		loadfunc(sa[1]);
	errno = 0;
	bval = varvalue(sa[0]);
	if (errno) {
		objerror(m, WARNING, "compute error");
		return;
	}
	scalecolor(r->pcol, bval);
}


p_cfunc(m, r)			/* compute color pattern */
register OBJREC  *m;
RAY  *r;
{
	extern double  varvalue();
	extern int  errno;
	COLOR  cval;
	register char  **sa;

	setfunc(m, r);

	sa = m->oargs.sarg;

	if (m->oargs.nsargs < 4)
		objerror(m, USER, "bad # arguments");
	if (!vardefined(sa[0]))
		loadfunc(sa[3]);
	errno = 0;
	setcolor(cval, varvalue(sa[0]),
			varvalue(sa[1]),
			varvalue(sa[2]));
	if (errno) {
		objerror(m, WARNING, "compute error");
		return;
	}
	multcolor(r->pcol, cval);
}
