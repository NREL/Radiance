/* Copyright (c) 1997 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Quadtree driver support routines.
 */

#include "standard.h"
#include "rhd_qtree.h"

RTREE	qtrunk;			/* our quadtree trunk */
double	qtDepthEps = .02;	/* epsilon to compare depths (z fraction) */
int	qtMinNodesiz = 2;	/* minimum node dimension (pixels) */

static RLEAF	*leafpile;	/* our collection of leaf values */
static int	nleaves;	/* count of leaves in our pile */
static int	bleaf, tleaf;	/* bottom and top (next) leaf index (ring) */

#define	TBUNDLESIZ	409	/* number of twigs in a bundle */

static RTREE	**twigbundle;	/* free twig blocks (NULL term.) */
static int	nexttwig;	/* next free twig */

static RTREE	emptytree;	/* empty tree for test below */

#define	is_stump(t)	(!bcmp((char *)(t), (char *)&emptytree, sizeof(RTREE)))


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

	tmClearHisto();
	bzero((char *)&qtrunk, sizeof(RTREE));
	nexttwig = 0;
	if (twigbundle == NULL)
		return;
	if (!really) {		/* just clear allocated blocks */
		for (i = 0; twigbundle[i] != NULL; i++)
			bzero((char *)twigbundle[i], TBUNDLESIZ*sizeof(RTREE));
		return;
	}
				/* else "really" means free up memory */
	for (i = 0; twigbundle[i] != NULL; i++)
		free((char *)twigbundle[i]);
	free((char *)twigbundle);
	twigbundle = NULL;
}


static RLEAF *
newleaf()			/* allocate a leaf from our pile */
{
	RLEAF	*lp;
	
	lp = leafpile + tleaf++;
	if (tleaf >= nleaves)		/* get next leaf in ring */
		tleaf = 0;
	if (tleaf == bleaf)		/* need to shake some free */
		qtCompost(LFREEPCT);
	return(lp);
}


int
qtAllocLeaves(n)		/* allocate space for n leaves */
int	n;
{
	unsigned	nbytes;
	register unsigned	i;

	qtFreeTree(0);		/* make sure tree is empty */
	if (n <= 0)
		return(0);
	if (nleaves >= n)
		return(nleaves);
	else if (nleaves > 0)
		free((char *)leafpile);
				/* round space up to nearest power of 2 */
	nbytes = n*sizeof(RLEAF) + 8;
	for (i = 1024; nbytes > i; i <<= 1)
		;
	n = (i - 8) / sizeof(RLEAF);
	leafpile = (RLEAF *)malloc(n*sizeof(RLEAF));
	if (leafpile == NULL)
		return(-1);
	nleaves = n;
	bleaf = tleaf = 0;
	return(nleaves);
}


qtFreeLeaves()			/* free our allocated leaves and twigs */
{
	qtFreeTree(1);		/* free tree also */
	if (nleaves <= 0)
		return;
	free((char *)leafpile);
	leafpile = NULL;
	nleaves = 0;
}


static
shaketree(tp)			/* shake dead leaves from tree */
register RTREE	*tp;
{
	register int	i, li;

	for (i = 0; i < 4; i++)
		if (tp->flgs & BRF(i))
			shaketree(tp->k[i].b);
		else if (tp->k[i].l != NULL) {
			li = tp->k[i].l - leafpile;
			if (bleaf < tleaf ? (li < bleaf || li >= tleaf) :
					(li < bleaf && li >= tleaf)) {
				tmAddHisto(&tp->k[i].l->brt, 1, -1);
				tp->k[i].l = NULL;
			}
		}
}


int
qtCompost(pct)			/* free up some leaves */
int	pct;
{
	int	nused, nclear;

	if (is_stump(&qtrunk))
		return(0);
				/* figure out how many leaves to clear */
	nclear = nleaves * pct / 100;
	nused = tleaf > bleaf ? tleaf-bleaf : tleaf+nleaves-bleaf;
	nclear -= nleaves - nused;	/* less what's already free */
	if (nclear <= 0)
		return(0);
	if (nclear >= nused) {	/* clear them all */
		qtFreeTree(0);
		bleaf = tleaf = 0;
		return(nused);
	}
				/* else clear leaves from bottom */
	bleaf += nclear;
	if (bleaf >= nleaves) bleaf -= nleaves;
	shaketree(&qtrunk);
	return(nclear);
}


