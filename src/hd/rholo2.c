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
	double	erg[2];			/* eye range in wall grid coords */
	double	gmin[2], gmax[2];	/* grid coordinate limits */
};				/* a grid coordinate range */


static
initeyelim(gcl, hd, gc)		/* initialize grid coordinate limits */
register struct gclim	*gcl;
int	hd;
GCOORD	*gc;
{
	register FLOAT	*v;
	register int	i;

	gcl->hp = hdlist[hd];
	copystruct(&gcl->gc, gc);
	hdgrid(gcl->egp, gcl->hp, myeye.vpt);
	for (i = 0; i < 2; i++) {
		v = gcl->hp->wg[((gcl->gc.w>>1)+i+1)%3];
		gcl->erg[i] = myeye.rng * VLEN(v);
		gcl->gmin[i] = FHUGE; gcl->gmax[i] = -FHUGE;
	}
}


static
groweyelim(gcl, gp)		/* grow grid limits about eye point */
register struct gclim	*gcl;
FVECT	gp;
{
	FVECT	ab;
	double	l2, d, mult, wg;
	register int	i, g;

	VSUB(ab, gcl->egp, gp);
	l2 = DOT(ab,ab);
	if (l2 <= gcl->erg[0]*gcl->erg[1]) {
		gcl->gmin[0] = gcl->gmin[1] = -FHUGE;
		gcl->gmax[0] = gcl->gmax[1] = FHUGE;
		return;
	}
	mult = gp[g = gcl->gc.w>>1];
	if (gcl->gc.w&1)
		mult -= gcl->hp->grid[g];
	if (ab[g]*ab[g] > gcl->erg[0]*gcl->erg[1])
		mult /= -ab[g];
	else if (fabs(ab[hdwg0[gcl->gc.w]]) > fabs(ab[hdwg1[gcl->gc.w]]))
		mult = (gcl->gc.i[0] + .5 - gp[hdwg0[gcl->gc.w]]) /
				ab[hdwg0[gcl->gc.w]];
	else
		mult = (gcl->gc.i[1] + .5 - gp[hdwg1[gcl->gc.w]]) /
				ab[hdwg1[gcl->gc.w]];
	for (i = 0; i < 2; i++) {
		g = ((gcl->gc.w>>1)+i+1)%3;
		wg = gp[g] + mult*ab[g];
		d = mult*gcl->erg[i];
		if (d < 0.) d = -d;
		if (wg - d < gcl->gmin[i])
			gcl->gmin[i] = wg - d;
		if (wg + d > gcl->gmax[i])
			gcl->gmax[i] = wg + d;
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
		if ((incell &= gcl->gmax[i] > gcl->gmin[i])) {
			rrng[i][0] = 256.*(gcl->gmin[i] - gcl->gc.i[i]) +
					(1.-FTINY);
			rrng[i][1] = 256.*(gcl->gmax[i] - gcl->gc.i[i]) +
					(1.-FTINY) - rrng[i][0];
			incell &= rrng[i][1] > 0;
		}
	}
	return(incell);
}


packrays(rod, p)		/* pack ray origins and directions */
register float	*rod;
register PACKET	*p;
{
#define gp	ro
#ifdef DEBUG
	double dist2sum = 0.;
	FVECT	vt;
#endif
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
		initeyelim(&eyelim, p->hd, gc);
		gp[gc[1].w>>1] = gc[1].w&1 ?
				hdlist[p->hd]->grid[gc[1].w>>1] : 0;
		gp[hdwg0[gc[1].w]] = gc[1].i[0];
		gp[hdwg1[gc[1].w]] = gc[1].i[1];
		groweyelim(&eyelim, gp);
		gp[hdwg0[gc[1].w]]++;
		gp[hdwg1[gc[1].w]]++;
		groweyelim(&eyelim, gp);
		useyelim &= clipeyelim(rrng0, &eyelim);
	}
	for (i = 0; i < p->nr; i++) {
	retry:
		if (useyelim) {
			p->ra[i].r[0][0] = (int)(frandom()*rrng0[0][1])
						+ rrng0[0][0];
			p->ra[i].r[0][1] = (int)(frandom()*rrng0[1][1])
						+ rrng0[1][0];
			initeyelim(&eyelim, p->hd, gc+1);
			gp[gc[0].w>>1] = gc[0].w&1 ?
					hdlist[p->hd]->grid[gc[0].w>>1] : 0;
			gp[hdwg0[gc[0].w]] = gc[0].i[0] +
					(1./256.)*(p->ra[i].r[0][0]+.5);
			gp[hdwg1[gc[0].w]] = gc[0].i[1] +
					(1./256.)*(p->ra[i].r[0][1]+.5);
			groweyelim(&eyelim, gp);
			if (!clipeyelim(rrng1, &eyelim)) {
				useyelim &= nretries-- > 0;
#ifdef DEBUG
				if (!useyelim)
					error(WARNING, "exceeded retry limit in packrays");
#endif
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
#ifdef DEBUG
		VSUM(vt, ro, rd, d);
		dist2sum += dist2line(myeye.vpt, ro, vt);
#endif
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
#ifdef DEBUG
	fprintf(stderr, "RMS distance = %f\n", sqrt(dist2sum/p->nr));
#endif
#undef gp
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
