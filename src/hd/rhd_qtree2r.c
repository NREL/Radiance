/* Copyright (c) 1997 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Quadtree display support routines for rectangle output.
 */

#include "standard.h"
#include "rhd_qtree.h"


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
