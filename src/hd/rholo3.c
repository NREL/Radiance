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

static PACKHEAD	*complist=NULL;	/* list of beams to compute */
static int	complen=0;	/* length of complist */
static int	listpos=0;	/* current list position for next_packet */
static int	lastin= -1;	/* last ordered position in list */


int
beamcmp(b0, b1)			/* comparison for descending compute order */
register PACKHEAD	*b0, *b1;
{
	return(	b1->nr*(b0->nc+1) - b0->nr*(b1->nc+1) );
}


int
dispbeam(b, hp, bi)			/* display a holodeck beam */
register BEAM	*b;
HOLO	*hp;
int	bi;
{
	static int	n = 0;
	static PACKHEAD	*p = NULL;

	if (b == NULL)
		return;
	if (b->nrm > n) {		/* (re)allocate packet holder */
		n = b->nrm;
		if (p == NULL) p = (PACKHEAD *)malloc(packsiz(n));
		else p = (PACKHEAD *)realloc((char *)p, packsiz(n));
		if (p == NULL)
			error(SYSTEM, "out of memory in dispbeam");
	}
					/* assign packet fields */
	bcopy((char *)hdbray(b), (char *)packra(p), b->nrm*sizeof(RAYVAL));
	p->nr = p->nc = b->nrm;
	for (p->hd = 0; hdlist[p->hd] != hp; p->hd++)
		if (hdlist[p->hd] == NULL)
			error(CONSISTENCY, "unregistered holodeck in dispbeam");
	p->bi = bi;
	disp_packet(p);			/* display it */
}


bundle_set(op, clist, nents)	/* bundle set operation */
int	op;
register PACKHEAD	*clist;
int	nents;
{
	register int	i, n;

	switch (op) {
	case BS_NEW:			/* new computation set */
		if (complen)
			free((char *)complist);
		if (nents <= 0) {
			complist = NULL;
			listpos = complen = 0;
			lastin = -1;
			return;
		}
		complist = (PACKHEAD *)malloc(nents*sizeof(PACKHEAD));
		if (complist == NULL)
			goto memerr;
		bcopy((char *)clist, (char *)complist, nents*sizeof(PACKHEAD));
		complen = nents;	/* finish initialization below */
		break;
	case BS_ADD:			/* add to computation set */
	case BS_ADJ:			/* adjust set quantities */
		if (nents <= 0)
			return;
					/* merge any common members */
		for (n = 0; n < nents; n++) {
			for (i = 0; i < complen; i++)
				if (clist[n].bi == complist[i].bi &&
						clist[n].hd == complist[i].hd) {
					int	oldnr = complist[i].nr;
					if (op == BS_ADD)
						complist[i].nr += clist[n].nr;
					else /* op == BS_ADJ */
						complist[i].nr = clist[n].nr;
					clist[n].nr = 0;
					clist[n].nc = complist[i].nc;
					if (complist[i].nr != oldnr)
						lastin = -1;	/* flag sort */
					break;
				}
			if (i >= complen)
				clist[n].nc = bnrays(hdlist[clist[n].hd],
						clist[n].bi);
		}
					/* sort updated list */
		sortcomplist();
					/* sort new entries */
		qsort((char *)clist, nents, sizeof(PACKHEAD), beamcmp);
					/* what can't we satisfy? */
		for (n = 0; n < nents && clist[n].nr > clist[n].nc; n++)
			;
		if (op == BS_ADJ)
			nents = n;
		if (n) {		/* allocate space for merged list */
			PACKHEAD	*newlist;
			newlist = (PACKHEAD *)malloc(
					(complen+n)*sizeof(PACKHEAD) );
			if (newlist == NULL)
				goto memerr;
						/* merge lists */
			mergeclists(newlist, clist, n, complist, complen);
			if (complen)
				free((char *)complist);
			complist = newlist;
			complen += n;
		}
		listpos = 0;
		lastin = complen-1;	/* list is now sorted */
		break;
	case BS_DEL:			/* delete from computation set */
		if (nents <= 0)
			return;
					/* find each member */
		for (i = 0; i < complen; i++)
			for (n = 0; n < nents; n++)
				if (clist[n].bi == complist[i].bi &&
						clist[n].hd == complist[i].hd) {
					if (clist[n].nr == 0 ||
						clist[n].nr >= complist[i].nr)
						complist[i].nr = 0;
					else
						complist[i].nr -= clist[n].nr;
					lastin = -1;	/* flag full sort */
					break;
				}
		return;			/* no display */
	default:
		error(CONSISTENCY, "bundle_set called with unknown operation");
	}
	if (outdev != NULL) {		/* load and display beams we have */
		register HDBEAMI	*hb;

		hb = (HDBEAMI *)malloc(nents*sizeof(HDBEAMI));
		for (i = 0; i < nents; i++) {
			hb[i].h = hdlist[clist[i].hd];
			hb[i].b = clist[i].bi;
		}
		hdloadbeams(hb, nents, dispbeam);
		free((char *)hb);
	}
	if (op == BS_NEW) {
		done_packets(flush_queue());	/* empty queue, so we can... */
		for (i = 0; i < complen; i++)	/* ...get number computed */
			complist[i].nc = bnrays(hdlist[complist[i].hd],
						complist[i].bi);
		listpos = 0;
		lastin = -1;		/* flag for initial sort */
	}
	return;
memerr:
	error(SYSTEM, "out of memory in bundle_set");
}


