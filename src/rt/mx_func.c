/* Copyright (c) 1988 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  mx_func.c - routine for mixture functions.
 *
 *     11/2/88
 */

#include  "ray.h"

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
	extern double  varvalue();
	extern int  errno;
	register int  i;
	double  coef;
	OBJECT  mod[2];
	register char  **sa;

	setfunc(m, r);

	sa = m->oargs.sarg;

	if (m->oargs.nsargs < 4)
		objerror(m, USER, "bad # arguments");
	for (i = 0; i < 2; i++)
		if (!strcmp(sa[i], VOIDID))
			mod[i] = OVOID;
		else if ((mod[i] = modifier(sa[i])) == OVOID) {
			sprintf(errmsg, "undefined modifier \"%s\"", sa[i]);
			objerror(m, USER, errmsg);
		}
	funcfile(sa[3]);
	errno = 0;
	coef = varvalue(sa[2]);
	if (errno) {
		objerror(m, WARNING, "compute error");
		return;
	}
	raymixture(r, mod[0], mod[1], coef);
}
