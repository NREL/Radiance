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
#include "random.h"

#ifndef MAXDIST
#define MAXDIST		42	/* maximum distance outside section */
#endif
#ifndef NVSAMPS
#define NVSAMPS		4096	/* number of ray samples per view */
#endif

#define MAXTODO		3	/* maximum sections to look at */
#define MAXDRAT		3.0	/* maximum distance ratio */

#define CBEAMBLK	1024	/* cbeam allocation block size */

static struct beamcomp {
	int	hd;		/* holodeck section number */
	int	bi;		/* beam index */
	int4	nr;		/* number of samples desired */
} *cbeam = NULL;	/* current beam list */

static int	ncbeams = 0;	/* number of sorted beams in cbeam */
static int	xcbeams = 0;	/* extra (unregistered) beams past ncbeams */
static int	maxcbeam = 0;	/* size of cbeam array */


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


int
comptodo(tdl, vw)		/* compute holodeck sections in view */
int	tdl[MAXTODO+1];
VIEW	*vw;
{
	int	n = 0;
	FVECT	gp, dv;
	double	dist2[MAXTODO+1], thisdist2;
	register int	i, j;
					/* find closest MAXTODO sections */
	for (i = 0; hdlist[i]; i++) {
		hdgrid(gp, hdlist[i], vw->vp);
		for (j = 0; j < 3; j++)
			if (gp[j] < 0.)
				dv[j] = -gp[j];
			else if (gp[j] > hdlist[i]->grid[j])
				dv[j] = gp[j] - hdlist[i]->grid[j];
			else
				dv[j] = 0.;
		thisdist2 = DOT(dv,dv);
		for (j = n; j > 0 && thisdist2 < dist2[j-1]; j--) {
			tdl[j] = tdl[j-1];
			dist2[j] = dist2[j-1];
		}
		tdl[j] = i;
		dist2[j] = thisdist2;
		if (n < MAXTODO)
			n++;
	}
					/* watch for bad move */
	if (n && dist2[0] > MAXDIST*MAXDIST) {
		error(COMMAND, "move past outer limits");
		return(0);
	}
					/* avoid big differences */
	for (j = 1; j < n; j++)
		if (dist2[j] > MAXDRAT*MAXDRAT*dist2[j-1])
			n = j;
	tdl[n] = -1;
	return(n);
}


addview(hd, vw, hres, vres)	/* add view for section */
int	hd;
VIEW	*vw;
int	hres, vres;
{
	int	sampquant;
	int	h, v, shr, svr;
	GCOORD	gc[2];
	FVECT	rorg, rdir;
				/* compute sampling grid */
	if (hres|vres && hres*vres <= NVSAMPS) {
		shr = hres; svr = vres;
		sampquant = 1;
	} else {
		shr = sqrt(NVSAMPS/viewaspect(vw)) + .5;
		svr = (NVSAMPS + shr/2)/shr;
		sampquant = (hres*vres + shr*svr/2)/(shr*svr);
	}
				/* intersect sample rays with section */
	for (v = svr; v--; )
		for (h = shr; h--; ) {
			if (viewray(rorg, rdir, vw, (v+frandom())/svr,
						(h+frandom())/shr) < -FTINY)
				continue;
			if (hdinter(gc, NULL, NULL, hdlist[hd], rorg, rdir)
						>= FHUGE)
				continue;
			cbeam[getcbeam(hd,hdbindex(hdlist[hd],gc))].nr +=
					sampquant;
		}
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
}


beam_view(vn, hr, vr)		/* add beam view (if advisable) */
VIEW	*vn;
int	hr, vr;
{
	int	todo[MAXTODO+1];
	int	n;
					/* sort our list */
	cbeamsort(1);
					/* add view to nearby sections */
	if (!(n = comptodo(todo, vn)))
		return(0);
	while (n--)
		addview(todo[n], vn, hr, vr);
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
