/* Copyright (c) 1996 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Draw small sources into image in case we missed them.
 */

#include  "ray.h"

#include  "view.h"

#include  "source.h"


#define	CLIP_ABOVE	1
#define CLIP_BELOW	2
#define CLIP_RIGHT	3
#define CLIP_LEFT	4

#define MAXVERT		10


static int
inregion(p, cv, crit)			/* check if vertex is in region */
FLOAT	p[2];
double	cv;
int	crit;
{
	switch (crit) {
	case CLIP_ABOVE:
		return(p[1] < cv);
	case CLIP_BELOW:
		return(p[1] >= cv);
	case CLIP_RIGHT:
		return(p[0] < cv);
	case CLIP_LEFT:
		return(p[0] >= cv);
	}
	return(-1);
}


static
clipregion(a, b, cv, crit, r)		/* find intersection with boundary */
register FLOAT	a[2], b[2];
double	cv;
int	crit;
FLOAT	r[2];	/* return value */
{
	switch (crit) {
	case CLIP_ABOVE:
	case CLIP_BELOW:
		r[1] = cv;
		r[0] = a[0] + (cv-a[1])/(b[1]-a[1])*(b[0]-a[0]);
		return;
	case CLIP_RIGHT:
	case CLIP_LEFT:
		r[0] = cv;
		r[1] = a[1] + (cv-a[0])/(b[0]-a[0])*(b[1]-a[1]);
		return;
	}
}


static int
hp_clip_poly(vl, nv, cv, crit, vlo)	/* clip polygon to half-plane */
FLOAT	vl[][2];
int	nv;
double	cv;
int	crit;
FLOAT	vlo[][2];	/* return value */
{
	FLOAT	*s, *p;
	register int	j, nvo;

	s = vl[nv-1];
	nvo = 0;
	for (j = 0; j < nv; j++) {
		p = vl[j];
		if (inregion(p, cv, crit)) {
			if (!inregion(s, cv, crit))
				clipregion(s, p, cv, crit, vlo[nvo++]);
			vlo[nvo][0] = p[0]; vlo[nvo++][1] = p[1];
		} else if (inregion(s, cv, crit))
			clipregion(s, p, cv, crit, vlo[nvo++]);
		s = p;
	}
	return(nvo);
}


static int
box_clip_poly(vl, nv, xl, xr, yb, ya, vlo)	/* clip polygon to box */
FLOAT	vl[MAXVERT][2];
int	nv;
double	xl, xr, yb, ya;
FLOAT	vlo[MAXVERT][2];	/* return value */
{
	FLOAT	vlt[MAXVERT][2];
	int	nvt, nvo;

	nvt = hp_clip_poly(vl, nv, yb, CLIP_BELOW, vlt);
	nvo = hp_clip_poly(vlt, nvt, ya, CLIP_ABOVE, vlo);
	nvt = hp_clip_poly(vlo, nvo, xl, CLIP_LEFT, vlt);
	nvo = hp_clip_poly(vlt, nvt, xr, CLIP_RIGHT, vlo);

	return(nvo);
}


static double
minw2(vl, nv, ar2)			/* compute square of minimum width */
FLOAT	vl[][2];
int	nv;
double	ar2;
{
	double	d2, w2, w2min, w2max;
	register FLOAT	*p0, *p1, *p2;
	int	i, j;
				/* find minimum for all widths */
	w2min = FHUGE;
	p0 = vl[nv-1];
	for (i = 0; i < nv; i++) {	/* for each edge */
		p1 = vl[i];
		d2 = (p1[0]-p0[0])*(p1[0]-p0[0]) +
				(p1[1]-p0[1])*(p1[1]-p0[1])*ar2;
		w2max = 0.;		/* find maximum for this side */
		for (j = 1; j < nv-1; j++) {
			p2 = vl[(i+j)%nv];
			w2 = (p1[0]-p0[0])*(p2[1]-p0[1]) -
					(p1[1]-p0[1])*(p2[0]-p0[0]);
			w2 = w2*w2*ar2/d2;	/* triangle height squared */
			if (w2 > w2max)
				w2max = w2;
		}
		if (w2max < w2min)	/* global min. based on local max.'s */
			w2min = w2max;
		p0 = p1;
	}
	return(w2min);
}


