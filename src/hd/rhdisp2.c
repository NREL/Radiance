/* Copyright (c) 1997 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Holodeck beam tracking
 */

#include "rholo.h"
#include "rhdisp.h"
#include "rhdriver.h"

extern int	*getviewcells();

typedef struct {
	int	hd;		/* holodeck section number */
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


cbeamadd(bl, n, v, hr, vr)	/* add beams to server list */
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
	for (i = 0; i < n; i++)
		pa[i].nr = npixels(v, hr, vr,
				hdlist[pa[i].hd=bl[i].hd],
				pa[i].bi=bl[i].bi) / 8;

	serv_request(DR_ADDSET, n*sizeof(PACKHEAD), (char *)pa);
	free((char *)pa);
}


cbeamdel(bl, n)		/* delete unwanted beam requests */
register struct beamcomp	*bl;
int	n;
{
	register PACKHEAD	*pa;
	register int	i;

	if (n <= 0)
		return;
	pa = (PACKHEAD *)malloc(n*sizeof(PACKHEAD));
	if (pa == NULL)
		error(SYSTEM, "out of memory in cbeamdel");
	for (i = 0; i < n; i++) {
		pa[i].hd = bl[i].hd;
		pa[i].bi = bl[i].bi;
		pa[i].nr = 0;		/* removes any request */
	}
	serv_request(DR_DELSET, n*sizeof(PACKHEAD), (char *)pa);
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
	copystruct(gc, gcp);
	copystruct(gc+1, &bp->gc);
	if ((bi = hdbindex(hdlist[voxel[bp->ca.vi].hd], gc)) <= 0)
		return(0);		/* should report an error? */
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
	FVECT	org, dir[4];
	FVECT	gp, cv[4], vc;
	struct beamact	bo;
	register int	i;
				/* compute cell vertices */
	hdcell(cv, hdlist[voxel[cap->vi].hd], gcp);
				/* compute cell and voxel centers */
	for (i = 0; i < 3; i++) {
		org[i] = 0.5*(cv[0][i] + cv[2][i]);
		gp[i] = voxel[cap->vi].i[i] + 0.5;
	}
	hdworld(vc, hdlist[voxel[cap->vi].hd], gp);
				/* compute voxel pyramid using vector trick */
	for (i = 0; i < 3; i++) {
		dir[0][i] = vc[i] - cv[0][i];		/* to 3 */
		dir[2][i] = vc[i] - cv[3][i];		/* to 0 */
		if (gcp->w & 1) {	/* watch vertex order! */
			dir[1][i] = vc[i] - cv[2][i];	/* to 1 */
			dir[3][i] = vc[i] - cv[1][i];	/* to 2 */
		} else {
			dir[1][i] = vc[i] - cv[1][i];	/* to 2 */
			dir[3][i] = vc[i] - cv[2][i];	/* to 1 */
		}
	}
				/* visit beams for opposite cells */
	copystruct(&bo.ca, cap);
	copystruct(&bo.gc, gcp);
	return(visit_cells(org, dir, hdlist[voxel[cap->vi].hd], dobeam, &bo));
}


int
doview(cap, vp)			/* visit cells for a given view */
struct cellact	*cap;
VIEW	*vp;
{
	FVECT	org, dir[4];
					/* compute view pyramid */
	if (vp->type == VT_PAR) goto viewerr;
	if (viewray(org, dir[0], vp, 0., 0.) < -FTINY) goto viewerr;
	if (viewray(org, dir[1], vp, 0., 1.) < -FTINY) goto viewerr;
	if (viewray(org, dir[2], vp, 1., 1.) < -FTINY) goto viewerr;
	if (viewray(org, dir[3], vp, 1., 0.) < -FTINY) goto viewerr;
					/* visit cells within pyramid */
	return(visit_cells(org, dir, hdlist[voxel[cap->vi].hd], docell, cap));
viewerr:
	error(INTERNAL, "unusable view in doview");
}


mvview(voxi, vold, vnew)	/* move view for a voxel */
int	voxi;
VIEW	*vold, *vnew;
{
	int	netchange = 0;
	int	*ocl, *ncl;
	struct cellact	ca;
	int	ocnt, ncnt;
	int	c;
	register GCOORD	*ogcp, *ngcp;
				/* get old and new cell lists */
	ocl = getviewcells(hdlist[voxel[voxi].hd], vold);
	ncl = getviewcells(hdlist[voxel[voxi].hd], vnew);
	if (ocl != NULL) {
		ocnt = *ocl; ogcp = (GCOORD *)(ocl+1);
	} else {
		ocnt = 0; ogcp = NULL;
	}
	if (ncl != NULL) {
		ncnt = *ncl; ngcp = (GCOORD *)(ncl+1);
	} else {
		ncnt = 0; ngcp = NULL;
	}
	ca.vi = voxi;		/* add and delete cells */
	while (ocnt > 0 & ncnt > 0)
		if ((c = cellcmp(ogcp, ngcp)) > 0) {
			ca.add = 1;		/* new cell */
			netchange += docell(ngcp++, &ca);
			ncnt--;
		} else if (c < 0) {
			ca.add = 0;		/* obsolete cell */
			netchange -= docell(ogcp++, &ca);
			ocnt--;
		} else {
			ogcp++; ocnt--;		/* unchanged cell */
			ngcp++; ncnt--;
		}
				/* take care of list tails */
	for (ca.add = 1; ncnt > 0; ncnt--)
		docell(ngcp++, &ca);
	for (ca.add = 0; ocnt > 0; ocnt--)
		docell(ogcp++, &ca);
				/* clean up */
	if (ocl != NULL) free((char *)ocl);
	if (ncl != NULL) free((char *)ncl);
	return(netchange);
}


beam_view(vo, vn)	/* change beam view */
VIEW	*vo, *vn;
{
	struct cellact	ca;
	VOXL	vlnew[8];
	int	n, comn;

	if (!vn->type) {		/* clear our beam list */
		set_voxels(vlnew, 0);
		cbeamdel(cbeam, ncbeams);
		ncbeams = 0;
		return;
	}
					/* find our new voxels */
	n = get_voxels(vlnew, vn->vp);
					/* set the new voxels */
	comn = set_voxels(vlnew, n);
	if (!vo->type)
		comn = 0;
	ca.add = 1;			/* update our beam list */
	for (ca.vi = n; ca.vi--; )
		if (comn & 1<<ca.vi)	/* change which cells we see */
			mvview(ca.vi, vo, vn);
		else			/* else add all new cells */
			doview(&ca, vn);
					/* inform server of new beams */
	cbeamadd(cbeam+ncbeams, xcbeams, vn, odev.hres, odev.vres);
					/* sort list to put orphans at end */
	cbeamsort(0);
					/* tell server to delete orphans */
	cbeamdel(cbeam+ncbeams, xcbeams);
	xcbeams = 0;			/* truncate our list */
}
