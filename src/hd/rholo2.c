/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Rtrace support routines for holodeck rendering
 */

#include "rholo.h"
#include "paths.h"
#include "random.h"


VIEWPOINT	myeye;		/* target view position */

struct gclim {
	HOLO	*hp;			/* holodeck pointer */
	GCOORD	gc;			/* grid cell */
	FVECT	egp;			/* eye grid point */
	double	erg2;			/* mean square eye grid range */
	double	gmin[2], gmax[2];	/* grid coordinate limits */
};				/* a grid coordinate range */


static
initeyelim(gcl, hp, gc)		/* initialize grid coordinate limits */
register struct gclim	*gcl;
register HOLO	*hp;
GCOORD	*gc;
{
	register FLOAT	*v;
	register int	i;

	if (hp != NULL) {
		hdgrid(gcl->egp, gcl->hp = hp, myeye.vpt);
		gcl->erg2 = 0;
		for (i = 0, v = hp->wg[0]; i < 3; i++, v += 3)
			gcl->erg2 += DOT(v,v);
		gcl->erg2 *= (1./3.) * myeye.rng*myeye.rng;
	}
	if (gc != NULL)
		copystruct(&gcl->gc, gc);
	gcl->gmin[0] = gcl->gmin[1] = FHUGE;
	gcl->gmax[0] = gcl->gmax[1] = -FHUGE;
}


static
groweyelim(gcl, gc, r0, r1)	/* grow grid limits about eye point */
register struct gclim	*gcl;
GCOORD	*gc;
double	r0, r1;
{
	FVECT	gp, ab;
	double	vlen, plen, dv0, dv1;
	double	rd2, dwall, gpos;
	int	eyeout;
	register int	i, g0, g1;

	i = gc->w>>1;
	if (gc->w&1)
		eyeout = (gp[i] = gcl->hp->grid[i]) < gcl->egp[i];
	else
		eyeout = (gp[i] = 0) > gcl->egp[i];
	gp[hdwg0[gc->w]] = gc->i[0] + r0;
	gp[hdwg1[gc->w]] = gc->i[1] + r1;
	VSUB(ab, gcl->egp, gp);
	rd2 = DOT(ab,ab);
	if (rd2 <= gcl->erg2) {
		gcl->gmin[0] = gcl->gmin[1] = -FHUGE;
		gcl->gmax[0] = gcl->gmax[1] = FHUGE;
		return;
	}
	rd2 = gcl->erg2 / rd2;
	vlen = 1. - rd2;
	plen = sqrt(rd2 * vlen);
	g0 = gcl->gc.w>>1;
	dwall = (gcl->gc.w&1 ? gcl->hp->grid[g0] : 0) - gp[g0];
	for (i = 0; i < 4; i++) {
		if (i == 2)
			plen = -plen;
		g1 = (g0+(i&1)+1)%3;
		dv0 = vlen*ab[g0] + plen*ab[g1];
		dv1 = vlen*ab[g1] - plen*ab[g0];
		if ((dv0 < 0 ^ dwall < 0 ^ eyeout) ||
				(dv0 <= FTINY && dv0 >= -FTINY)) {
			if (eyeout)
				dv1 = -dv1;
			if (dv1 > FTINY)
				gcl->gmax[i&1] = FHUGE;
			else if (dv1 < -FTINY)
				gcl->gmin[i&1] = -FHUGE;
		} else {
			gpos = gp[g1] + dv1*dwall/dv0;
			if (gpos < gcl->gmin[i&1])
				gcl->gmin[i&1] = gpos;
			if (gpos > gcl->gmax[i&1])
				gcl->gmax[i&1] = gpos;
		}
	}
}


static int
clipeyelim(rrng, gcl)		/* clip eye limits to grid cell */
register short	rrng[2][2];
register struct gclim	*gcl;
{
	int	incell = 1;
	register int	i;

	for (i = 0; i < 2; i++) {
		if (gcl->gmin[i] < gcl->gc.i[i])
			gcl->gmin[i] = gcl->gc.i[i];
		if (gcl->gmax[i] > gcl->gc.i[i]+1)
			gcl->gmax[i] = gcl->gc.i[i]+1;
		if (gcl->gmax[i] > gcl->gmin[i]) {
			rrng[i][0] = 256.*(gcl->gmin[i] - gcl->gc.i[i]) +
					(1.-FTINY);
			rrng[i][1] = 256.*(gcl->gmax[i] - gcl->gc.i[i]) +
					(1.-FTINY) - rrng[i][0];
		} else
			rrng[i][0] = rrng[i][1] = 0;
		incell &= rrng[i][1] > 0;
	}
	return(incell);
}


