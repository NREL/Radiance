/* Copyright (c) 1997 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Quadtree driver support routines.
 */

#include "standard.h"
#include "rhd_qtree.h"
				/* quantity of leaves to free at a time */
#ifndef LFREEPCT
#define	LFREEPCT	25
#endif
				/* maximum allowed angle difference (deg.) */
#ifndef MAXANG
#define MAXANG		20
#endif
#if MAXANG>0
#define MAXDIFF2	( MAXANG*MAXANG * (PI*PI/180./180.))
#endif

#define abs(i)		((i) < 0 ? -(i) : (i))

RTREE	qtrunk;			/* our quadtree trunk */
double	qtDepthEps = .05;	/* epsilon to compare depths (z fraction) */
int	qtMinNodesiz = 2;	/* minimum node dimension (pixels) */
struct rleaves	qtL;		/* our pile of leaves */

#define	TBUNDLESIZ	409	/* number of twigs in a bundle */

static RTREE	**twigbundle;	/* free twig blocks (NULL term.) */
static int	nexttwig;	/* next free twig */


static RTREE *
newtwig()			/* allocate a twig */
{
	register int	bi;

	if (twigbundle == NULL) {	/* initialize */
		twigbundle = (RTREE **)malloc(sizeof(RTREE *));
		if (twigbundle == NULL)
			goto memerr;
		twigbundle[0] = NULL;
	}
	bi = nexttwig / TBUNDLESIZ;
	if (twigbundle[bi] == NULL) {	/* new block */
		twigbundle = (RTREE **)realloc((char *)twigbundle,
					(bi+2)*sizeof(RTREE *));
		if (twigbundle == NULL)
			goto memerr;
		twigbundle[bi] = (RTREE *)calloc(TBUNDLESIZ, sizeof(RTREE));
		if (twigbundle[bi] == NULL)
			goto memerr;
		twigbundle[bi+1] = NULL;
	}
				/* nexttwig++ % TBUNDLESIZ */
	return(twigbundle[bi] + (nexttwig++ - bi*TBUNDLESIZ));
memerr:
	error(SYSTEM, "out of memory in newtwig");
}


qtFreeTree(really)		/* free allocated twigs */
int	really;
{
	register int	i;

	qtrunk.flgs = CH_ANY;	/* chop down tree */
	if (twigbundle == NULL)
		return;
	i = (TBUNDLESIZ-1+nexttwig)/TBUNDLESIZ;
	nexttwig = 0;
	if (!really) {		/* just clear allocated blocks */
		while (i--)
			bzero((char *)twigbundle[i], TBUNDLESIZ*sizeof(RTREE));
		return;
	}
				/* else "really" means free up memory */
	for (i = 0; twigbundle[i] != NULL; i++)
		free((char *)twigbundle[i]);
	free((char *)twigbundle);
	twigbundle = NULL;
}


#define	LEAFSIZ		(3*sizeof(float)+sizeof(int4)+\
			sizeof(TMbright)+6*sizeof(BYTE))

int
qtAllocLeaves(n)		/* allocate space for n leaves */
register int	n;
{
	unsigned	nbytes;
	register unsigned	i;

	qtFreeTree(0);		/* make sure tree is empty */
	if (n <= 0)
		return(0);
	if (qtL.nl >= n)
		return(qtL.nl);
	else if (qtL.nl > 0)
		free(qtL.base);
				/* round space up to nearest power of 2 */
	nbytes = n*LEAFSIZ + 8;
	for (i = 1024; nbytes > i; i <<= 1)
		;
	n = (i - 8) / LEAFSIZ;	/* should we make sure n is even? */
	qtL.base = (char *)malloc(n*LEAFSIZ);
	if (qtL.base == NULL)
		return(0);
				/* assign larger alignment types earlier */
	qtL.wp = (float (*)[3])qtL.base;
	qtL.wd = (int4 *)(qtL.wp + n);
	qtL.brt = (TMbright *)(qtL.wd + n);
	qtL.chr = (BYTE (*)[3])(qtL.brt + n);
	qtL.rgb = (BYTE (*)[3])(qtL.chr + n);
	qtL.nl = n;
	qtL.tml = qtL.bl = qtL.tl = 0;
	return(n);
}

#undef	LEAFSIZ