RLEAF *
qtFindLeaf(x, y)		/* find closest leaf to (x,y) */
int	x, y;
{
	register RTREE	*tp = &qtrunk;
	RLEAF	*lp = NULL;
	int	x0=0, y0=0, x1=odev.hres, y1=odev.vres;
	int	mx, my;
	register int	q;
					/* check limits */
	if (x < 0 || x >= odev.hres || y < 0 || y >= odev.vres)
		return(NULL);
					/* find nearby leaf in our tree */
	for ( ; ; ) {
		for (q = 0; q < 4; q++)		/* find any leaf this level */
			if (!(tp->flgs & BRF(q)) && tp->k[q].l != NULL) {
				lp = tp->k[q].l;
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
		if (tp->k[q].l != NULL)		/* good shot! */
			return(tp->k[q].l);
		return(lp);			/* else return what we have */
	}
}


static
addleaf(lp)			/* add a leaf to our tree */
RLEAF	*lp;
{
	register RTREE	*tp = &qtrunk;
	int	x0=0, y0=0, x1=odev.hres, y1=odev.vres;
	RLEAF	*lo = NULL;
	int	x, y, mx, my;
	double	z;
	FVECT	ip, wp;
	register int	q;
					/* compute leaf location */
	VCOPY(wp, lp->wp);
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
		if (tp->k[q].l == NULL) {	/* found stem for leaf */
			tp->k[q].l = lp;
			tp->flgs |= CHF(q);
			break;
		}	
						/* check existing leaf */
		if (lo != tp->k[q].l) {
			lo = tp->k[q].l;
			VCOPY(wp, lo->wp);
			viewloc(ip, &odev.v, wp);
		}
						/* is node minimum size? */
		if (x1-x0 <= qtMinNodesiz || y1-y0 <= qtMinNodesiz) {
			if (z > (1.-qtDepthEps)*ip[2])	/* who is closer? */
				return;			/* old one is */
			tp->k[q].l = lp;		/* new one is */
			tp->flgs |= CHF(q);
			tmAddHisto(&lo->brt, 1, -1);	/* drop old one */
			break;
		}
		tp->flgs |= CHBRF(q);		/* else grow tree */
		tp = tp->k[q].b = newtwig();
		tp->flgs |= CH_ANY;		/* all new */
		q = 0;				/* old leaf -> new branch */
		mx = ip[0] * odev.hres;
		my = ip[1] * odev.vres;
		if (mx >= (x0 + x1) >> 1) q |= 01;
		if (my >= (y0 + y1) >> 1) q |= 02;
		tp->k[q].l = lo;
	}
	tmAddHisto(&lp->brt, 1, 1);	/* add leaf to histogram */
}


dev_value(c, p)			/* add a pixel value to our output queue */
COLR	c;
FVECT	p;
{
	register RLEAF	*lp;

	lp = newleaf();
	VCOPY(lp->wp, p);
	tmCvColrs(&lp->brt, lp->chr, c, 1);
	addleaf(lp);
}


qtReplant()			/* replant our tree using new view */
{
	register int	i;

	if (bleaf == tleaf)		/* anything to replant? */
		return;
	qtFreeTree(0);			/* blow the tree away */
					/* now rebuild it */
	for (i = bleaf; i != tleaf; ) {
		addleaf(leafpile+i);
		if (++i >= nleaves) i = 0;
	}
	tmComputeMapping(0., 0., 0.);	/* update the display */
	qtUpdate();
}


