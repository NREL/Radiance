/* Copyright (c) 1997 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Holodeck beam support for display process
 */

#include "rholo.h"
#include "rhdisp.h"
#include "view.h"

struct cellist {
	GCOORD	*cl;
	int	n;
};


int
npixels(vp, hr, vr, hp, bi)	/* compute appropriate number to evaluate */
register VIEW	*vp;
int	hr, vr;
HOLO	*hp;
int	bi;
{
	VIEW	vrev;
	GCOORD	gc[2];
	FVECT	cp[4], ip[4];
	double	af, ab;
	register int	i;
					/* compute cell corners in image */
	if (!hdbcoord(gc, hp, bi))
		error(CONSISTENCY, "bad beam index in npixels");
	hdcell(cp, hp, gc+1);		/* find cell on front image */
	for (i = 0; i < 4; i++) {
		viewloc(ip[i], vp, cp[i]);
		if (ip[i][2] < 0.) {
			af = 0;
			goto getback;
		}
		ip[i][0] *= (double)hr;	/* scale by resolution */
		ip[i][1] *= (double)vr;
	}
					/* compute front area */
	af = (ip[1][0]-ip[0][0])*(ip[2][1]-ip[0][1]) -
		(ip[2][0]-ip[0][0])*(ip[1][1]-ip[0][1]);
	af += (ip[2][0]-ip[3][0])*(ip[1][1]-ip[3][1]) -
		(ip[1][0]-ip[3][0])*(ip[2][1]-ip[3][1]);
	if (af >= 0) af *= 0.5;
	else af *= -0.5;
getback:
	copystruct(&vrev, vp);		/* compute reverse view */
	for (i = 0; i < 3; i++) {
		vrev.vdir[i] = -vp->vdir[i];
		vrev.vup[i] = -vp->vup[i];
		vrev.hvec[i] = -vp->hvec[i];
		vrev.vvec[i] = -vp->vvec[i];
	}
	hdcell(cp, hp, gc);		/* find cell on back image */
	for (i = 0; i < 4; i++) {
		viewloc(ip[i], &vrev, cp[i]);
		if (ip[i][2] < 0.)
			return((int)(af + 0.5));
		ip[i][0] *= (double)hr;	/* scale by resolution */
		ip[i][1] *= (double)vr;
	}
					/* compute back area */
	ab = (ip[1][0]-ip[0][0])*(ip[2][1]-ip[0][1]) -
		(ip[2][0]-ip[0][0])*(ip[1][1]-ip[0][1]);
	ab += (ip[2][0]-ip[3][0])*(ip[1][1]-ip[3][1]) -
		(ip[1][0]-ip[3][0])*(ip[2][1]-ip[3][1]);
	if (ab >= 0) ab *= 0.5;
	else ab *= -0.5;
					/* round off smaller area */
	if (af <= ab)
		return((int)(af + 0.5));
	return((int)(ab + 0.5));
}


/*
 * The ray directions that define the pyramid in visit_cells() needn't
 * be normalized, but they must be given in clockwise order as seen
 * from the pyramid's apex (origin).
 */
int
visit_cells(orig, pyrd, hp, vf, dp)	/* visit cells within a pyramid */
FVECT	orig, pyrd[4];		/* pyramid ray directions in clockwise order */
HOLO	*hp;
int	(*vf)();
char	*dp;
{
	int	n = 0;
	int	inflags = 0;
	FVECT	gp, pn[4], lo, ld;
	double	po[4], lbeg, lend, d, t;
	GCOORD	gc;
	register int	i;
					/* figure out whose side we're on */
	hdgrid(gp, hp, orig);
	for (i = 0; i < 3; i++) {
		inflags |= (gp[i] > FTINY) << (i<<1);
		inflags |= (gp[i] < hp->grid[i]-FTINY) << (i<<1 | 1);
	}
					/* compute pyramid planes */
	for (i = 0; i < 4; i++) {
		fcross(pn[i], pyrd[i], pyrd[(i+1)&03]);
		po[i] = DOT(pn[i], orig);
	}
					/* traverse each wall */
	for (gc.w = 0; gc.w < 6; gc.w++) {
		if (!(inflags & 1<<gc.w))	/* origin on wrong side */
			continue;
					/* scanline algorithm */
		for (gc.i[1] = hp->grid[((gc.w>>1)+2)%3]; gc.i[1]--; ) {
						/* compute scanline */
			gp[gc.w>>1] = gc.w&1 ? hp->grid[gc.w>>1] : 0;
			gp[((gc.w>>1)+1)%3] = 0;
			gp[((gc.w>>1)+2)%3] = gc.i[1] + 0.5;
			hdworld(lo, hp, gp);
			gp[((gc.w>>1)+1)%3] = 1;
			hdworld(ld, hp, gp);
			ld[0] -= lo[0]; ld[1] -= lo[1]; ld[2] -= lo[2];
						/* find scanline limits */
			lbeg = 0; lend = hp->grid[((gc.w>>1)+1)%3];
			for (i = 0; i < 4; i++) {
				t = DOT(pn[i], lo) - po[i];
				d = -DOT(pn[i], ld);
				if (d > FTINY) {		/* <- plane */
					if ((t /= d) < lend)
						lend = t;
				} else if (d < -FTINY) {	/* plane -> */
					if ((t /= d) > lbeg)
						lbeg = t;
				} else if (t < 0) {		/* outside */
					lend = -1;
					break;
				}
			}
			if (lbeg >= lend)
				continue;
			i = lend + .5;		/* visit cells on this scan */
			for (gc.i[0] = lbeg + .5; gc.i[0] < i; gc.i[0]++)
				n += (*vf)(&gc, dp);
		}
	}
	return(n);
}


