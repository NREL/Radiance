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
float	*rod;
register PACKET	*p;
{
	static int	nmh = 0;
	static int	*mhtab;
	FVECT	ro, rd;
	register BEAM	*bp;
	GCOORD	gc[2];
	int	ila[4], offset;
	double	d, sl[4];
	register int	i, j, k;

	if (!hdbcoord(gc, hdlist[p->hd], p->bi))
		error(CONSISTENCY, "bad beam index in packrays");
							/* uniqueness hash */
	if ((bp = hdgetbeam(hdlist[p->hd], p->bi)) != NULL) {
		if (2*bp->nrm > nmh) {
			if (nmh) free((char *)mhtab);
			nmh = 2*bp->nrm + 1;
			mhtab = (int *)malloc(nmh*sizeof(int));
			if (mhtab == NULL)
				error(SYSTEM, "out of memory in packrays");
		}
		for (k = nmh; k--; )
			mhtab[k] = -1;
		for (i = bp->nrm; i--; ) {
			ila[0] = hdbray(bp)[i].r[0][0];
			ila[1] = hdbray(bp)[i].r[0][1];
			ila[2] = hdbray(bp)[i].r[1][0];
			ila[3] = hdbray(bp)[i].r[1][1];
			for (k = ilhash(ila,4); mhtab[k%nmh] >= 0; k++)
				;
			mhtab[k%nmh] = i;
		}
	}
							/* init each ray */
	ila[0] = p->hd; ila[1] = p->bi;
	offset = ilhash(ila,2) + p->nc;
	for (i = 0; i < p->nr; i++) {
		do {					/* next unique ray */
			multisamp(sl, 4, urand(offset+i));
			p->ra[i].r[0][0] = ila[0] = sl[0] * 256.;
			p->ra[i].r[0][1] = ila[1] = sl[1] * 256.;
			p->ra[i].r[1][0] = ila[2] = sl[2] * 256.;
			p->ra[i].r[1][1] = ila[3] = sl[3] * 256.;
			if (bp == NULL)
				break;
			for (k = ilhash(ila,4); (j = mhtab[k%nmh]) >= 0; k++)
				if (hdbray(bp)[j].r[0][0] ==
							p->ra[i].r[0][0] &&
						hdbray(bp)[j].r[0][1] ==
							p->ra[i].r[0][1] &&
						hdbray(bp)[j].r[1][0] ==
							p->ra[i].r[1][0] &&
						hdbray(bp)[j].r[1][1] ==
							p->ra[i].r[1][1]) {
					offset += bp->nrm - j;
					break;
				}
		} while (j >= 0);
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
