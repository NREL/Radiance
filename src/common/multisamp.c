/* Copyright (c) 1997 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

#include "random.h"

/*
 * Binary space partitioning curve for multidimensional sampling.
 *
 *	Written by Christophe Schlick
 */

multisamp(t, n, r)	/* convert 1-dimensional sample to N dimensions */
double	t[];			/* returned N-dimensional vector */
register int	n;		/* number of dimensions */
double	r;			/* 1-dimensional sample [0,1) */
{
	int	j;
	register int	i, k;
	int	ti[8];
	double	s;

	i = n;
	while (i-- > 0)
		ti[i] = 0;
	j = 8;
	while (j--) {
		k = s = r*(1<<n);
		r = s - k;
		i = n;
		while (i-- > 0)
			ti[i] += ti[i] + ((k>>i) & 1);
	}
	i = n;
	while (i-- > 0)
		t[i] = 1./256. * (ti[i] + frandom());
}