static
redraw(ca, tp, x0, y0, x1, y1, l)	/* redraw portion of a tree */
BYTE	ca[3];		/* returned average color */
register RTREE	*tp;
int	x0, y0, x1, y1;
int	l[2][2];
{
	int	csm[3], nc;
	BYTE	rgb[3];
	int	quads = CH_ANY;
	int	mx, my;
	register int	i;
					/* compute midpoint */
	mx = (x0 + x1) >> 1;
	my = (y0 + y1) >> 1;
					/* see what to do */
	if (l[0][0] >= mx)
		quads &= ~(CHF(2)|CHF(0));
	else if (l[0][1] <= mx)
		quads &= ~(CHF(3)|CHF(1));
	if (l[1][0] >= my)
		quads &= ~(CHF(1)|CHF(0));
	else if (l[1][1] <= my)
		quads &= ~(CHF(3)|CHF(2));
	tp->flgs &= ~quads;		/* mark them done */
	csm[0] = csm[1] = csm[2] = nc = 0;
					/* do leaves first */
	for (i = 0; i < 4; i++)
		if (quads & CHF(i) && !(tp->flgs & BRF(i)) &&
				tp->k[i].l != NULL) {
			tmMapPixels(rgb, &tp->k[i].l->brt, tp->k[i].l->chr, 1);
			dev_paintr(rgb, i&01 ? mx : x0, i&02 ? my : y0,
					i&01 ? x1 : mx, i&02 ? y1 : my);
			csm[0] += rgb[0]; csm[1] += rgb[1]; csm[2] += rgb[2];
			nc++;
			quads &= ~CHF(i);
		}
					/* now do branches */
	for (i = 0; i < 4; i++)
		if (quads & CHF(i) && tp->flgs & BRF(i)) {
			redraw(rgb, tp->k[i].b, i&01 ? mx : x0, i&02 ? my : y0,
					i&01 ? x1 : mx, i&02 ? y1 : my, l);
			csm[0] += rgb[0]; csm[1] += rgb[1]; csm[2] += rgb[2];
			nc++;
			quads &= ~CHF(i);
		}
	if (nc > 1) {
		ca[0] = csm[0]/nc; ca[1] = csm[1]/nc; ca[2] = csm[2]/nc;
	} else {
		ca[0] = csm[0]; ca[1] = csm[1]; ca[2] = csm[2];
	}
	if (!quads) return;
					/* fill in gaps with average */
	for (i = 0; i < 4; i++)
		if (quads & CHF(i))
			dev_paintr(ca, i&01 ? mx : x0, i&02 ? my : y0,
					i&01 ? x1 : mx, i&02 ? y1 : my);
}


static
update(ca, tp, x0, y0, x1, y1)	/* update tree display as needed */
BYTE	ca[3];		/* returned average color */
register RTREE	*tp;
int	x0, y0, x1, y1;
{
	int	csm[3], nc;
	BYTE	rgb[3];
	int	gaps = 0;
	int	mx, my;
	register int	i;
					/* compute midpoint */
	mx = (x0 + x1) >> 1;
	my = (y0 + y1) >> 1;
	csm[0] = csm[1] = csm[2] = nc = 0;
					/* do leaves first */
	for (i = 0; i < 4; i++)
		if ((tp->flgs & CHBRF(i)) == CHF(i)) {
			if (tp->k[i].l == NULL) {
				gaps |= 1<<i;	/* empty stem */
				continue;
			}
			tmMapPixels(rgb, &tp->k[i].l->brt, tp->k[i].l->chr, 1);
			dev_paintr(rgb, i&01 ? mx : x0, i&02 ? my : y0,
					i&01 ? x1 : mx, i&02 ? y1 : my);
			csm[0] += rgb[0]; csm[1] += rgb[1]; csm[2] += rgb[2];
			nc++;
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


qtRedraw(x0, y0, x1, y1)	/* redraw part of our screen */
int	x0, y0, x1, y1;
{
	int	lim[2][2];
	BYTE	ca[3];

	if (is_stump(&qtrunk))
		return;
	if ((lim[0][0]=x0) <= 0 & (lim[1][0]=y0) <= 0 &
		(lim[0][1]=x1) >= odev.hres-1 & (lim[1][1]=y1) >= odev.vres-1
			|| tmTop->lumap == NULL)
		if (tmComputeMapping(0., 0., 0.) != TM_E_OK)
			return;
	redraw(ca, &qtrunk, 0, 0, odev.hres, odev.vres, lim);
}


qtUpdate()			/* update our tree display */
{
	BYTE	ca[3];

	if (is_stump(&qtrunk))
		return;
	if (tmTop->lumap == NULL)
		tmComputeMapping(0., 0., 0.);
	update(ca, &qtrunk, 0, 0, odev.hres, odev.vres);
}
