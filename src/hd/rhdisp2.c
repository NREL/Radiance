/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Holodeck beam tracking for display process
 */

#include "rholo.h"
#include "rhdisp.h"
#include "rhdriver.h"

#ifndef MAXDIST
#define MAXDIST		42	/* maximum distance outside section */
#endif

#define MAXVOXEL	32	/* maximum number of active voxels */

typedef struct {
	int	hd;		/* holodeck section number (-1 if inactive) */
	int	i[3];		/* voxel index (may be outside section) */
} VOXL;			/* a voxel */

static VOXL voxel[MAXVOXEL] = {{-1}};	/* current voxel list */

#define CBEAMBLK	1024	/* cbeam allocation block size */

static struct beamcomp {
	int	hd;		/* holodeck section number */
	int	bi;		/* beam index */
	int4	nr;		/* number of samples desired */
} *cbeam = NULL;	/* current beam list */

static int	ncbeams = 0;	/* number of sorted beams in cbeam */
static int	xcbeams = 0;	/* extra (unregistered) beams past ncbeams */
static int	maxcbeam = 0;	/* size of cbeam array */

struct cellact {
	short	vi;		/* voxel index */
	short	rev;		/* reverse ray direction? */
	VIEW	*vp;		/* view for image */
	short	hr, vr;		/* image resolution */
};		/* action for cell */

struct beamact {
	struct cellact	ca;	/* cell action */
	GCOORD	gc;		/* grid coordinate */
};		/* beam origin and action */


int
newcbeam()		/* allocate new entry at end of cbeam array */
{
	int	i;

	if ((i = ncbeams + xcbeams++) >= maxcbeam) {	/* grow array */
		maxcbeam += CBEAMBLK;
		if (cbeam == NULL)
			cbeam = (struct beamcomp *)malloc(
					maxcbeam*sizeof(struct beamcomp) );
		else
			cbeam = (struct beamcomp *)realloc( (char *)cbeam,
					maxcbeam*sizeof(struct beamcomp) );
		if (cbeam == NULL)
			error(SYSTEM, "out of memory in newcbeam");
	}
	return(i);
}


int
cbeamcmp(cb1, cb2)	/* compare two cbeam entries for sort: keep orphans */
register struct beamcomp	*cb1, *cb2;
{
	register int	c;

	if ((c = cb1->bi - cb2->bi))	/* sort on beam index first */
		return(c);
	return(cb1->hd - cb2->hd);	/* use hd to resolve matches */
}


int
cbeamcmp2(cb1, cb2)	/* compare two cbeam entries for sort: no orphans */
register struct beamcomp	*cb1, *cb2;
{
	register int	c;

	if (!cb1->nr)			/* put orphans at the end, unsorted */
		return(cb2->nr);
	if (!cb2->nr)
		return(-1);
	if ((c = cb1->bi - cb2->bi))	/* sort on beam index first */
		return(c);
	return(cb1->hd - cb2->hd);	/* use hd to resolve matches */
}


int
findcbeam(hd, bi)	/* find the specified beam in our sorted list */
int	hd, bi;
{
	struct beamcomp	cb;
	register struct beamcomp	*p;

	if (ncbeams <= 0)
		return(-1);
	cb.hd = hd; cb.bi = bi; cb.nr = 0;
	p = (struct beamcomp *)bsearch((char *)&cb, (char *)cbeam, ncbeams,
			sizeof(struct beamcomp), cbeamcmp);
	if (p == NULL)
		return(-1);
	return(p - cbeam);
}


int
getcbeam(hd, bi)	/* get the specified beam, allocating as necessary */
register int	hd;
int	bi;
{
	register int	n;
				/* first, look in sorted list */
	if ((n = findcbeam(hd, bi)) >= 0)
		return(n);
				/* linear search through xcbeams to be sure */
	for (n = ncbeams+xcbeams; n-- > ncbeams; )
		if (cbeam[n].bi == bi && cbeam[n].hd == hd)
			return(n);
				/* check legality */
	if (hd < 0 | hd >= HDMAX || hdlist[hd] == NULL)
		error(INTERNAL, "illegal holodeck number in getcbeam");
	if (bi < 1 | bi > nbeams(hdlist[hd]))
		error(INTERNAL, "illegal beam index in getcbeam");
	n = newcbeam();		/* allocate and assign */
	cbeam[n].nr = 0; cbeam[n].hd = hd; cbeam[n].bi = bi;
	return(n);
}