qtFreeLeaves()			/* free our allocated leaves and twigs */
{
	qtFreeTree(1);		/* free tree also */
	if (qtL.nl <= 0)
		return;
	free(qtL.base);
	qtL.base = NULL;
	qtL.nl = 0;
}


static
shaketree(tp)			/* shake dead leaves from tree */
register RTREE	*tp;
{
	register int	i, li;

	for (i = 0; i < 4; i++)
		if (tp->flgs & BRF(i)) {
			shaketree(tp->k[i].b);
			if (is_stump(tp->k[i].b))
				tp->flgs &= ~BRF(i);
		} else if (tp->flgs & LFF(i)) {
			li = tp->k[i].li;
			if (qtL.bl < qtL.tl ?
				(li < qtL.bl || li >= qtL.tl) :
				(li < qtL.bl && li >= qtL.tl))
				tp->flgs &= ~LFF(i);
		}
}


int
qtCompost(pct)			/* free up some leaves */
int	pct;
{
	int	nused, nclear, nmapped;

				/* figure out how many leaves to clear */
	nclear = qtL.nl * pct / 100;
	nused = qtL.tl - qtL.bl;
	if (nused <= 0) nused += qtL.nl;
	nclear -= qtL.nl - nused;
	if (nclear <= 0)
		return(0);
	if (nclear >= nused) {	/* clear them all */
		qtFreeTree(0);
		qtL.tml = qtL.bl = qtL.tl = 0;
		return(nused);
	}
				/* else clear leaves from bottom */
	nmapped = qtL.tml - qtL.bl;
	if (nmapped < 0) nmapped += qtL.nl;
	qtL.bl += nclear;
	if (qtL.bl >= qtL.nl) qtL.bl -= qtL.nl;
	if (nmapped <= nclear) qtL.tml = qtL.bl;
	shaketree(&qtrunk);
	return(nclear);
}


int
qtFindLeaf(x, y)		/* find closest leaf to (x,y) */
int	x, y;
{
	register RTREE	*tp = &qtrunk;
	int	li = -1;
	int	x0=0, y0=0, x1=odev.hres, y1=odev.vres;
	int	mx, my;
	register int	q;
					/* check limits */
	if (x < 0 || x >= odev.hres || y < 0 || y >= odev.vres)
		return(-1);
					/* find nearby leaf in our tree */
	for ( ; ; ) {
		for (q = 0; q < 4; q++)		/* find any leaf this level */
			if (tp->flgs & LFF(q)) {
				li = tp->k[q].li;
				break;
			}
		q = 0;				/* which quadrant are we? */
		mx = (x0 + x1) >> 1;
		my = (y0 + y1) >> 1;
		if (x < mx) x1 = mx;
		else {x0 = mx; q |= 01;}
		if (y < my) y1 = my;
		else {y0 = my; q |= 02;}
		if (tp->flgs & BRF(q)) {	/* branch down if not a leaf */
			tp = tp->k[q].b;
			continue;
		}
		if (tp->flgs & LFF(q))		/* good shot! */
			return(tp->k[q].li);
		return(li);			/* else return what we have */
	}
}


