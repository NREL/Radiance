/* Copyright (c) 1997 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Routines for tracking beam compuatations
 */

#include "rholo.h"


#define abs(x)		((x) > 0 ? (x) : -(x))
#define sgn(x)		((x) > 0 ? 1 : (x) < 0 ? -1 : 0)


static PACKHEAD	*complist;	/* list of beams to compute */
static int	complen;	/* length of complist */
static int	listpos;	/* current list position for next_pack */
static int	lastin;		/* last ordered position in list */


int
weightf(hp, x0, x1, x2)		/* voxel weighting function */
register HOLO	*hp;
register int	x0, x1, x2;
{
	switch (vlet(OCCUPANCY)) {
	case 'U':		/* uniform weighting */
		return(1);
	case 'C':		/* center weighting (crude) */
		x0 += x0 - hp->grid[0] + 1;
		x0 = abs(x0)*hp->grid[1]*hp->grid[2];
		x1 += x1 - hp->grid[1] + 1;
		x1 = abs(x1)*hp->grid[0]*hp->grid[2];
		x2 += x2 - hp->grid[2] + 1;
		x2 = abs(x2)*hp->grid[0]*hp->grid[1];
		return(hp->grid[0]*hp->grid[1]*hp->grid[2] -
				(x0+x1+x2)/3);
	default:
		badvalue(OCCUPANCY);
	}
}


/* The following is by Daniel Cohen, taken from Graphics Gems IV, p. 368 */
long
lineweight(hp, x, y, z, dx, dy, dz)	/* compute weights along a line */
HOLO	*hp;
int	x, y, z, dx, dy, dz;
{
	long	wres = 0;
	int	n, sx, sy, sz, exy, exz, ezy, ax, ay, az, bx, by, bz;

	sx = sgn(dx);	sy = sgn(dy);	sz = sgn(dz);
	ax = abs(dx);	ay = abs(dy);	az = abs(dz);
	bx = 2*ax;	by = 2*ay;	bz = 2*az;
	exy = ay-ax;	exz = az-ax;	ezy = ay-az;
	n = ax+ay+az + 1;		/* added increment to visit last */
	while (n--) {
		wres += weightf(hp, x, y, z);
		if (exy < 0) {
			if (exz < 0) {
				x += sx;
				exy += by; exz += bz;
			} else {
				z += sz;
				exz -= bx; ezy += by;
			}
		} else {
			if (ezy < 0) {
				z += sz;
				exz -= bx; ezy += by;
			} else {
				y += sy;
				exy -= bx; ezy -= bz;
			}
		}
	}
	return(wres);
}


int
beamcmp(b0, b1)			/* comparison for descending compute order */
register PACKHEAD	*b0, *b1;
{
	return(	b1->nr*(bnrays(hdlist[b0->hd],b0->bi)+1) -
		b0->nr*(bnrays(hdlist[b1->hd],b1->bi)+1) );
}


init_global()			/* initialize global ray computation */
{
	long	wtotal = 0;
	int	i, j;
	int	lseg[2][3];
	double	frac;
	register int	k;
					/* allocate beam list */
	complen = 0;
	for (j = 0; hdlist[j] != NULL; j++)
		complen += nbeams(hdlist[j]);
	complist = (PACKHEAD *)malloc(complen*sizeof(PACKHEAD));
	if (complist == NULL)
		error(SYSTEM, "out of memory in init_global");
					/* compute beam weights */
	k = 0;
	for (j = 0; hdlist[j] != NULL; j++)
		for (i = nbeams(hdlist[j]); i > 0; i--) {
			hdlseg(lseg, hdlist[j], i);
			complist[k].hd = j;
			complist[k].bi = i;
			complist[k].nr = lineweight( hdlist[j],
					lseg[0][0], lseg[0][1], lseg[0][2],
					lseg[1][0] - lseg[0][0],
					lseg[1][1] - lseg[0][1],
					lseg[1][2] - lseg[0][2] );
			wtotal += complist[k++].nr;
		}
					/* adjust weights */
	if (vdef(DISKSPACE)) {
		frac = 1024.*1024.*vflt(DISKSPACE) / (wtotal*sizeof(RAYVAL));
		if (frac < 0.95 | frac > 1.05)
			while (k--)
				complist[k].nr = frac * complist[k].nr;
	}
	listpos = 0; lastin = -1;
}


mergeclists(cdest, cl1, n1, cl2, n2)	/* merge two sorted lists */
PACKHEAD	*cdest;
PACKHEAD	*cl1, *cl2;
int	n1, n2;
{
	int	cmp;

	while (n1 | n2) {
		if (!n1) cmp = 1;
		else if (!n2) cmp = -1;
		else cmp = beamcmp(cl1, cl2);
		if (cmp > 0) {
			copystruct(cdest, cl2);
			cl2++; n2--;
		} else {
			copystruct(cdest, cl1);
			cl1++; n1--;
		}
		cdest++;
	}
}


sortcomplist()			/* fix our list order */
{
	PACKHEAD	*list2;
				/* empty queue */
	done_packets(flush_queue());
	if (lastin < 0)		/* flag to sort entire list */
		qsort((char *)complist, complen, sizeof(PACKHEAD), beamcmp);
	else if (listpos) {	/* else sort and merge sublist */
		list2 = (PACKHEAD *)malloc(listpos*sizeof(PACKHEAD));
		if (list2 == NULL)
			error(SYSTEM, "out of memory in sortcomplist");
		bcopy((char *)complist,(char *)list2,listpos*sizeof(PACKHEAD));
		qsort((char *)list2, listpos, sizeof(PACKHEAD), beamcmp);
		mergeclists(complist, list2, listpos,
				complist+listpos, complen-listpos);
		free((char *)list2);
	}
	listpos = 0; lastin = complen-1;
}


/*
 * The following routine works on the assumption that the bundle weights are
 * more or less evenly distributed, such that computing a packet causes
 * a given bundle to move way down in the computation order.  We keep
 * track of where the computed bundle with the highest priority would end
 * up, and if we get further in our compute list than this, we resort the
 * list and start again from the beginning.  We have to flush the queue
 * each time we sort, to ensure that we are not disturbing the order.
 *	If our major assumption is violated, and we have a very steep
 * descent in our weights, then we will end up resorting much more often
 * than necessary, resulting in frequent flushing of the queue.  Since
 * a merge sort is used, the sorting costs will be minimal.
 */
next_packet(p)			/* prepare packet for computation */
register PACKET	*p;
{
	int	ncomp;
	register int	i;

	if (complen <= 0)
		return(0);
	if (listpos > lastin)		/* time to sort the list */
		sortcomplist();
	p->hd = complist[listpos].hd;
	p->bi = complist[listpos].bi;
	ncomp = bnrays(hdlist[p->hd],p->bi);
	p->nr = complist[listpos].nr - ncomp;
	if (p->nr <= 0)
		return(0);
	if (p->nr > RPACKSIZ)
		p->nr = RPACKSIZ;
	ncomp += p->nr;			/* find where this one would go */
	while (lastin > listpos && complist[listpos].nr *
		(bnrays(hdlist[complist[lastin].hd],complist[lastin].bi)+1)
			> complist[lastin].nr * (ncomp+1))
		lastin--;
	listpos++;
	return(1);
}
