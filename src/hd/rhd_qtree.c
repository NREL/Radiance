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

RTREE	qtrunk;			/* our quadtree trunk */
double	qtDepthEps = .02;	/* epsilon to compare depths (z fraction) */
int	qtMinNodesiz = 2;	/* minimum node dimension (pixels) */
struct rleaves	qtL;		/* our pile of leaves */

#define	TBUNDLESIZ	409	/* number of twigs in a bundle */

static RTREE	**twigbundle;	/* free twig blocks (NULL term.) */
static int	nexttwig;	/* next free twig */

#define	is_stump(t)	(!((t)->flgs & (BR_ANY|LF_ANY)))


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


static int
newleaf()			/* allocate a leaf from our pile */
{
	int	li;
	
	li = qtL.tl++;
	if (qtL.tl >= qtL.nl)	/* get next leaf in ring */
		qtL.tl = 0;
	if (qtL.tl == qtL.bl)	/* need to shake some free */
		qtCompost(LFREEPCT);
	return(li);
}


#define	LEAFSIZ		(3*sizeof(float)+sizeof(TMbright)+6*sizeof(BYTE))

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
	qtL.brt = (TMbright *)(qtL.wp + n);
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
	int	x, y, mx, my;
	double	z;
	FVECT	ip, wp;
	register int	q;
					/* compute leaf location */
	VCOPY(wp, qtL.wp[li]);
	viewloc(ip, &odev.v, wp);
	if (ip[2] <= 0. || ip[0] < 0. || ip[0] >= 1.
			|| ip[1] < 0. || ip[1] >= 1.)
		return;
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
						/* check existing leaf */
		if (lo != tp->k[q].li) {
			lo = tp->k[q].li;
			VCOPY(wp, qtL.wp[lo]);
			viewloc(ip, &odev.v, wp);
		}
						/* is node minimum size? */
		if (x1-x0 <= qtMinNodesiz || y1-y0 <= qtMinNodesiz) {
			if (z > (1.-qtDepthEps)*ip[2])	/* who is closer? */
				return;			/* old one is */
			tp->k[q].li = li;		/* new one is */
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
		tp->k[q].li = lo;
		tp->flgs |= LFF(q)|CH_ANY;	/* all new */
	}
}


dev_value(c, p)			/* add a pixel value to our output queue */
COLR	c;
FVECT	p;
{
	register int	li;

	li = newleaf();
	VCOPY(qtL.wp[li], p);
	tmCvColrs(&qtL.brt[li], qtL.chr[li], c, 1);
	addleaf(li);
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


static
redraw(tp, x0, y0, x1, y1, l)	/* mark portion of a tree for redraw */
register RTREE	*tp;
int	x0, y0, x1, y1;
int	l[2][2];
{
	int	quads = CH_ANY;
	int	mx, my;
	register int	i;
					/* compute midpoint */
	mx = (x0 + x1) >> 1;
	my = (y0 + y1) >> 1;
					/* see what to do */
	if (l[0][0] >= mx)
		quads &= ~(CHF(2)|CHF(0));
	else if (l[0][1] < mx)
		quads &= ~(CHF(3)|CHF(1));
	if (l[1][0] >= my)
		quads &= ~(CHF(1)|CHF(0));
	else if (l[1][1] < my)
		quads &= ~(CHF(3)|CHF(2));
	tp->flgs |= quads;		/* mark quadrants for update */
					/* climb the branches */
	for (i = 0; i < 4; i++)
		if (tp->flgs & BRF(i) && quads & CHF(i))
			redraw(tp->k[i].b, i&01 ? mx : x0, i&02 ? my : y0,
					i&01 ? x1 : mx, i&02 ? y1 : my, l);
}


static
update(ca, tp, x0, y0, x1, y1)	/* update tree display as needed */
BYTE	ca[3];		/* returned average color */
register RTREE	*tp;
int	x0, y0, x1, y1;
{
	int	csm[3], nc;
	register BYTE	*cp;
	BYTE	rgb[3];
	int	gaps = 0;
	int	mx, my;
	register int	i;
					/* compute midpoint */
	mx = (x0 + x1) >> 1;
	my = (y0 + y1) >> 1;
	csm[0] = csm[1] = csm[2] = nc = 0;
					/* do leaves first */
	for (i = 0; i < 4; i++) {
		if (tp->flgs & LFF(i)) {
			cp = qtL.rgb[tp->k[i].li];
			csm[0] += cp[0]; csm[1] += cp[1]; csm[2] += cp[2];
			nc++;
			if (tp->flgs & CHF(i))
				dev_paintr(cp, i&01 ? mx : x0, i&02 ? my : y0,
					       i&01 ? x1 : mx, i&02 ? y1 : my);
		} else if ((tp->flgs & CHBRF(i)) == CHF(i))
			gaps |= 1<<i;	/* empty stem */
	}
					/* now do branches */
	for (i = 0; i < 4; i++)
		if ((tp->flgs & CHBRF(i)) == CHBRF(i)) {
			update(rgb, tp->k[i].b, i&01 ? mx : x0, i&02 ? my : y0,
					i&01 ? x1 : mx, i&02 ? y1 : my);
			csm[0] += rgb[0]; csm[1] += rgb[1]; csm[2] += rgb[2];
			nc++;
		}
	if (nc > 1) {
		ca[0] = csm[0]/nc; ca[1] = csm[1]/nc; ca[2] = csm[2]/nc;
	} else {
		ca[0] = csm[0]; ca[1] = csm[1]; ca[2] = csm[2];
	}
					/* fill in gaps with average */
	for (i = 0; gaps && i < 4; gaps >>= 1, i++)
		if (gaps & 01)
			dev_paintr(ca, i&01 ? mx : x0, i&02 ? my : y0,
					i&01 ? x1 : mx, i&02 ? y1 : my);
	tp->flgs &= ~CH_ANY;		/* all done */
}


qtRedraw(x0, y0, x1, y1)	/* redraw part or all of our screen */
int	x0, y0, x1, y1;
{
	int	lim[2][2];

	if (is_stump(&qtrunk))
		return;
	if (!qtMapLeaves((lim[0][0]=x0) <= 0 & (lim[1][0]=y0) <= 0 &
		(lim[0][1]=x1) >= odev.hres-1 & (lim[1][1]=y1) >= odev.vres-1))
		return;
	redraw(&qtrunk, 0, 0, odev.hres, odev.vres, lim);
}


qtUpdate()			/* update our tree display */
{
	BYTE	ca[3];

	if (is_stump(&qtrunk))
		return;
	if (!qtMapLeaves(0))
		return;
	update(ca, &qtrunk, 0, 0, odev.hres, odev.vres);
}