sect_behind(hp, vp)		/* check if section is "behind" viewpoint */
register HOLO	*hp;
register VIEW	*vp;
{
	FVECT	hcent;
					/* compute holodeck section center */
	VSUM(hcent, hp->orig, hp->xv[0], 0.5);
	VSUM(hcent, hcent, hp->xv[1], 0.5);
	VSUM(hcent, hcent, hp->xv[2], 0.5);
					/* behind if center is behind */
	return(DOT(vp->vdir,hcent) < DOT(vp->vdir,vp->vp));
}


viewpyramid(org, dir, hp, vp)	/* compute view pyramid */
FVECT	org, dir[4];
HOLO	*hp;
VIEW	*vp;
{
	register int	i;
					/* check view type */
	if (vp->type == VT_PAR)
		return(0);
					/* in front or behind? */
	if (!sect_behind(hp, vp)) {
		if (viewray(org, dir[0], vp, 0., 0.) < -FTINY)
			return(0);
		if (viewray(org, dir[1], vp, 0., 1.) < -FTINY)
			return(0);
		if (viewray(org, dir[2], vp, 1., 1.) < -FTINY)
			return(0);
		if (viewray(org, dir[3], vp, 1., 0.) < -FTINY)
			return(0);
		return(1);
	}				/* reverse pyramid */
	if (viewray(org, dir[3], vp, 0., 0.) < -FTINY)
		return(0);
	if (viewray(org, dir[2], vp, 0., 1.) < -FTINY)
		return(0);
	if (viewray(org, dir[1], vp, 1., 1.) < -FTINY)
		return(0);
	if (viewray(org, dir[0], vp, 1., 0.) < -FTINY)
		return(0);
	for (i = 0; i < 3; i++) {
		dir[0][i] = -dir[0][i];
		dir[1][i] = -dir[1][i];
		dir[2][i] = -dir[2][i];
		dir[3][i] = -dir[3][i];
	}
	return(-1);
}


int
addcell(gcp, cl)		/* add a cell to a list */
GCOORD	*gcp;
register struct cellist	*cl;
{
	copystruct(cl->cl+cl->n, gcp);
	cl->n++;
	return(1);
}


int
cellcmp(gcp1, gcp2)		/* visit_cells() cell ordering */
register GCOORD	*gcp1, *gcp2;
{
	register int	c;

	if ((c = gcp1->w - gcp2->w))
		return(c);
	if ((c = gcp2->i[1] - gcp1->i[1]))	/* wg1 is reverse-ordered */
		return(c);
	return(gcp1->i[0] - gcp2->i[0]);
}


GCOORD *
getviewcells(np, hp, vp)	/* get ordered cell list for section view */
int	*np;		/* returned number of cells (negative if reversed) */
register HOLO	*hp;
VIEW	*vp;
{
	FVECT	org, dir[4];
	int	orient;
	struct cellist	cl;
					/* compute view pyramid */
	*np = 0;
	orient = viewpyramid(org, dir, hp, vp);
	if (!orient)
		return(NULL);
					/* allocate enough list space */
	cl.n = 2*(	hp->grid[0]*hp->grid[1] +
			hp->grid[0]*hp->grid[2] +
			hp->grid[1]*hp->grid[2]	);
	cl.cl = (GCOORD *)malloc(cl.n*sizeof(GCOORD));
	if (cl.cl == NULL)
		goto memerr;
	cl.n = 0;			/* add cells within pyramid */
	visit_cells(org, dir, hp, addcell, &cl);
	if (!cl.n) {
		free((char *)cl.cl);
		return(NULL);
	}
	*np = cl.n * orient;
#if 0
	/* We're just going to free this memory in a moment, and list is
	 * sorted automatically by visit_cells(), so we don't need this.
	 */
					/* optimize memory use */
	cl.cl = (GCOORD *)realloc((char *)cl.cl, cl.n*sizeof(GCOORD));
	if (cl.cl == NULL)
		goto memerr;
					/* sort the list */
	qsort((char *)cl.cl, cl.n, sizeof(GCOORD), cellcmp);
#endif
	return(cl.cl);
memerr:
	error(SYSTEM, "out of memory in getviewcells");
}
