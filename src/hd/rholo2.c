/* Copyright (c) 1997 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Rtrace support routines for holodeck rendering
 */

#include "rholo.h"
#include "random.h"


packrays(rod, p)		/* pack ray origins and directions */
register float	*rod;
register PACKET	*p;
{
	static FVECT	ro, rd;
	GCOORD	gc[2];
	double	d;
	long	r1, r2;
	register int	i;

	if (!hdbcoord(gc, hdlist[p->hd], p->bi))
		error(CONSISTENCY, "bad beam index in packrays");
	for (i = 0; i < p->nr; i++) {
		r1 = random(); r2 = random();
		p->ra[i].r[0][0] = r1 ^ r2>>7;
		p->ra[i].r[0][1] = r1<<2 ^ r2;
		p->ra[i].r[1][0] = r1<<4 ^ r2>>15;
		p->ra[i].r[1][1] = r1<<6 ^ r2>>23;
		d = hdray(ro, rd, hdlist[p->hd], gc, p->ra[i].r);
		if (p->offset != NULL) {
			VSUM(ro, ro, rd, d);		/* exterior only */
			p->offset[i] = d;
		}
		VCOPY(rod, ro);
		rod += 3;
		VCOPY(rod, rd);
		rod += 3;
	}
}


donerays(p, rvl)		/* encode finished ray computations */
register PACKET	*p;
register float	*rvl;
{
	double	d;
	register int	i;

	for (i = 0; i < p->nr; i++) {
		setcolr(p->ra[i].v, rvl[0], rvl[1], rvl[2]);
		d = rvl[3];
		if (p->offset != NULL)
			d += p->offset[i];
		p->ra[i].d = hdcode(hdlist[p->hd], d);
		rvl += 4;
	}
}
