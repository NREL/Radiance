/* Copyright (c) 1997 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Holodeck beam support
 */

#include "rholo.h"
#include "rhdisp.h"
#include "view.h"


int
npixels(vp, hr, vr, hp, bi)	/* compute appropriate number to evaluate */
VIEW	*vp;
int	hr, vr;
HOLO	*hp;
int	bi;
{
	GCOORD	gc[2];
	FVECT	cp[4];
	FVECT	ip[4];
	double	d;
	register int	i;
					/* compute cell corners in image */
	if (!hdbcoord(gc, hp, bi))
		error(CONSISTENCY, "bad beam index in npixels");
	hdcell(cp, hp, gc+1);
	for (i = 0; i < 4; i++) {
		viewloc(ip[i], vp, cp[i]);
		if (ip[i][2] < 0.)
			return(0);
		ip[i][0] *= (double)hr;	/* scale by resolution */
		ip[i][1] *= (double)vr;
	}
					/* compute quad area */
	d = (ip[1][0]-ip[0][0])*(ip[2][1]-ip[0][1]) -
		(ip[2][0]-ip[0][0])*(ip[1][1]-ip[0][1]);
	d += (ip[2][0]-ip[3][0])*(ip[1][1]-ip[3][1]) -
		(ip[1][0]-ip[3][0])*(ip[2][1]-ip[3][1]);
	if (d < 0)
		d = -d;
					/* round off result */
	return((int)(.5*d+.5));
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
			ld[0] -= lo[0]; ld[1] -= lo[1]; ld[2] -= lo[1];
						/* find scanline limits */
			lbeg = 0; lend = hp->grid[((gc.w>>1)+1)%3];
			for (i = 0; i < 4; i++) {
				t = DOT(pn[i], lo) - po[i];
				d = -DOT(pn[i], ld);
				if (d <= FTINY && d >= -FTINY) {
					if (t < 0)
						goto nextscan;
					continue;
				}
				if (t > 0) {
					if ((t /= d) < lend)
						lend = t;
				} else {
					if ((t /= d) > lbeg)
						lbeg = t;
				}
			}
			i = lend + .5;		/* visit cells on this scan */
			for (gc.i[0] = lbeg + .5; gc.i[0] < i; gc.i[0]++)
				n += (*vf)(&gc, dp);
		nextscan:;
		}
	}
	return(n);
}


int
addcell(gcp, cl)		/* add a cell to a list */
GCOORD	*gcp;
register int	*cl;
{
	copystruct((GCOORD *)(cl+1) + *cl, gcp);
	(*cl)++;
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


int *
getviewcells(hp, vp)		/* get ordered cell list for section view */
register HOLO	*hp;
VIEW	*vp;
{
	FVECT	org, dir[4];
	int	n;
	register int	*cl;
					/* compute view pyramid */
	if (vp->type == VT_PAR) goto viewerr;
	if (viewray(org, dir[0], vp, 0., 0.) < -FTINY) goto viewerr;
	if (viewray(org, dir[1], vp, 0., 1.) < -FTINY) goto viewerr;
	if (viewray(org, dir[2], vp, 1., 1.) < -FTINY) goto viewerr;
	if (viewray(org, dir[3], vp, 1., 0.) < -FTINY) goto viewerr;
					/* allocate enough list space */
	n = 2*(	hp->grid[0]*hp->grid[1] +
		hp->grid[0]*hp->grid[2] +
		hp->grid[1]*hp->grid[2]	);
	cl = (int *)malloc(sizeof(int) + n*sizeof(GCOORD));
	if (cl == NULL)
		goto memerr;
	*cl = 0;
					/* add cells within pyramid */
	visit_cells(org, dir, hp, addcell, cl);
	if (!*cl) {
		free((char *)cl);
		return(NULL);
	}
#if 0
	/* We're just going to free this memory in a moment, and list is */
	/* sorted automatically by visit_cells(), so we don't need this. */
	if (*cl < n) {			/* optimize memory use */
		cl = (int *)realloc((char *)cl,
				sizeof(int) + *cl*sizeof(GCOORD));
		if (cl == NULL)
			goto memerr;
	}
					/* sort the list */
	qsort((char *)(cl+1), *cl, sizeof(GCOORD), cellcmp);
#endif
	return(cl);
viewerr:
	error(INTERNAL, "unusable view in getviewcells");
memerr:
	error(SYSTEM, "out of memory in getviewcells");
}
