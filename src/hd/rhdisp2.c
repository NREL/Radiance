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

extern GCOORD	*getviewcells();

static VIEW	dvw;		/* current view corresponding to beam list */

typedef struct {
	int	hd;		/* holodeck section number (-1 if inactive) */
	int	i[3];		/* voxel index (may be outside section) */
} VOXL;			/* a voxel */

static VOXL voxel[8] = {	/* current voxel list */
	{-1}, {-1}, {-1}, {-1},
	{-1}, {-1}, {-1}, {-1}
};

#define CBEAMBLK	1024	/* cbeam allocation block size */

static struct beamcomp {
	short	wants;		/* flags telling which voxels want us */
	short	hd;		/* holodeck section number */
	int	bi;		/* beam index */
} *cbeam = NULL;	/* current beam list */

static int	ncbeams = 0;	/* number of sorted beams in cbeam */
static int	xcbeams = 0;	/* extra (unregistered) beams past ncbeams */
static int	maxcbeam = 0;	/* size of cbeam array */

struct cellact {
	short	vi;		/* voxel index */
	short	add;		/* zero means delete */
	short	rev;		/* reverse ray direction? */
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

	if (!cb1->wants)		/* put orphans at the end, unsorted */
		return(cb2->wants);
	if (!cb2->wants)
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
	cb.wants = 0; cb.hd = hd; cb.bi = bi;
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
	cbeam[n].wants = 0; cbeam[n].hd = hd; cbeam[n].bi = bi;
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
		if (cbeam[i].wants)
			break;
	xcbeams = ncbeams - ++i;	/* put orphans after ncbeams */
	ncbeams = i;
}


cbeamop(op, bl, n, v, hr, vr)	/* update beams on server list */
int	op;
register struct beamcomp	*bl;
int	n;
VIEW	*v;
int	hr, vr;
{
	register PACKHEAD	*pa;
	register int	i;

	if (n <= 0)
		return;
	pa = (PACKHEAD *)malloc(n*sizeof(PACKHEAD));
	if (pa == NULL)
		error(SYSTEM, "out of memory in cbeamadd");
	for (i = 0; i < n; i++) {
		pa[i].hd = bl[i].hd;
		pa[i].bi = bl[i].bi;
		pa[i].nr = v==NULL ? 0 :
				npixels(v, hr, vr, hdlist[bl[i].hd], bl[i].bi);
		pa[i].nc = 0;
	}
	serv_request(op, n*sizeof(PACKHEAD), (char *)pa);
	free((char *)pa);
}


int
set_voxels(vl, n)	/* set new voxel array */
VOXL	vl[8];
int	n;
{
	short	wmap[256];
	int	vmap[8];
	int	comn = 0;
	int	no;
	register int	i, j;
					/* find common voxels */
	for (j = 0; j < 8 && voxel[j].hd >= 0; j++) {
		vmap[j] = -1;
		for (i = n; i--; )
			if (!bcmp((char *)(vl+i), (char *)(voxel+j),
					sizeof(VOXL))) {
				vmap[j] = i;
				comn |= 1<<i;
				break;
			}
	}
	no = comn ? j : 0;		/* compute flag mapping */
	for (i = 256; i--; ) {
		wmap[i] = 0;
		for (j = no; j--; )
			if (vmap[j] >= 0 && i & 1<<j)
				wmap[i] |= 1<<vmap[j];
	}
					/* fix cbeam flags */
	for (i = ncbeams; i--; )
		cbeam[i].wants = wmap[cbeam[i].wants];
					/* update our voxel list */
	bcopy((char *)vl, (char *)voxel, n*sizeof(VOXL));
	for (j = n; j < 8; j++)
		voxel[j].hd = -1;
	return(comn);			/* return bit array of common voxels */
}


int
get_voxels(vl, vp)	/* find voxels corresponding to view point */
VOXL	vl[8];
FVECT	vp;
{
	int	n = 0;
	FVECT	gp;
	double	d;
	int	dist, bestd = 0x7fff;
	VOXL	vox;
	register int	i, j, k;
					/* find closest voxels */
	for (i = 0; n < 8 && hdlist[i]; i++) {
		hdgrid(gp, hdlist[i], vp);
		for (j = 0; n < 8 && j < 8; j++) {
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
				n = 0;
				bestd = dist;
			}
			vox.hd = i;		/* add this voxel */
			copystruct(&vl[n], &vox);
			n++;
		}
	}
					/* check for really stupid move */
	if (bestd > MAXDIST) {
		error(COMMAND, "move past outer limits");
		return(0);
	}
	return(n);
}


