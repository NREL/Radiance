/* Copyright (c) 1999 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Rendering routines for rhpict.
 */

#include "holo.h"
#include "view.h"

#ifndef DEPS
#define DEPS		0.01	/* depth epsilon */
#endif
#ifndef MAXRAD
#define MAXRAD		64	/* maximum kernel radius */
#endif
#ifndef NNEIGH
#define NNEIGH		7	/* find this many neighbors */
#endif

#define NINF		16382

#define MAXRAD2		(MAXRAD*MAXRAD+1)

#define G0NORM		0.286	/* ground zero normalization (1/x integral) */

#ifndef FL4OP
#define FL4OP(f,i,op)	((f)[(i)>>5] op (1L<<((i)&0x1f)))
#define CHK4(f,i)	FL4OP(f,i,&)
#define SET4(f,i)	FL4OP(f,i,|=)
#define CLR4(f,i)	FL4OP(f,i,&=~)
#define TGL4(f,i)	FL4OP(f,i,^=)
#define FL4NELS(n)	(((n)+0x1f)>>5)
#define CLR4ALL(f,n)	bzero((char *)(f),FL4NELS(n)*sizeof(int4))
#endif

static int4	*pixFlags;	/* pixel occupancy flags */
static float	pixWeight[MAXRAD2];	/* pixel weighting function */

extern VIEW	myview;		/* current output view */
extern COLOR	*mypixel;	/* pixels being rendered */
extern float	*myweight;	/* weights (used to compute final pixels) */
extern float	*mydepth;	/* depth values (visibility culling) */
extern int	hres, vres;	/* current horizontal and vertical res. */


pixBeam(bp, hb)			/* render a particular beam */
BEAM	*bp;
register HDBEAMI	*hb;
{
	GCOORD	gc[2];
	register RAYVAL	*rv;
	FVECT	rorg, rdir, wp, ip;
	double	d, prox;
	COLOR	col;
	int	n;
	register int4	p;

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
			prox = d*d/DOT(ip,ip);	/* cos(diff_angle)^32 */
			prox *= prox; prox *= prox; prox *= prox; prox *= prox;
		} else {
			if (myview.type == VT_PAR || myview.vaft > FTINY)
				continue;		/* inf. off view */
			VSUM(wp, myview.vp, rdir, FHUGE);
			prox = 1.;
		}
		viewloc(ip, &myview, wp);	/* frustum clipping */
		if (ip[2] < 0.)
			continue;
		if (ip[0] < 0. || ip[0] >= 1.)
			continue;
		if (ip[1] < 0. || ip[1] >= 1.)
			continue;
		if (myview.vaft > FTINY && ip[2] > myview.vaft - myview.vfore)
			continue;		/* not exact for VT_PER */
		p = (int)(ip[1]*vres)*hres + (int)(ip[0]*hres);
		if (mydepth[p] > FTINY) {	/* check depth */
			if (ip[2] > mydepth[p]*(1.+DEPS))
				continue;
			if (ip[2] < mydepth[p]*(1.-DEPS)) {
				setcolor(mypixel[p], 0., 0., 0.);
				myweight[p] = 0.;
			}
		}
		colr_color(col, rv->v);
		scalecolor(col, prox);
		addcolor(mypixel[p], col);
		myweight[p] += prox;
		mydepth[p] = ip[2];
	}
}


int
kill_occl(h, v, nl, n)		/* check for occlusion errors */
int	h, v;
short	nl[NNEIGH][2];
int	n;
{
	short	forequad[2][2];
	int	d;
	register int4	i;

	if (n <= 0)
		return(1);
	forequad[0][0] = forequad[0][1] = forequad[1][0] = forequad[1][1] = 0;
	for (i = n; i--; ) {
		d = (h-nl[i][0])*(h-nl[i][0]) + (v-nl[i][1])*(v-nl[i][1]);
		if (mydepth[nl[i][1]*hres+nl[i][0]] <
				mydepth[v*hres+h]*(1.-DEPS*d))
			forequad[nl[i][0]<h][nl[i][1]<v] = 1;
	}
	if (forequad[0][0]+forequad[0][1]+forequad[1][0]+forequad[1][1] > 1) {
		i = v*hres + h;
		setcolor(mypixel[i], 0., 0., 0.);
		myweight[i] = 0.;	/* occupancy reset afterwards */
	}
	return(1);
}


