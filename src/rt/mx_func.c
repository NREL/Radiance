/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  mx_func.c - routine for mixture functions.
 *
 *     11/2/88
 */

#include  "ray.h"

#include  "func.h"

/*
 *	A mixture function is specified:
 *
 *	modifier mixfunc name
 *	4+ foremod backmod varname vfname xf
 *	0
 *	n A1 A2 ..
 *
 *  Vfname is the name of the file where the variable definition
 *  can be found.  The list of real arguments can be accessed by
 *  definitions in the file.  The xf is a transformation
 *  to get from the original coordinates to the current coordinates.
 */


mx_func(m, r)			/* compute mixture function */
register OBJREC  *m;
RAY  *r;
{
	register int  i;
	double  coef;
	OBJECT  mod[2];
	register MFUNC  *mf;

	if (m->oargs.nsargs < 4)
		objerror(m, USER, "bad # arguments");
	for (i = 0; i < 2; i++)
		if (!strcmp(m->oargs.sarg[i], VOIDID))
			mod[i] = OVOID;
		else if ((mod[i] = modifier(m->oargs.sarg[i])) == OVOID) {
			sprintf(errmsg, "undefined modifier \"%s\"",
					m->oargs.sarg[i]);
			objerror(m, USER, errmsg);
		}
	mf = getfunc(m, 3, 0x4, 0);
	setfunc(m, r);
	errno = 0;
	coef = evalue(mf->ep[0]);
	if (errno) {
		objerror(m, WARNING, "compute error");
		return;
	}
	raymixture(r, mod[0], mod[1], coef);
}