cbeamsort(adopt)	/* sort our beam list, possibly turning out orphans */
int	adopt;
{
	register int	i;

	if (!(ncbeams += xcbeams))
		return;
	xcbeams = 0;
	qsort((char *)cbeam, ncbeams, sizeof(struct beamcomp),
			adopt ? cbeamcmp : cbeamcmp2);
	if (adopt)
		return;
	for (i = ncbeams; i--; )	/* identify orphans */
		if (cbeam[i].nr)
			break;
	xcbeams = ncbeams - ++i;	/* put orphans after ncbeams */
	ncbeams = i;
}


cbeamop(op, bl, n)		/* update beams on server list */
int	op;
register struct beamcomp	*bl;
int	n;
{
	register PACKHEAD	*pa;
	register int	i;

	if (n <= 0)
		return;
	pa = (PACKHEAD *)malloc(n*sizeof(PACKHEAD));
	if (pa == NULL)
		error(SYSTEM, "out of memory in cbeamop");
	for (i = 0; i < n; i++) {
		pa[i].hd = bl[i].hd;
		pa[i].bi = bl[i].bi;
		pa[i].nr = bl[i].nr;
		pa[i].nc = 0;
	}
	serv_request(op, n*sizeof(PACKHEAD), (char *)pa);
	free((char *)pa);
}


unsigned int4
add_voxels(vp)		/* add voxels corresponding to view point */
FVECT	vp;
{
	int	first, n, rfl = 0;
	FVECT	gp;
	double	d;
	int	dist, bestd = 0x7fff;
	VOXL	vox;
	register int	i, j, k;
					/* count voxels in list already */
	for (first = 0; first < MAXVOXEL && voxel[first].hd >= 0; first++)
		;
					/* find closest voxels */
	for (n = first, i = 0; n < MAXVOXEL && hdlist[i]; i++) {
		hdgrid(gp, hdlist[i], vp);
		for (j = 0; n < MAXVOXEL && j < 8; j++) {
			dist = 0;
			for (k = 0; k < 3; k++) {
				d = gp[k] - .5 + (j>>k & 1);
				if (d < 0.)
					dist += -(vox.i[k] = (int)d - 1);
				else if ((vox.i[k] = d) >= hdlist[i]->grid[k])
					dist += vox.i[k] -
							hdlist[i]->grid[k] + 1;
			}
			if (dist > bestd)	/* others were closer */
				continue;
			if (dist < bestd) {	/* start closer list */
				n = first;
				bestd = dist;
				rfl = 0;
			}
						/* check if already in list */
			for (k = first; k--; )
				if (voxel[k].hd == i &&
						voxel[k].i[0] == vox.i[0] &&
						voxel[k].i[1] == vox.i[1] &&
						voxel[k].i[2] == vox.i[2])
					break;
			if (k >= 0) {
				rfl |= 1<<k;
				continue;
			}
			vox.hd = i;		/* add this voxel */
			copystruct(&voxel[n], &vox);
			rfl |= 1<<n++;
		}
	}
					/* check for really stupid move */
	if (bestd > MAXDIST) {
		error(COMMAND, "move past outer limits");
		return(0);
	}
	if (n < MAXVOXEL)
		voxel[n].hd = -1;
	return(rfl);
}


int
dobeam(gcp, bp)		/* express interest in a beam */
GCOORD	*gcp;
register struct beamact	*bp;
{
	GCOORD	gc[2];
	int	bi, i, n;
					/* compute beam index */
	if (bp->ca.rev) {
		copystruct(gc, &bp->gc);
		copystruct(gc+1, gcp);
	} else {
		copystruct(gc, gcp);
		copystruct(gc+1, &bp->gc);
	}
	if ((bi = hdbindex(hdlist[voxel[bp->ca.vi].hd], gc)) <= 0)
		error(CONSISTENCY, "bad grid coordinate in dobeam");
					/* add it in */
	i = getcbeam(voxel[bp->ca.vi].hd, bi);
	n = npixels(bp->ca.vp, bp->ca.hr, bp->ca.vr,
			hdlist[cbeam[i].hd], cbeam[i].bi);
	if (n > cbeam[i].nr)
		cbeam[i].nr = n;
	return(i == ncbeams+xcbeams-1);	/* return 1 if totally new */
}



