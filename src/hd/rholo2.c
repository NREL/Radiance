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


packrays(rod, p)		/* pack ray origins and directions */
register float	*rod;
register PACKET	*p;
{
	short	packord[RPACKSIZ];
	float	packdc2[RPACKSIZ];
	int	iterleft = 3*p->nr;
	BYTE	rpos[2][2];
	FVECT	ro, rd, rp1;
	GCOORD	gc[2];
	double	d, dc2, md2, td2;
	int	i;
	register int	ii;

	if (!hdbcoord(gc, hdlist[p->hd], p->bi))
		error(CONSISTENCY, "bad beam index in packrays");
	td2 = (myeye.rng+FTINY)*(myeye.rng+FTINY);
	for (i = 0, md2 = 0.; i < p->nr || md2 > td2; ) {
		rpos[0][0] = frandom() * 256.;
		rpos[0][1] = frandom() * 256.;
		rpos[1][0] = frandom() * 256.;
		rpos[1][1] = frandom() * 256.;
		d = hdray(ro, rd, hdlist[p->hd], gc, rpos);
		if (myeye.rng > FTINY) {		/* check eyepoint */
			register int	nexti;

			VSUM(rp1, ro, rd, d);
			dc2 = dist2line(myeye.vpt, ro, rp1);
			dc2 /= (double)(p->nr*p->nr);
			if (i == p->nr) {		/* packet full */
				nexti = packord[i-1];
				if (!iterleft--)
					break;		/* tried enough! */
				if (dc2 >= packdc2[nexti])
					continue;	/* worse than worst */
				md2 -= packdc2[nexti];
			} else
				nexti = i++;
			md2 += packdc2[nexti] = dc2;	/* new distance */
			for (ii = i; --ii; ) {		/* insertion sort */
				if (dc2 > packdc2[packord[ii-1]])
					break;
				packord[ii] = packord[ii-1];
			}
			packord[ii] = nexti;
			ii = nexti;			/* put it here */
		} else
			ii = i++;
		if (p->offset != NULL) {
			if (!vdef(OBSTRUCTIONS))
				d *= frandom();		/* random offset */
			VSUM(ro, ro, rd, d);		/* advance ray */
			p->offset[ii] = d;
		}
		p->ra[ii].r[0][0] = rpos[0][0];
		p->ra[ii].r[0][1] = rpos[0][1];
		p->ra[ii].r[1][0] = rpos[1][0];
		p->ra[ii].r[1][1] = rpos[1][1];
		VCOPY(rod+6*ii, ro);
		VCOPY(rod+6*ii+3, rd);
	}
#ifdef DEBUG
	fprintf(stderr, "%f mean distance for target %f (%d iterations left)\n",
			sqrt(md2), myeye.rng, iterleft);
#endif
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