int
dobeam(gcp, bp)		/* express interest or disintrest in a beam */
GCOORD	*gcp;
register struct beamact	*bp;
{
	GCOORD	gc[2];
	int	bi, i;
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
	if (bp->ca.add) {		/* add it in */
		i = getcbeam(voxel[bp->ca.vi].hd, bi);
		cbeam[i].wants |= 1<<bp->ca.vi;	/* say we want it */
		return(i == ncbeams+xcbeams-1);	/* return 1 if totally new */
	}
					/* else delete it */
	i = findcbeam(voxel[bp->ca.vi].hd, bi);
	if (i >= 0 && cbeam[i].wants & 1<<bp->ca.vi) {
		cbeam[i].wants &= ~(1<<bp->ca.vi);	/* we don't want it */
		return(1);			/* indicate change */
	}
	return(0);
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
	if (axmax < 0.)
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
doview(cap, vp)			/* visit cells for a given view */
struct cellact	*cap;
VIEW	*vp;
{
	int	orient;
	FVECT	org, dir[4];
					/* compute view pyramid */
	orient = viewpyramid(org, dir, hdlist[voxel[cap->vi].hd], vp);
	if (!orient)
		error(INTERNAL, "unusable view in doview");
	cap->rev = orient < 0;
					/* visit cells within pyramid */
	return(visit_cells(org, dir, hdlist[voxel[cap->vi].hd], docell, cap));
}


mvview(voxi, vold, vnew)	/* move view for a voxel */
int	voxi;
VIEW	*vold, *vnew;
{
	int	netchange = 0;
	struct cellact	oca, nca;
	int	ocnt, ncnt;
	int	nmatch = 0;
	GCOORD	*ogcl, *ngcl;
	register int	c;
	register GCOORD	*ogcp, *ngcp;
				/* get old and new cell lists */
	ogcp = ogcl = getviewcells(&ocnt, hdlist[voxel[voxi].hd], vold);
	ngcp = ngcl = getviewcells(&ncnt, hdlist[voxel[voxi].hd], vnew);
				/* set up actions */
	oca.vi = nca.vi = voxi;
	oca.add = 0; nca.add = 1;
	if ((oca.rev = ocnt < 0))
		ocnt = -ocnt;
	if ((nca.rev = ncnt < 0))
		ncnt = -ncnt;
	if (oca.rev == nca.rev) {	/* count matches */
		int	oc = ocnt, nc = ncnt;
		while (oc > 0 & nc > 0) {
			c = cellcmp(ogcp, ngcp);
			if (c >= 0) { ngcp++; nc--; }
			if (c <= 0) { ogcp++; oc--; }
			nmatch += c==0;
		}
		ogcp = ogcl; ngcp = ngcl;
	}
	if (nmatch < ocnt>>1) {		/* faster to just delete old cells? */
		for (c = ncbeams; c--; )
			if (cbeam[c].wants & 1<<voxi) {
				cbeam[c].wants &= ~(1<<voxi);
				netchange--;
			}
		free((char *)ogcl);
		ogcl = NULL; ocnt = 0;
	}
				/* add and delete cells in order */
	while (ocnt > 0 & ncnt > 0)
		if ((c = cellcmp(ogcp, ngcp)) > 0) {	/* new cell */
			netchange += docell(ngcp++, &nca);
			ncnt--;
		} else if (c < 0) {			/* old cell */
			netchange -= docell(ogcp++, &oca);
			ocnt--;
		} else {				/* same cell */
			ogcp++; ocnt--;
			ngcp++; ncnt--;
		}
				/* take care of list tails */
	for ( ; ncnt > 0; ncnt--)
		netchange += docell(ngcp++, &nca);
	for ( ; ocnt > 0; ocnt--)
		netchange -= docell(ogcp++, &oca);
				/* clean up */
	if (ogcl != NULL) free((char *)ogcl);
	if (ngcl != NULL) free((char *)ngcl);
	return(netchange);
}


int
beam_sync()		/* synchronize beams on server */
{
	cbeamop(DR_NEWSET, cbeam, ncbeams, &odev.v, odev.hres, odev.vres);
	return(ncbeams);
}


beam_view(vn)			/* change beam view (if advisable) */
VIEW	*vn;
{
	struct cellact	ca;
	VOXL	vlnew[8];
	int	n, comn;

	if (vn == NULL || !vn->type) {	/* clear our beam list */
		set_voxels(vlnew, 0);
		cbeamop(DR_DELSET, cbeam, ncbeams, NULL, 0, 0);
		ncbeams = 0;
		dvw.type = 0;
		return(1);
	}
					/* find our new voxels */
	n = get_voxels(vlnew, vn->vp);
	if (dvw.type && !n) {
		copystruct(vn, &dvw);		/* cancel move */
		return(0);
	}
					/* set the new voxels */
	comn = set_voxels(vlnew, n);
	if (!dvw.type)
		comn = 0;
	ca.add = 1;			/* update our beam list */
	for (ca.vi = n; ca.vi--; )
		if (comn & 1<<ca.vi)	/* change which cells we see */
			mvview(ca.vi, &dvw, vn);
		else			/* else add all new cells */
			doview(&ca, vn);
					/* inform server of new beams */
	cbeamop(DR_ADDSET, cbeam+ncbeams, xcbeams, vn, odev.hres, odev.vres);
					/* sort list to put orphans at end */
	cbeamsort(0);
					/* tell server to delete orphans */
	cbeamop(DR_DELSET, cbeam+ncbeams, xcbeams, NULL, 0, 0);
	xcbeams = 0;			/* truncate our list */
	copystruct(&dvw, vn);		/* record new view */
	return(1);
}