static
convex_center(vl, nv, cv)		/* compute center of convex polygon */
register FLOAT	vl[][2];
int	nv;
FLOAT	cv[2];		/* return value */
{
	register int	i;
					/* simple average (suboptimal) */
	cv[0] = cv[1] = 0.;
	for (i = 0; i < nv; i++) {
		cv[0] += vl[i][0];
		cv[1] += vl[i][1];
	}
	cv[0] /= (double)nv;
	cv[1] /= (double)nv;
}


static double
poly_area(vl, nv)			/* compute area of polygon */
register FLOAT	vl[][2];
int	nv;
{
	double	a;
	FLOAT	v0[2], v1[2];
	register int	i;

	a = 0.;
	v0[0] = vl[1][0] - vl[0][0];
	v0[1] = vl[1][1] - vl[0][1];
	for (i = 2; i < nv; i++) {
		v1[0] = vl[i][0] - vl[0][0];
		v1[1] = vl[i][1] - vl[0][1];
		a += v0[0]*v1[1] - v0[1]*v1[0];
		v0[0] = v1[0]; v0[1] = v1[1];
	}
	return(a * (a >= 0. ? .5 : -.5));
}


static int
convex_hull(vl, nv, vlo)		/* compute polygon's convex hull */
FLOAT	vl[][2];
int	nv;
FLOAT	vlo[][2];	/* return value */
{
	int	nvo, nvt;
	FLOAT	vlt[MAXVERT][2];
	double	voa, vta;
	register int	i, j;
					/* start with original polygon */
	for (i = nvo = nv; i--; ) {
		vlo[i][0] = vl[i][0]; vlo[i][1] = vl[i][1];
	}
	voa = poly_area(vlo, nvo);	/* compute its area */
	for (i = 0; i < nvo; i++) {		/* for each output vertex */
		for (j = 0; j < i; j++) {
			vlt[j][0] = vlo[j][0]; vlt[j][1] = vlo[j][1];
		}
		nvt = nvo - 1;			/* make poly w/o vertex */
		for (j = i; j < nvt; j++) {
			vlt[j][0] = vlo[j+1][0]; vlt[j][1] = vlo[j+1][1];
		}
		vta = poly_area(vlt, nvt);
		if (vta >= voa) {		/* is simpler poly bigger? */
			voa = vta;			/* then use it */
			for (j = nvo = nvt; j--; ) {
				vlo[j][0] = vlt[j][0]; vlo[j][1] = vlt[j][1];
			}
			i--;				/* next adjust */
		}
	}
	return(nvo);
}


int
sourcepoly(vw, sn, sp)			/* compute image polygon for source */
VIEW	*vw;
int	sn;
FLOAT	sp[MAXVERT][2];
{
	static char	cubeord[8][6] = {{1,3,2,6,4,5},{0,4,5,7,3,2},
					 {0,1,3,7,6,4},{0,1,5,7,6,2},
					 {0,2,6,7,5,1},{0,4,6,7,3,1},
					 {0,2,3,7,5,4},{1,5,4,6,2,3}};
	register SRCREC	*s = source + sn;
	FVECT	ap, ip;
	FLOAT	pt[6][2];
	int	dir;
	register int	i, j;

	if (s->sflags & (SDISTANT|SFLAT)) {
		if (s->sflags & SDISTANT && vw->type == VT_PAR)
			return(0);		/* all or nothing case */
		if (s->sflags & SFLAT) {
			for (i = 0; i < 3; i++)
				ap[i] = s->sloc[i] - vw->vp[i];
			if (DOT(ap, s->snorm) >= 0.)
				return(0);	/* source faces away */
		}
		for (j = 0; j < 4; j++) {	/* four corners */
			for (i = 0; i < 3; i++) {
				ap[i] = s->sloc[i];
				if (j==1|j==2) ap[i] += s->ss[SU][i];
				else ap[i] -= s->ss[SU][i];
				if (j==2|j==3) ap[i] += s->ss[SV][i];
				else ap[i] -= s->ss[SV][i];
				if (s->sflags & SDISTANT) {
					ap[i] *= 1. + vw->vfore;
					ap[i] += vw->vp[i];
				}
			}
			viewloc(ip, vw, ap);		/* find image point */
			if (ip[2] <= 0.)
				return(0);		/* in front of view */
			sp[j][0] = ip[0]; sp[j][1] = ip[1];
		}
		return(4);
	}
					/* identify furthest corner */
	for (i = 0; i < 3; i++)
		ap[i] = s->sloc[i] - vw->vp[i];
	dir =	(DOT(ap,s->ss[SU])>0.) |
		(DOT(ap,s->ss[SV])>0.)<<1 |
		(DOT(ap,s->ss[SW])>0.)<<2 ;
					/* order vertices based on this */
	for (j = 0; j < 6; j++) {
		for (i = 0; i < 3; i++) {
			ap[i] = s->sloc[i];
			if (cubeord[dir][j] & 1) ap[i] += s->ss[SU][i];
			else ap[i] -= s->ss[SU][i];
			if (cubeord[dir][j] & 2) ap[i] += s->ss[SV][i];
			else ap[i] -= s->ss[SV][i];
			if (cubeord[dir][j] & 4) ap[i] += s->ss[SW][i];
			else ap[i] -= s->ss[SW][i];
		}
		viewloc(ip, vw, ap);		/* find image point */
		if (ip[2] <= 0.)
			return(0);		/* in front of view */
		pt[j][0] = ip[0]; pt[j][1] = ip[1];
	}
	return(convex_hull(pt, 6, sp));		/* make sure it's convex */
}


			/* add sources smaller than rad to computed subimage */