int
docell(gcp, cap)	/* find beams corresponding to cell and voxel */
GCOORD	*gcp;
register struct cellact	*cap;
{
	register HOLO	*hp = hdlist[voxel[cap->vi].hd];
	FVECT	org, dir[4];
	FVECT	vgp, cgp, vc;
	FVECT	v1, v2;
	struct beamact	bo;
	int	axmax, j;
	double	d, avmax;
	register int	i;
				/* compute cell center */
	cgp[gcp->w>>1] = gcp->w&1 ? hp->grid[gcp->w>>1] : 0 ;
	cgp[hdwg0[gcp->w]] = gcp->i[0] + .5;
	cgp[hdwg1[gcp->w]] = gcp->i[1] + .5;
	hdworld(org, hp, cgp);
				/* compute direction to voxel center */
	for (i = 3; i--; )
		vgp[i] = voxel[cap->vi].i[i] + .5;
	hdworld(vc, hp, vgp);
	for (i = 3; i--; )
		vc[i] -= org[i];
				/* compute maximum area axis */
	axmax = -1; avmax = 0;
	for (i = 3; i--; ) {
		d = vgp[i] - cgp[i];
		if (d < 0.) d = -d;
		if (d > avmax) {
			avmax = d;
			axmax = i;
		}
	}
#ifdef DEBUG
	if (axmax < 0)
		error(CONSISTENCY, "botched axis computation in docell");
#endif
				/* compute offset vectors */
	d = 0.5/hp->grid[j=(axmax+1)%3];
	for (i = 3; i--; )
		v1[i] = hp->xv[j][i] * d;
	d = 0.5/hp->grid[j=(axmax+2)%3];
	if (DOT(hp->wg[axmax], vc) < 0.)
		d = -d;	/* reverse vertex order */
	for (i = 3; i--; )
		v2[i] = hp->xv[j][i] * d;
				/* compute voxel pyramid */
	for (i = 3; i--; ) {
		dir[0][i] = vc[i] - v1[i] - v2[i];
		dir[1][i] = vc[i] + v1[i] - v2[i];
		dir[2][i] = vc[i] + v1[i] + v2[i];
		dir[3][i] = vc[i] - v1[i] + v2[i];
	}
				/* visit beams for opposite cells */
	copystruct(&bo.ca, cap);
	copystruct(&bo.gc, gcp);
	return(visit_cells(org, dir, hp, dobeam, &bo));
}


int
doview(cap)			/* visit cells for a given view */
struct cellact	*cap;
{
	int	orient;
	FVECT	org, dir[4];
					/* compute view pyramid */
	orient = viewpyramid(org, dir, hdlist[voxel[cap->vi].hd], cap->vp);
	if (!orient)
		error(INTERNAL, "unusable view in doview");
	cap->rev = orient < 0;
					/* visit cells within pyramid */
	return(visit_cells(org, dir, hdlist[voxel[cap->vi].hd], docell, cap));
}


beam_init(fresh)		/* clear beam list for new view(s) */
int	fresh;
{
	register int	i;

	if (fresh)			/* discard old beams? */
		ncbeams = xcbeams = 0;
	else				/* else clear sample requests */
		for (i = ncbeams+xcbeams; i--; )
			cbeam[i].nr = 0;
	voxel[0].hd = -1;		/* clear voxel list */
}


beam_view(vn, hr, vr)		/* add beam view (if advisable) */
VIEW	*vn;
int	hr, vr;
{
	struct cellact	ca;
	unsigned int4	vfl;
					/* sort our list */
	cbeamsort(1);
					/* add new voxels */
	vfl = add_voxels(vn->vp);
	if (!vfl)
		return(0);
	ca.vp = vn; ca.hr = hr; ca.vr = vr;
					/* update our beam list */
	for (ca.vi = 0; vfl; vfl >>= 1, ca.vi++)
		if (vfl & 1)
			doview(&ca);
	return(1);
}


int
beam_sync(all)			/* update beam list on server */
int	all;
{
					/* sort list (put orphans at end) */
	cbeamsort(all < 0);
	if (all)
		cbeamop(DR_NEWSET, cbeam, ncbeams);
	else
		cbeamop(DR_ADJSET, cbeam, ncbeams+xcbeams);
	xcbeams = 0;			/* truncate our list */
	return(ncbeams);
}
