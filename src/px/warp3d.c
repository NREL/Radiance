/* Copyright (c) 1997 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * 3D warping routines.
 */

#include <stdio.h>

#include <math.h>

#include "fvect.h"

#include "warp3d.h"

#define MIND	1e-5		/* minimum distance between input points */

typedef struct {
	GNDX	n;		/* index must be first */
	W3VEC	p;
} KEYDP;		/* key/data allocation pair */

#define fgetvec(f,v)	(fgetval(f,'f',v) > 0 && fgetval(f,'f',v+1) > 0 \
				&& fgetval(f,'f',v+2) > 0)

#define AHUNK	32		/* number of points to allocate at a time */

#ifndef malloc
extern char	*malloc(), *realloc();
#endif
extern void	free();


double
wpdist2(p1, p2)			/* compute square of distance between points */
register W3VEC	p1, p2;
{
	double	d, d2;

	d = p1[0] - p2[0]; d2  = d*d;
	d = p1[1] - p2[1]; d2 += d*d;
	d = p1[2] - p2[2]; d2 += d*d;
	return(d2);
}


int
warp3d(po, pi, wp)		/* warp 3D point according to the given map */
W3VEC	po, pi;
register WARP3D	*wp;
{
	int	rval = W3OK;
	GNDX	gi;
	W3VEC	gd;

	if (wp->grid.gn[0] == 0 && (rval = new3dwgrid(wp)) != W3OK)
		return(rval);
	rval |= gridpoint(gi, gd, pi, &wp->grid);
	if (wp->grid.flags & W3EXACT) {
		rval |= warp3dex(po, pi, wp);
		return(rval);
	}
	if (wp->grid.flags & W3FAST) {
		rval |= get3dgpt(po, gi, wp);
		return(rval);
	}
	rval |= get3dgin(po, gi, gd, wp);
	return(rval);
}


int
gridpoint(ndx, rem, ipt, gp)	/* compute grid position for ipt */
GNDX	ndx;
W3VEC	rem, ipt;
register struct grid3d	*gp;
{
	int	rval = W3OK;
	register int	i;

	for (i = 0; i < 3; i++) {
		rem[i] = (ipt[i] - gp->gmin[i])/gp->gstep[i];
		if (rem[i] < 0.) {
			ndx[i] = 0;
			rval = W3GAMUT;
		} else if ((int)rem[i] >= gp->gn[i]) {
			ndx[i] = gp->gn[i] - 1;
			rval = W3GAMUT;
		} else
			ndx[i] = (int)rem[i];
		rem[i] -= (double)ndx[i];
	}
	return(rval);
}


int
get3dgpt(po, ndx, wp)		/* get value for voxel */
W3VEC	po;
GNDX	ndx;
register WARP3D	*wp;
{
	W3VEC	gpt;
	register LUENT	*le;
	KEYDP	*kd;
	int	rval = W3OK;
	register int	i;

	le = lu_find(&wp->grid.gtab, (char *)ndx);
	if (le == NULL)
		return(W3ERROR);
	if (le->data == NULL) {
		if (le->key != NULL)
			kd = (KEYDP *)le->key;
		else if ((kd = (KEYDP *)malloc(sizeof(KEYDP))) == NULL)
			return(W3ERROR);
		for (i = 0; i < 3; i++) {
			kd->n[i] = ndx[i];
			gpt[i] = wp->grid.gmin[i] + ndx[i]*wp->grid.gstep[i];
			if (wp->grid.flags & W3FAST)	/* on centers */
				gpt[i] += .5*wp->grid.gstep[i];
		}
		rval |= warp3dex(kd->p, gpt, wp);
		le->key = (char *)kd->n;
		le->data = (char *)kd->p;
	}
	W3VCPY(po, (float *)le->data);
	return(rval);
}


