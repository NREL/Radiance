/* Copyright (c) 1999 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Rendering routines for rhpict.
 */

#include "holo.h"
#include "view.h"

extern VIEW	myview;		/* current output view */
extern COLOR	*mypixel;	/* pixels being rendered */
extern float	*myweight;	/* weights (used to compute final pixels) */
extern int	hres, vres;	/* current horizontal and vertical res. */


render_beam(bp, hb)		/* render a particular beam */
BEAM	*bp;
register HDBEAMI	*hb;
{
	GCOORD	gc[2];
	register RAYVAL	*rv;
	FVECT	rorg, rdir, wp, ip;
	double	d, prox;
	COLOR	col;
	int	n, p;

	if (!hdbcoord(gc, hb->h, hb->b))
		error(CONSISTENCY, "bad beam in render_beam");
	for (n = bp->nrm, rv = hdbray(bp); n--; rv++) {
						/* reproject each sample */
		hdray(rorg, rdir, hb->h, gc, rv->r);
		if (rv->d < DCINF) {
			d = hddepth(hb->h, rv->d);
			VSUM(wp, rorg, rdir, d);
			VSUB(ip, wp, myview.vp);
			d = DOT(ip,rdir);
			prox = 1. - d*d/DOT(ip,ip);	/* sin(diff_angle)^2 */
		} else {
			if (myview.type == VT_PAR || myview.vaft > FTINY)
				continue;		/* inf. off view */
			VSUM(wp, myview.vp, rdir, FHUGE);
			prox = 0.;
		}
		viewloc(ip, &myview, wp);	/* frustum clipping */
		if (ip[2] < 0.)
			continue;
		if (ip[0] < 0. || ip[0] >= 1.)
			continue;
		if (ip[1] < 0. || ip[1] >= 1.)
			continue;
		if (myview.vaft > FTINY && ip[2] > myview.vaft - myview.vfore)
			continue;
						/* we're in! */
		p = (int)(ip[1]*vres)*hres + (int)(ip[0]*hres);
		colr_color(col, rv->v);
		addcolor(mypixel[p], col);
		myweight[p] += 1.0;
	}
}