double
beamvolume(hp, bi)	/* compute approximate volume of a beam */
HOLO	*hp;
int	bi;
{
	GCOORD	gc[2];
	FVECT	cp[4], edgeA, edgeB, cent[2];
	FVECT	v, crossp[2], diffv;
	double	vol[2];
	register int	i;
					/* get grid coordinates */
	if (!hdbcoord(gc, hp, bi))
		error(CONSISTENCY, "bad beam index in beamvolume");
	for (i = 0; i < 2; i++) {	/* compute cell area vectors */
		hdcell(cp, hp, gc+i);
		VSUM(edgeA, cp[1], cp[0], -1.0);
		VSUM(edgeB, cp[3], cp[1], -1.0);
		fcross(crossp[i], edgeA, edgeB);
					/* compute center */
		cent[i][0] = 0.5*(cp[0][0] + cp[2][0]);
		cent[i][1] = 0.5*(cp[0][1] + cp[2][1]);
		cent[i][2] = 0.5*(cp[0][2] + cp[2][2]);
	}
					/* compute difference vector */
	VSUM(diffv, cent[1], cent[0], -1.0);
	for (i = 0; i < 2; i++) {	/* compute volume contributions */
		vol[i] = 0.5*DOT(crossp[i], diffv);
		if (vol[i] < 0.) vol[i] = -vol[i];
	}
	return(vol[0] + vol[1]);	/* return total volume */
}


init_global()			/* initialize global ray computation */
{
	long	wtotal = 0;
	double	frac;
	int	i;
	register int	j, k;
					/* free old list */
	if (complen > 0)
		free((char *)complist);
					/* allocate beam list */
	complen = 0;
	for (j = 0; hdlist[j] != NULL; j++)
		complen += nbeams(hdlist[j]);
	complist = (PACKHEAD *)malloc(complen*sizeof(PACKHEAD));
	if (complist == NULL)
		error(SYSTEM, "out of memory in init_global");
					/* compute beam weights */
	k = 0;
	for (j = 0; hdlist[j] != NULL; j++) {
		frac = 512. * hdlist[j]->wg[0] *
				hdlist[j]->wg[1] * hdlist[j]->wg[2];
		if (frac < 0.) frac = -frac;
		for (i = nbeams(hdlist[j]); i > 0; i--) {
			complist[k].hd = j;
			complist[k].bi = i;
			complist[k].nr = frac*beamvolume(hdlist[j], i) + 0.5;
			wtotal += complist[k++].nr;
		}
	}
					/* adjust weights */
	if (vdef(DISKSPACE))
		frac = 1024.*1024.*vflt(DISKSPACE) / (wtotal*sizeof(RAYVAL));
	else
		frac = 1024.*1024.*16384. / (wtotal*sizeof(RAYVAL));
	while (k--)
		complist[k].nr = frac * complist[k].nr;
	listpos = 0; lastin = -1;	/* flag initial sort */
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
	register int	i;

	if (complen <= 0)	/* check to see if there is even a list */
		return;
	if (lastin < 0 || listpos*4 >= complen*3)
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
					/* drop satisfied requests */
	for (i = complen; i-- && complist[i].nr <= complist[i].nc; )
		;
	if (i < 0) {
		free((char *)complist);
		complist = NULL;
		complen = 0;
	} else if (i < complen-1) {
		list2 = (PACKHEAD *)realloc((char *)complist,
				(i+1)*sizeof(PACKHEAD));
		if (list2 != NULL) {
			complist = list2;
			complen = i+1;
		}
	}
	listpos = 0; lastin = i;
}


/*
 * The following routine works on the assumption that the bundle weights are
 * more or less evenly distributed, such that computing a packet causes
 * a given bundle to move way down in the computation order.  We keep
 * track of where the computed bundle with the highest priority would end
 * up, and if we get further in our compute list than this, we resort the
 * list and start again from the beginning.  Since
 * a merge sort is used, the sorting costs are minimal.
 */
next_packet(p)			/* prepare packet for computation */
register PACKET	*p;
{
	register int	i;

	if (listpos > lastin)		/* time to sort the list */
		sortcomplist();
	if (complen <= 0)
		return(0);
	p->hd = complist[listpos].hd;
	p->bi = complist[listpos].bi;
	p->nc = complist[listpos].nc;
	p->nr = complist[listpos].nr - p->nc;
	if (p->nr <= 0)
		return(0);
	if (p->nr > RPACKSIZ)
		p->nr = RPACKSIZ;
	complist[listpos].nc += p->nr;	/* find where this one would go */
	while (lastin > listpos && 
			beamcmp(complist+lastin, complist+listpos) > 0)
		lastin--;
	listpos++;
	return(1);
}
