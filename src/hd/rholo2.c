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


packrays(rod, p)		/* pack ray origins and directions */
register float	*rod;
register PACKET	*p;
{
	static FVECT	ro, rd;
	GCOORD	gc[2];
	int	ila[2], hsh;
	double	d, sl[4];
	register int	i;

	if (!hdbcoord(gc, hdlist[p->hd], p->bi))
		error(CONSISTENCY, "bad beam index in packrays");
	ila[0] = p->hd; ila[1] = p->bi;
	hsh = ilhash(ila,2) + p->nc;
	for (i = 0; i < p->nr; i++) {
		multisamp(sl, 4, urand(hsh+i));
		p->ra[i].r[0][0] = sl[0] * 256.;
		p->ra[i].r[0][1] = sl[1] * 256.;
		p->ra[i].r[1][0] = sl[2] * 256.;
		p->ra[i].r[1][1] = sl[3] * 256.;
		d = hdray(ro, rd, hdlist[p->hd], gc, p->ra[i].r);
		if (!vdef(OBSTRUCTIONS))
			d *= frandom();			/* random offset */
		if (p->offset != NULL) {
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
	if (system(combuf)) {
		unlink(tf2);			/* clean up */
		unlink(tf1);
		error(WARNING, "error executing rad command");
		return(-1);
	}
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