packrays(rod, p)		/* pack ray origins and directions */
register float	*rod;
register PACKET	*p;
{
	int	nretries = p->nr + 2;
	struct gclim	eyelim;
	short	rrng0[2][2], rrng1[2][2];
	int	useyelim;
	GCOORD	gc[2];
	FVECT	ro, rd;
	double	d;
	register int	i;

	if (!hdbcoord(gc, hdlist[p->hd], p->bi))
		error(CONSISTENCY, "bad beam index in packrays");
	if ((useyelim = myeye.rng > FTINY)) {
		initeyelim(&eyelim, hdlist[p->hd], gc);
		groweyelim(&eyelim, gc+1, 0., 0.);
		groweyelim(&eyelim, gc+1, 1., 1.);
		useyelim &= clipeyelim(rrng0, &eyelim);
	}
	for (i = 0; i < p->nr; i++) {
	retry:
		if (useyelim) {
			initeyelim(&eyelim, NULL, gc+1);
			p->ra[i].r[0][0] = (int)(frandom()*rrng0[0][1])
						+ rrng0[0][0];
			p->ra[i].r[0][1] = (int)(frandom()*rrng0[1][1])
						+ rrng0[1][0];
			groweyelim(&eyelim, gc,
					(1./256.)*(p->ra[i].r[0][0]+.5),
					(1./256.)*(p->ra[i].r[0][1]+.5));
			if (!clipeyelim(rrng1, &eyelim)) {
				useyelim &= nretries-- > 0;
				goto retry;
			}
			p->ra[i].r[1][0] = (int)(frandom()*rrng1[0][1])
						+ rrng1[0][0];
			p->ra[i].r[1][1] = (int)(frandom()*rrng1[1][1])
						+ rrng1[1][0];
		} else {
			p->ra[i].r[0][0] = frandom() * 256.;
			p->ra[i].r[0][1] = frandom() * 256.;
			p->ra[i].r[1][0] = frandom() * 256.;
			p->ra[i].r[1][1] = frandom() * 256.;
		}
		d = hdray(ro, rd, hdlist[p->hd], gc, p->ra[i].r);
		if (p->offset != NULL) {
			if (!vdef(OBSTRUCTIONS))
				d *= frandom();		/* random offset */
			VSUM(ro, ro, rd, d);		/* advance ray */
			p->offset[i] = d;
		}
		VCOPY(rod, ro);
		rod += 3;
		VCOPY(rod, rd);
		rod += 3;
	}
}


donerays(p, rvl)		/* encode finished ray computations */
register PACKET	*p;
register float	*rvl;
{
	double	d;
	register int	i;

	for (i = 0; i < p->nr; i++) {
		setcolr(p->ra[i].v, rvl[0], rvl[1], rvl[2]);
		d = rvl[3];
		if (p->offset != NULL)
			d += p->offset[i];
		p->ra[i].d = hdcode(hdlist[p->hd], d);
		rvl += 4;
	}
	p->nc += p->nr;
}


int
done_rtrace()			/* clean up and close rtrace calculation */
{
	int	status;
					/* already closed? */
	if (!nprocs)
		return;
					/* flush beam queue */
	done_packets(flush_queue());
					/* sync holodeck */
	hdsync(NULL, 1);
					/* close rtrace */
	if ((status = end_rtrace()))
		error(WARNING, "bad exit status from rtrace");
	if (vdef(REPORT)) {		/* report time */
		eputs("rtrace process closed\n");
		report(0);
	}
	return(status);			/* return status */
}


new_rtrace()			/* restart rtrace calculation */
{
	char	combuf[128];

	if (nprocs > 0)			/* already running? */
		return;
	starttime = time(NULL);		/* reset start time and counts */
	npacksdone = nraysdone = 0L;
	if (vdef(TIME))			/* reset end time */
		endtime = starttime + vflt(TIME)*3600. + .5;
	if (vdef(RIF)) {		/* rerun rad to update octree */
		sprintf(combuf, "rad -v 0 -s -w %s", vval(RIF));
		if (system(combuf))
			error(WARNING, "error running rad");
	}
	if (start_rtrace() < 1)		/* start rtrace */
		error(WARNING, "cannot restart rtrace");
	else if (vdef(REPORT)) {
		eputs("rtrace process restarted\n");
		report(0);
	}
}


getradfile()			/* run rad and get needed variables */
{
	static short	mvar[] = {OCTREE,EYESEP,-1};
	static char	tf1[] = TEMPLATE;
	char	tf2[64];
	char	combuf[256];
	char	*pippt;
	register int	i;
	register char	*cp;
					/* check if rad file specified */
	if (!vdef(RIF))
		return(0);
					/* create rad command */
	mktemp(tf1);
	sprintf(tf2, "%s.rif", tf1);
	sprintf(combuf,
		"rad -v 0 -s -e -w %s OPTFILE=%s | egrep '^[ \t]*(NOMATCH",
			vval(RIF), tf1);
	cp = combuf;
	while (*cp){
		if (*cp == '|') pippt = cp;
		cp++;
	}				/* match unset variables */
	for (i = 0; mvar[i] >= 0; i++)
		if (!vdef(mvar[i])) {
			*cp++ = '|';
			strcpy(cp, vnam(mvar[i]));
			while (*cp) cp++;
			pippt = NULL;
		}
	if (pippt != NULL)
		strcpy(pippt, "> /dev/null");	/* nothing to match */
	else
		sprintf(cp, ")[ \t]*=' > %s", tf2);
#ifdef DEBUG
	wputs(combuf); wputs("\n");
#endif
	system(combuf);				/* ignore exit code */
	if (pippt == NULL) {
		loadvars(tf2);			/* load variables */
		unlink(tf2);
	}
	rtargc += wordfile(rtargv+rtargc, tf1);	/* get rtrace options */
	unlink(tf1);			/* clean up */
	return(1);
}


report(t)			/* report progress so far */
time_t	t;
{
	static time_t	seconds2go = 1000000;

	if (t == 0L)
		t = time(NULL);
	sprintf(errmsg, "%ld packets (%ld rays) done after %.2f hours\n",
			npacksdone, nraysdone, (t-starttime)/3600.);
	eputs(errmsg);
	if (seconds2go == 1000000)
		seconds2go = vdef(REPORT) ? (long)(vflt(REPORT)*60. + .5) : 0L;
	if (seconds2go)
		reporttime = t + seconds2go;
}