int
get3dgin(po, ndx, rem, wp)	/* interpolate from warp grid */
W3VEC	po, rem;
GNDX	ndx;
WARP3D	*wp;
{
	W3VEC	cv[8];
	int	rval;
	GNDX	gi;
	register int	i;
					/* get corner values */
	for (i = 0; i < 8; i++) {
		gi[0] = ndx[0] + (i & 1);
		gi[1] = ndx[1] + (i>>1 & 1);
		gi[2] = ndx[2] + (i>>2);
		rval |= get3dgpt(cv[i], gi, wp);
	}
	if (rval & W3ERROR)
		return(rval);
	l3interp(po, cv, rem, 3);	/* perform trilinear interpolation */
	return(rval);
}


l3interp(vo, cl, dv, n)		/* trilinear interpolation (recursive) */
W3VEC	vo, *cl, dv;
int	n;
{
	W3VEC	v0, v1;
	register int	i;

	if (--n) {
		l3interp(v0, cl, dv, n);
		l3interp(v1, cl+(1<<n), dv, n);
	} else {
		W3VCPY(v0, cl[0]);
		W3VCPY(v1, cl[1]);
	}
	for (i = 0; i < 3; i++)
		vo[i] = (1.-dv[n])*v0[i] + dv[n]*v1[i];
}


int
warp3dex(po, pi, wp)		/* compute warp using 1/r^2 weighted avg. */
W3VEC	po, pi;
register WARP3D	*wp;
{
	double	d2, w, wsum;
	W3VEC	pt;
	register int	i;

	pt[0] = pt[1] = pt[2] = 0.;
	wsum = 0.;
	for (i = wp->npts; i--; ) {
		d2 = wpdist2(pi, wp->ip[i]);
		if (d2 <= MIND*MIND) w = 1./(MIND*MIND);
		else w = 1./d2;
		pt[0] += w*wp->op[i][0];
		pt[1] += w*wp->op[i][1];
		pt[2] += w*wp->op[i][2];
		wsum += w;
	}
	if (wsum > 0.) {
		po[0] = pt[0]/wsum;
		po[1] = pt[1]/wsum;
		po[2] = pt[2]/wsum;
	}
	return(W3OK);
}


WARP3D *
new3dw(flgs)			/* allocate and initialize WARP3D struct */
int	flgs;
{
	register WARP3D  *wp;

	if ((flgs & (W3EXACT|W3FAST)) == (W3EXACT|W3FAST)) {
		eputs("new3dw: only one of W3EXACT or W3FAST\n");
		return(NULL);
	}
	if ((wp = (WARP3D *)malloc(sizeof(WARP3D))) == NULL) {
		eputs("new3dw: no memory\n");
		return(NULL);
	}
	wp->ip = wp->op = NULL;
	wp->npts = 0;
	wp->grid.flags = flgs;
	wp->grid.gn[0] = wp->grid.gn[1] = wp->grid.gn[2] = 0;
	return(wp);
}


WARP3D *
load3dw(fn, wp)			/* load 3D warp from file */
char	*fn;
WARP3D	*wp;
{
	FILE	*fp;
	W3VEC	inp, outp;

	if ((fp = fopen(fn, "r")) == NULL) {
		eputs(fn);
		eputs(": cannot open\n");
		return(NULL);
	}
	if (wp == NULL && (wp = new3dw()) == NULL)
		goto memerr;
	while (fgetvec(fp, inp) && fgetvec(fp, outp))
		if (!add3dpt(wp, inp, outp))
			goto memerr;
	if (!feof(fp)) {
		wputs(fn);
		wputs(": non-number in 3D warp file\n");
	}
	goto cleanup;
memerr:
	eputs("load3dw: out of memory\n");
cleanup:
	fclose(fp);
	return(wp);
}