drawsources(vw, xr, yr, pic, zbf, x0, xsiz, y0, ysiz, rad)
VIEW	*vw;				/* full image view */
int	xr, yr;				/* full image dimensions */
COLOR	*pic[];				/* subimage pixel value array */
float	*zbf[];				/* subimage distance array (opt.) */
int	x0, xsiz, y0, ysiz;		/* origin and size of subimage */
int	rad;				/* source sample size */
{
	int	sn;
	FLOAT	spoly[MAXVERT][2], ppoly[MAXVERT][2];
	int	nsv, npv;
	int	xmin, xmax, ymin, ymax, x, y, i;
	FLOAT	cxy[2];
	double	pa;
	RAY	sr;
					/* loop through all sources */
	for (sn = 0; sn < nsources; sn++) {
					/* compute image polygon for source */
		if (!(nsv = sourcepoly(vw, sn, spoly)))
			continue;
					/* big enough for standard sampling? */
		if (minw2(spoly, nsv, vw->vn2/vw->hn2) > (double)rad*rad/xr/xr)
			continue;
					/* clip source poly to subimage */
		nsv = box_clip_poly(spoly, nsv,
				(double)x0/xr, (double)(x0+xsiz)/xr,
				(double)y0/yr, (double)(y0+ysiz)/yr, spoly);
		if (!nsv)
			continue;
					/* find common subimage (BBox) */
		xmin = x0 + xsiz; xmax = x0;
		ymin = y0 + ysiz; ymax = y0;
		for (i = 0; i < nsv; i++) {
			if ((double)xmin/xr > spoly[i][0])
				xmin = spoly[i][0]*xr + FTINY;
			if ((double)xmax/xr < spoly[i][0])
				xmax = spoly[i][0]*xr - FTINY;
			if ((double)ymin/yr > spoly[i][1])
				ymin = spoly[i][1]*yr + FTINY;
			if ((double)ymax/yr < spoly[i][1])
				ymax = spoly[i][1]*yr - FTINY;
		}
					/* evaluate each pixel in BBox */
		for (y = ymin; y <= ymax; y++)
			for (x = xmin; x <= xmax; x++) {
							/* subarea for pixel */
				npv = box_clip_poly(spoly, nsv,
						(double)x/xr, (x+1.)/xr,
						(double)y/yr, (y+1.)/yr, ppoly);
				if (!npv)
					continue;	/* no overlap */
				convex_center(ppoly, npv, cxy);
				if ((sr.rmax = viewray(sr.rorg, sr.rdir, vw,
						cxy[0], cxy[1])) < -FTINY)
					continue;	/* not in view */
				if (source[sn].sflags & SSPOT &&
						spotout(&sr, source[sn].sl.s))
					continue;	/* outside spot */
				rayorigin(&sr, NULL, SHADOW, 1.0);
				sr.rsrc = sn;
				rayvalue(&sr);		/* compute value */
				if (bright(sr.rcol) <= FTINY)
					continue;	/* missed/blocked */
							/* modify pixel */
				if (zbf[y-y0] != NULL &&
						sr.rt < zbf[y-y0][x-x0])
					zbf[y-y0][x-x0] = sr.rt;
				pa = poly_area(ppoly, npv);
				scalecolor(sr.rcol, pa*xr*yr);
				scalecolor(pic[y-y0][x-x0], (1.-pa*xr*yr));
				addcolor(pic[y-y0][x-x0], sr.rcol);
			}
	}
}