static
addleaf(li)			/* add a leaf to our tree */
int	li;
{
	register RTREE	*tp = &qtrunk;
	int	x0=0, y0=0, x1=odev.hres, y1=odev.vres;
	int	lo = -1;
	double	d2;
	int	x, y, mx, my;
	double	z;
	FVECT	ip, wp, vd;
	register int	q;
					/* compute leaf location in view */
	VCOPY(wp, qtL.wp[li]);
	viewloc(ip, &odev.v, wp);
	if (ip[2] <= 0. || ip[0] < 0. || ip[0] >= 1.
			|| ip[1] < 0. || ip[1] >= 1.)
		return(-1);			/* behind or outside view */
#ifdef DEBUG
	if (odev.v.type == VT_PAR | odev.v.vfore > FTINY)
		error(INTERNAL, "bad view assumption in addleaf");
#endif
	for (q = 0; q < 3; q++)
		vd[q] = (wp[q] - odev.v.vp[q])/ip[2];
	d2 = fdir2diff(qtL.wd[li], vd);
#ifdef MAXDIFF2
	if (d2 > MAXDIFF2)
		return(0);			/* leaf dir. too far off */
#endif
	x = ip[0] * odev.hres;
	y = ip[1] * odev.vres;
	z = ip[2];
					/* find the place for it */
	for ( ; ; ) {
		q = 0;				/* which quadrant? */
		mx = (x0 + x1) >> 1;
		my = (y0 + y1) >> 1;
		if (x < mx) x1 = mx;
		else {x0 = mx; q |= 01;}
		if (y < my) y1 = my;
		else {y0 = my; q |= 02;}
		if (tp->flgs & BRF(q)) {	/* move to next branch */
			tp->flgs |= CHF(q);		/* not sure; guess */
			tp = tp->k[q].b;
			continue;
		}
		if (!(tp->flgs & LFF(q))) {	/* found stem for leaf */
			tp->k[q].li = li;
			tp->flgs |= CHLFF(q);
			break;
		}	
		if (lo != tp->k[q].li) {	/* check old leaf */
			lo = tp->k[q].li;
			VCOPY(wp, qtL.wp[lo]);
			viewloc(ip, &odev.v, wp);
		}
						/* is node minimum size? */
		if (y1-y0 <= qtMinNodesiz || x1-x0 <= qtMinNodesiz) {
			if (z > (1.+qtDepthEps)*ip[2])
				return(0);		/* old one closer */
			if (z >= (1.-qtDepthEps)*ip[2] &&
					fdir2diff(qtL.wd[lo], vd) < d2)
				return(0);		/* old one better */
			tp->k[q].li = li;		/* else new one is */
			tp->flgs |= CHF(q);
			break;
		}
		tp->flgs &= ~LFF(q);		/* else grow tree */
		tp->flgs |= CHBRF(q);
		tp = tp->k[q].b = newtwig();
		q = 0;				/* old leaf -> new branch */
		mx = ip[0] * odev.hres;
		my = ip[1] * odev.vres;
		if (mx >= (x0 + x1) >> 1) q |= 01;
		if (my >= (y0 + y1) >> 1) q |= 02;
		tp->flgs = CH_ANY|LFF(q);	/* all new */
		tp->k[q].li = lo;
	}
	return(1);		/* done */
}


dev_value(c, p, v)		/* add a pixel value to our quadtree */
COLR	c;
FVECT	p, v;
{
	register int	li;

	li = qtL.tl++;
	if (qtL.tl >= qtL.nl)	/* advance to next leaf in ring */
		qtL.tl = 0;
	if (qtL.tl == qtL.bl)	/* need to shake some free */
		qtCompost(LFREEPCT);
	VCOPY(qtL.wp[li], p);
	qtL.wd[li] = encodedir(v);
	tmCvColrs(&qtL.brt[li], qtL.chr[li], c, 1);
	if (!addleaf(li))
		qtL.tl = li;	/* unget this leaf */
}


qtReplant()			/* replant our tree using new view */
{
	register int	i;
					/* anything to replant? */
	if (qtL.bl == qtL.tl)
		return;
	qtFreeTree(0);			/* blow the old tree away */
					/* regrow it in new place */
	for (i = qtL.bl; i != qtL.tl; ) {
		addleaf(i);
		if (++i >= qtL.nl) i = 0;
	}
}


qtMapLeaves(redo)		/* map our leaves to RGB */
int	redo;
{
	int	aorg, alen, borg, blen;
					/* recompute mapping? */
	if (redo)
		qtL.tml = qtL.bl;
					/* already done? */
	if (qtL.tml == qtL.tl)
		return(1);
					/* compute segments */
	aorg = qtL.tml;
	if (qtL.tl >= aorg) {
		alen = qtL.tl - aorg;
		blen = 0;
	} else {
		alen = qtL.nl - aorg;
		borg = 0;
		blen = qtL.tl;
	}
					/* (re)compute tone mapping? */
	if (qtL.tml == qtL.bl) {
		tmClearHisto();
		tmAddHisto(qtL.brt+aorg, alen, 1);
		if (blen > 0)
			tmAddHisto(qtL.brt+borg, blen, 1);
		if (tmComputeMapping(0., 0., 0.) != TM_E_OK)
			return(0);
	}
	if (tmMapPixels(qtL.rgb+aorg, qtL.brt+aorg,
			qtL.chr+aorg, alen) != TM_E_OK)
		return(0);
	if (blen > 0)
		tmMapPixels(qtL.rgb+borg, qtL.brt+borg,
				qtL.chr+borg, blen);
	qtL.tml = qtL.tl;
	return(1);
}