int
add3dpt(wp, pti, pto)		/* add 3D point pair to warp structure */
register WARP3D	*wp;
W3VEC	pti, pto;
{
	double	d2;
	register W3VEC	*na;
	register int	i;

	if (wp->npts == 0) {			/* initialize */
		wp->ip = (W3VEC *)malloc(AHUNK*sizeof(W3VEC));
		if (wp->ip == NULL) return(0);
		wp->op = (W3VEC *)malloc(AHUNK*sizeof(W3VEC));
		if (wp->op == NULL) return(0);
		wp->d2min = 1e10;
		wp->d2max = 0.;
		W3VCPY(wp->llim, pti);
		W3VCPY(wp->ulim, pti);
	} else {
		if (wp->npts % AHUNK == 0) {	/* allocate another hunk */
			na = (W3VEC *)realloc((char *)wp->ip,
					(wp->npts+AHUNK)*sizeof(W3VEC));
			if (na == NULL) return(0);
			wp->ip = na;
			na = (W3VEC *)realloc((char *)wp->op,
					(wp->npts+AHUNK)*sizeof(W3VEC));
			if (na == NULL) return(0);
			wp->op = na;
		}
		for (i = 0; i < 3; i++)		/* check boundaries */
			if (pti[i] < wp->llim[i])
				wp->llim[i] = pti[i];
			else if (pti[i] > wp->ulim[i])
				wp->ulim[i] = pti[i];
		for (i = wp->npts; i--; ) {	/* check distances */
			d2 = wpdist2(pti, wp->ip[i]);
			if (d2 < MIND*MIND)	/* value is too close */
				return(wp->npts);
			if (d2 < wp->d2min)
				wp->d2min = d2;
			if (d2 > wp->d2max)
				wp->d2max = d2;
		}
	}
	W3VCPY(wp->ip[wp->npts], pti);	/* add new point */
	W3VCPY(wp->op[wp->npts], pto);
	done3dgrid(&wp->grid);		/* old grid is invalid */
	return(++wp->npts);
}


free3dw(wp)			/* free WARP3D data */
register WARP3D	*wp;
{
	done3dgrid(&wp->grid);
	free((char *)wp->ip);
	free((char *)wp->op);
	free((char *)wp);
}


long
gridhash(gp)			/* hash a grid point index */
GNDX	gp;
{
	return(((unsigned long)gp[0]<<GNBITS | gp[1])<<GNBITS | gp[2]);
}


int
new3dwgrid(wp)			/* initialize interpolating grid for warp */
register WARP3D	*wp;
{
	double	gridstep, d;
	register int	i;
				/* free old grid (if any) */
	done3dgrid(&wp->grid);
				/* check parameters */
	if (wp->npts < 2)
		return(W3BADMAP);	/* undefined for less than 2 points */
	if (wp->d2max < MIND)
		return(W3BADMAP);	/* not enough disparity */
				/* compute gamut */
	W3VCPY(wp->grid.gmin, wp->llim);
	W3VCPY(wp->grid.gmax, wp->ulim);
	gridstep = sqrt(wp->d2min);
	for (i = 0; i < 3; i++) {
		wp->grid.gmin[i] -= .501*gridstep;
		wp->grid.gmax[i] += .501*gridstep;
	}
	if (wp->grid.flags & W3EXACT) {
		wp->grid.gn[0] = wp->grid.gn[1] = wp->grid.gn[2] = 1;
		return(W3OK);		/* no interpolation, so no grid */
	}
				/* create grid */
	for (i = 0; i < 3; i++) {
		d = wp->grid.gmax[i] - wp->grid.gmin[i];
		wp->grid.gn[i] = d/gridstep + .5;
		if (wp->grid.gn[i] >= MAXGN)
			wp->grid.gn[i] = MAXGN-1;
		wp->grid.gstep[i] = d / wp->grid.gn[i];
	}
				/* initialize lookup table */
	wp->grid.gtab.hashf = gridhash;
	wp->grid.gtab.keycmp = NULL;
	wp->grid.gtab.freek = free;
	wp->grid.gtab.freed = NULL;
	return(lu_init(&wp->grid.gtab, 1024) ? W3OK : W3ERROR);
}


done3dgrid(gp)			/* free interpolating grid for warp */
register struct grid3d	*gp;
{
	if (gp->gn[0] == 0)
		return;
	lu_done(&gp->gtab);
	gp->gn[0] = gp->gn[1] = gp->gn[2] = 0;
}