int
grow_samp(h, v, nl, n)		/* grow sample point appropriately */
int	h, v;
register short	nl[NNEIGH][2];
int	n;
{
	COLOR	mykern[MAXRAD2];
	float	mykw[MAXRAD2];
	int4	maxr2;
	double	w;
	register int4	p, r2;
	int	maxr, h2, v2;

	if (n <= 0)
		return(1);
	p = v*hres + h;				/* build kernel values */
	maxr2 = (h-nl[n-1][0])*(h-nl[n-1][0]) + (v-nl[n-1][1])*(v-nl[n-1][1]);
	DCHECK(maxr2>=MAXRAD2, CONSISTENCY, "out of range neighbor");
	for (r2 = maxr2+1; --r2; ) {
		copycolor(mykern[r2], mypixel[p]);
		mykw[r2] = pixWeight[r2];
		if (2*r2 >= maxr2)		/* soften skirt */
			mykw[r2] *= (2*(maxr2-r2)+1.0)/maxr2;
		scalecolor(mykern[r2], mykw[r2]);
	}
	maxr = sqrt((double)maxr2) + .99;	/* stamp out that kernel */
	for (v2 = v-maxr; v2 <= v+maxr; v2++) {
		if (v2 < 0) continue;
		if (v2 >= vres) break;
		for (h2 = h-maxr; h2 <= h+maxr; h2++) {
			if (h2 < 0) continue;
			if (h2 >= hres) break;
			r2 = (v2-v)*(v2-v) + (h2-h)*(h2-h);
			if (r2 > maxr2) continue;
			if (CHK4(pixFlags, v2*hres+h2))
				continue;	/* occupied */
			addcolor(mypixel[v2*hres+h2], mykern[r2]);
			myweight[v2*hres+h2] += mykw[r2]*myweight[v*hres+h];
		}
	}
	return(1);
}


pixFlush()			/* done with beams -- flush pixel values */
{
	reset_flags();			/* set occupancy flags */
	meet_neighbors(kill_occl);	/* eliminate occlusion errors */
	reset_flags();			/* reset occupancy flags */
	if (pixWeight[0] <= FTINY) {	/* initialize weighting function */
		register int	r;
		for (r = MAXRAD2; --r; )
			pixWeight[r] = G0NORM/sqrt((double)r);
		pixWeight[0] = 1.;
	}
	meet_neighbors(grow_samp);	/* grow valid samples over image */
	free((char *)pixFlags);		/* free pixel flags */
	pixFlags = NULL;
}


reset_flags()			/* allocate/set/reset occupancy flags */
{
	register int	p;

	if (pixFlags == NULL) {
		pixFlags = (int4 *)calloc(FL4NELS(hres*vres), sizeof(int4));
		CHECK(pixFlags==NULL, SYSTEM, "out of memory in reset_flags");
	} else
		CLR4ALL(pixFlags, hres*vres);
	for (p = hres*vres; p--; )
		if (myweight[p] > FTINY)
			SET4(pixFlags, p);
}


int
findneigh(nl, h, v, rnl)	/* find NNEIGH neighbors for pixel */
short	nl[NNEIGH][2];
int	h, v;
register short	(*rnl)[NNEIGH];
{
	int	nn = 0;
	int4	d, ld, nd[NNEIGH+1];
	int	n, hoff;
	register int	h2, n2;

	ld = MAXRAD2;
	for (hoff = 1; hoff < hres; hoff = (hoff<0) - hoff) {
		h2 = h + hoff;
		if (h2 < 0 | h2 >= hres)
			continue;
		if ((h2-h)*(h2-h) >= ld)
			break;
		for (n = 0; n < NNEIGH && rnl[h2][n] < NINF; n++) {
			d = (h2-h)*(h2-h) + (v-rnl[h2][n])*(v-rnl[h2][n]);
			if (d >= ld)
				continue;
			for (n2 = nn; ; n2--) 	{	/* insert neighbor */
				if (!n2 || d >= nd[n2-1]) {
					nd[n2] = d;
					nl[n2][0] = h2;
					nl[n2][1] = rnl[h2][n];
					break;
				}
				nd[n2] = nd[n2-1];
				nl[n2][0] = nl[n2-1][0];
				nl[n2][1] = nl[n2-1][1];
			}
			if (nn < NNEIGH)
				nn++;
			else
				ld = nd[NNEIGH-1];
		}
	}
	return(nn);
}


meet_neighbors(nf)		/* run through samples and their neighbors */
int	(*nf)();
{
	short	ln[NNEIGH][2];
	int	h, v, n, v2;
	register short	(*rnl)[NNEIGH];
					/* initialize bottom row list */
	rnl = (short (*)[NNEIGH])malloc(NNEIGH*sizeof(short)*hres);
	CHECK(rnl==NULL, SYSTEM, "out of memory in meet_neighbors");
	for (h = 0; h < hres; h++) {
		for (n = v = 0; v < vres; v++)
			if (CHK4(pixFlags, v*hres+h)) {
				rnl[h][n++] = v;
				if (n >= NNEIGH)
					break;
			}
		while (n < NNEIGH)
			rnl[h][n++] = NINF;
	}
	v = 0;				/* do each row */
	for ( ; ; ) {
		for (h = 0; h < hres; h++) {
			if (!CHK4(pixFlags, v*hres+h))
				continue;	/* no one home */
			n = findneigh(ln, h, v, rnl);
			(*nf)(h, v, ln, n);	/* call on neighbors */
		}
		if (++v >= vres)		/* reinitialize row list */
			break;
		for (h = 0; h < hres; h++)
			for (v2 = rnl[h][NNEIGH-1]+1; v2 < vres; v2++) {
				if (v2 - v > v - rnl[h][0])
					break;		/* not close enough */
				if (CHK4(pixFlags, v2*hres+h)) {
					for (n = 0; n < NNEIGH-1; n++)
						rnl[h][n] = rnl[h][n+1];
					rnl[h][NNEIGH-1] = v2;
				}
			}
	}
	free((char *)rnl);		/* free row list */
}
